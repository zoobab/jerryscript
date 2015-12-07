/* Copyright 2015 Samsung Electronics Co., Ltd.
 * Copyright 2015 University of Szeged.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ecma-builtins.h"
#include "ecma-exceptions.h"
#include "ecma-function-object.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-lex-env.h"
#include "ecma-try-catch-macro.h"
#include "opcodes.h"
#include "vm-defines.h"

/**
 * Get 'this' argument value and call flags mask for function call
 *
 * See also:
 *          ECMA-262 v5, 11.2.3, steps 6-7

 * @return ecma-value
 *         Returned value must be freed with ecma_free_value
 */
static ecma_value_t
vm_get_call_this_arg (vm_frame_ctx_t *int_data_p, /**< interpreter context */
                      ecma_string_t *var_name_string)
{
  /* 6.b.i */
  ecma_object_t *ref_base_lex_env_p = ecma_op_resolve_reference_base (int_data_p->lex_env_p,
                                                                      var_name_string);
  JERRY_ASSERT (ref_base_lex_env_p != NULL);
  ecma_completion_value_t get_this_completion_value = ecma_op_implicit_this_value (ref_base_lex_env_p);

  ecma_value_t this_value = ecma_get_completion_value_value (get_this_completion_value);

  return this_value;
} /* vm_helper_call_get_call_flags_and_this_arg */

/**
 * 'Function call' opcode handler.
 *
 * See also: ECMA-262 v5, 11.2.3
 *
 * @return completion value
 *         Returned value must be freed with ecma_free_completion_value.
 */
ecma_completion_value_t
opfunc_call_n (vm_frame_ctx_t *frame_ctx_p, /**< interpreter context */
               uint16_t func_name_lit_idx,
               uint32_t args_num,
               ecma_value_t **stack_p)
{
  ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();
  ecma_string_t *func_name = ecma_get_string_from_value (frame_ctx_p->literal_start_p[func_name_lit_idx]);

  ecma_object_t *ref_base_lex_env_p = ecma_op_resolve_reference_base (frame_ctx_p->lex_env_p,
                                                                      func_name);

  JERRY_ASSERT (ref_base_lex_env_p != NULL);

  ECMA_TRY_CATCH (func_value,
                  ecma_op_get_value_lex_env_base (ref_base_lex_env_p,
                                                  func_name,
                                                  frame_ctx_p->is_strict),
                  ret_value);

  frame_ctx_p->instr_pos++;

  JERRY_ASSERT (!frame_ctx_p->is_call_in_direct_eval_form);

  uint8_t call_flags;
  ecma_value_t this_value = vm_get_call_this_arg (frame_ctx_p, func_name);
  ecma_collection_header_t *arg_collection_p = ecma_new_values_collection (NULL, 0, true);

  ecma_value_t *sp = *stack_p;

  for (int i = 0; i < args_num; i++)
  {
    if (*(--sp))
    {
      (*stack_p)--;
      ecma_append_to_values_collection (arg_collection_p, *sp, true);
    }
    else
    {
      ecma_append_to_values_collection (arg_collection_p,
                                        ecma_make_simple_value (ECMA_SIMPLE_VALUE_UNDEFINED),
                                        true);
    }
  }

  if (!ecma_op_is_callable (func_value))
  {
    ret_value = ecma_make_throw_obj_completion_value (ecma_new_standard_error (ECMA_ERROR_TYPE));
  }
  else
  {
    if (call_flags & OPCODE_CALL_FLAGS_DIRECT_CALL_TO_EVAL_FORM)
    {
      frame_ctx_p->is_call_in_direct_eval_form = true;
    }

    ecma_object_t *func_obj_p = ecma_get_object_from_value (func_value);

    ECMA_TRY_CATCH (call_ret_value,
                    ecma_op_function_call (func_obj_p,
                                           this_value,
                                           arg_collection_p),
                    ret_value);

//      ret_value = set_variable_value (frame_ctx_p, lit_oc,
//                                      lhs_var_idx,
//                                      call_ret_value);

    ECMA_FINALIZE (call_ret_value);

    if (call_flags & OPCODE_CALL_FLAGS_DIRECT_CALL_TO_EVAL_FORM)
    {
      JERRY_ASSERT (frame_ctx_p->is_call_in_direct_eval_form);
      frame_ctx_p->is_call_in_direct_eval_form = false;
    }
    else
    {
      JERRY_ASSERT (!frame_ctx_p->is_call_in_direct_eval_form);
    }
  }

  ecma_free_values_collection (arg_collection_p, true);
  ecma_free_value (this_value, true);

  ECMA_FINALIZE (func_value);

  return ret_value;
} /* opfunc_call_n */
