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
#include "opcodes-variable-helpers.h"
#include "vm-defines.h"

/**
 * 'Function declaration' opcode handler.
 *
 * @return completion value
 *         returned value must be freed with ecma_free_completion_value.
 */
ecma_completion_value_t
opfunc_func_decl_n (vm_frame_ctx_t *frame_ctx_p, /**< interpreter context */
                    ecma_string_t *func_name_str_p, /**< function name */
                    const ecma_length_t args_num) /**< number of arguments */
{
  ecma_collection_header_t *formal_params_collection_p;

  if (args_num != 0)
  {
    formal_params_collection_p = ecma_new_strings_collection (NULL, 0);

    for (int i = 0; i < args_num; i++)
    {
      ecma_value_t arg_name_value = 0; // FIXME

      ecma_append_to_values_collection (formal_params_collection_p, arg_name_value, false);
    }
  }
  else
  {
    formal_params_collection_p = NULL;
  }

  const bool is_configurable_bindings = frame_ctx_p->is_eval_code;

  ecma_completion_value_t ret_value = ecma_op_function_declaration (frame_ctx_p->lex_env_p,
                                                                    func_name_str_p,
                                                                    frame_ctx_p->bytecode_header_p,
                                                                    formal_params_collection_p,
                                                                    frame_ctx_p->is_strict,
                                                                    is_configurable_bindings);

  return ret_value;
} /* opfunc_func_decl_n */

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
               ecma_value_t func_value,
               uint8_t args_num,
               ecma_value_t *stack_p)
{
  ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

  JERRY_ASSERT (!frame_ctx_p->is_call_in_direct_eval_form);

  uint8_t call_flags;
  ecma_collection_header_t *arg_collection_p = ecma_new_values_collection (NULL, 0, true);

  for (int i = 0; i < args_num; i++)
  {
    ecma_append_to_values_collection (arg_collection_p, stack_p[i], true);
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
    ECMA_TRY_CATCH (this_value,
                    ecma_op_implicit_this_value (frame_ctx_p->ref_base_lex_env_p),
                    ret_value);

    ECMA_TRY_CATCH (call_ret_value,
                    ecma_op_function_call (func_obj_p,
                                           this_value,
                                           arg_collection_p),
                    ret_value);

    ECMA_FINALIZE (call_ret_value);
    ECMA_FINALIZE (this_value);

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

  return ret_value;
} /* opfunc_call_n */

/**
 * 'Variable declaration' opcode handler.
 *
 * See also: ECMA-262 v5, 10.5 - Declaration binding instantiation (block 8).
 *
 * @return completion value
 *         Returned value is simple and so need not be freed.
 *         However, ecma_free_completion_value may be called for it, but it is a no-op.
 */
ecma_completion_value_t
vm_var_decl (vm_frame_ctx_t *frame_ctx_p, /**< interpreter context */
                 ecma_value_t var_name_value) /**< variable name */
{
  ecma_string_t *var_name_str_p = ecma_get_string_from_value (var_name_value);

  if (!ecma_op_has_binding (frame_ctx_p->lex_env_p, var_name_str_p))
  {
    const bool is_configurable_bindings = frame_ctx_p->is_eval_code;

    ecma_completion_value_t completion = ecma_op_create_mutable_binding (frame_ctx_p->lex_env_p,
                                                                         var_name_str_p,
                                                                         is_configurable_bindings);

    JERRY_ASSERT (ecma_is_completion_value_empty (completion));

    /* Skipping SetMutableBinding as we have already checked that there were not
     * any binding with specified name in current lexical environment
     * and CreateMutableBinding sets the created binding's value to undefined */
    JERRY_ASSERT (ecma_is_completion_value_normal_simple_value (ecma_op_get_binding_value (frame_ctx_p->lex_env_p,
                                                                                           var_name_str_p,
                                                                                           true),
                                                                ECMA_SIMPLE_VALUE_UNDEFINED));
  }

  return ecma_make_empty_completion_value ();
} /* vm_var_decl */
