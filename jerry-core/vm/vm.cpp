/* Copyright 2015 University of Szeged.
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

#include "ecma-alloc.h"
#include "ecma-builtins.h"
#include "ecma-function-object.h"
#include "ecma-gc.h"
#include "ecma-helpers.h"
#include "ecma-lex-env.h"
#include "opcodes.h"
#include "vm.h"

#include "common.h"

/**
 * Top (current) interpreter context
 */
vm_frame_ctx_t *vm_top_context_p = NULL;

/**
 * Program bytecode pointer
 */
const cbc_compiled_code_t *__program = NULL;

/**
 * Add two ecma_value_t
 *
 * @return ecma_value_t
 */
ecma_value_t
vm_op_add (ecma_value_t lhs,
           ecma_value_t rhs)
{
  if (ecma_get_value_type_field (lhs) != ecma_get_value_type_field (rhs))
  {
    return ecma_make_simple_value (ECMA_SIMPLE_VALUE_UNDEFINED);
  }

  if (ecma_is_value_undefined (lhs))
  {
    return lhs;
  }
  else if (ecma_is_value_null (lhs))
  {
    return lhs;
  }
  else if (ecma_is_value_boolean (lhs))
  {

    return ecma_make_simple_value (ECMA_SIMPLE_VALUE_UNDEFINED);
  }
  else if (ecma_is_value_number (lhs))
  {
    ecma_number_t *lnum_p = ecma_get_number_from_value (lhs);
    ecma_number_t *rnum_p = ecma_get_number_from_value (rhs);
    ecma_number_t *retval = ecma_alloc_number ();
    *retval = *lnum_p + *rnum_p;
    return ecma_make_number_value (retval);
  }
  else if (ecma_is_value_string (lhs))
  {

    return ecma_make_simple_value (ECMA_SIMPLE_VALUE_UNDEFINED);
  }
  else if (ecma_is_value_object (lhs))
  {

    return ecma_make_simple_value (ECMA_SIMPLE_VALUE_UNDEFINED);
  }
  else
  {
    /* Impossible type of conversion from ecma_value to api_value */
    JERRY_UNREACHABLE ();
  }

  JERRY_UNREACHABLE ();
} /* vm_op_add */

/**
 * Initialize interpreter.
 */
void
vm_init (const cbc_compiled_code_t *program_p, /**< pointer to byte-code data */
         bool dump_mem_stats) /** dump per-instruction memory usage change statistics */
{
#ifdef MEM_STATS
  interp_mem_stats_enabled = dump_mem_stats;
#else /* MEM_STATS */
  JERRY_ASSERT (!dump_mem_stats);
#endif /* !MEM_STATS */

  JERRY_ASSERT (__program == NULL);

  __program = program_p;
} /* vm_init */

/**
 * Cleanup interpreter
 */
void
vm_finalize (void)
{
  if (__program)
  {
    mem_heap_free_block (__program);
  }
  __program = NULL;
} /* vm_finalize */

/**
 * Run global code
 */
jerry_completion_code_t
vm_run_global (void)
{
  JERRY_ASSERT (__program != NULL);

#ifdef MEM_STATS
  interp_mem_stats_print_legend ();
#endif /* MEM_STATS */

  bool is_strict = false;
  vm_instr_counter_t start_pos = 0;

  opcode_scope_code_flags_t scope_flags = vm_get_scope_flags (__program);

  if (scope_flags & OPCODE_SCOPE_CODE_FLAGS_STRICT)
  {
    is_strict = true;
  }

  ecma_object_t *glob_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_GLOBAL);
  ecma_object_t *lex_env_p = ecma_get_global_environment ();

  ecma_completion_value_t completion = vm_run_from_pos (__program,
                                                        start_pos,
                                                        ecma_make_object_value (glob_obj_p),
                                                        lex_env_p,
                                                        is_strict,
                                                        false);

  jerry_completion_code_t ret_code = JERRY_COMPLETION_CODE_OK;

//  if (ecma_is_completion_value_return (completion))
//  {
//    JERRY_ASSERT (ecma_is_value_undefined (ecma_get_completion_value_value (completion)));

//    ret_code = JERRY_COMPLETION_CODE_OK;
//  }
//  else
//  {
//    JERRY_ASSERT (ecma_is_completion_value_throw (completion));

//    ret_code = JERRY_COMPLETION_CODE_UNHANDLED_EXCEPTION;
//  }

  ecma_free_completion_value (completion);

  ecma_deref_object (glob_obj_p);
  ecma_deref_object (lex_env_p);

  return ret_code;
} /* vm_run_global */

/**
 * Run specified eval-mode bytecode
 *
 * @return completion value
 */
ecma_completion_value_t
vm_run_eval (const cbc_compiled_code_t *bytecode_data_p, /**< byte-code data header */
             bool is_direct) /**< is eval called in direct mode? */
{
  vm_instr_counter_t first_instr_index = 0u;
  opcode_scope_code_flags_t scope_flags = vm_get_scope_flags (bytecode_data_p);
  bool is_strict = ((scope_flags & OPCODE_SCOPE_CODE_FLAGS_STRICT) != 0);

  ecma_value_t this_binding;
  ecma_object_t *lex_env_p;

  /* ECMA-262 v5, 10.4.2 */
  if (is_direct)
  {
    this_binding = vm_get_this_binding ();
    lex_env_p = vm_get_lex_env ();
  }
  else
  {
    this_binding = ecma_make_object_value (ecma_builtin_get (ECMA_BUILTIN_ID_GLOBAL));
    lex_env_p = ecma_get_global_environment ();
  }

  if (is_strict)
  {
    ecma_object_t *strict_lex_env_p = ecma_create_decl_lex_env (lex_env_p);
    ecma_deref_object (lex_env_p);

    lex_env_p = strict_lex_env_p;
  }

  ecma_completion_value_t completion = vm_run_from_pos (bytecode_data_p,
                                                        first_instr_index,
                                                        this_binding,
                                                        lex_env_p,
                                                        is_strict,
                                                        true);

  if (ecma_is_completion_value_return (completion))
  {
    completion = ecma_make_normal_completion_value (ecma_get_completion_value_value (completion));
  }
  else
  {
    JERRY_ASSERT (ecma_is_completion_value_throw (completion));
  }

  ecma_deref_object (lex_env_p);
  ecma_free_value (this_binding, true);

  return completion;
} /* vm_run_eval */

ecma_completion_value_t
vm_loop (vm_frame_ctx_t *frame_ctx_p)
{
  // FIXME: Implement this
  ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

  const cbc_compiled_code_t *bytecode_header_p = frame_ctx_p->bytecode_header_p;
  cbc_opcode_t opcode;
  cbc_ext_opcode_t ext_opcode;
  uint8_t flags;
  uint8_t *byte_code_start_p;
  uint8_t *byte_code_p;
  uint16_t encoding_limit;
  uint16_t encoding_delta;
  ecma_value_t *literal_start_p;
  bool leave = false;

  ecma_value_t stack[1024];
  ecma_value_t *stack_p = stack;

  if (bytecode_header_p->status_flags & CBC_CODE_FLAGS_FULL_LITERAL_ENCODING == cbc_literal_encoding_small)
  {
    encoding_limit = 255;
    encoding_delta = 0xfe01;
  }
  else
  {
    encoding_limit = 128;
    encoding_delta = 0x8000;
  }

  literal_start_p = (ecma_value_t *) (((uint8_t *) bytecode_header_p) + sizeof (cbc_compiled_code_t));
  byte_code_start_p = (uint8_t *) (literal_start_p + bytecode_header_p->literal_end);
  byte_code_p = byte_code_start_p;

  frame_ctx_p->literal_start_p = literal_start_p;

  while (!leave)
  {
    size_t cbc_offset;

    opcode = (cbc_opcode_t) *byte_code_p;
    ext_opcode = CBC_EXT_NOP;
    cbc_offset = (size_t) (byte_code_p - byte_code_start_p);

    if (opcode != CBC_EXT_OPCODE)
    {
      flags = cbc_flags[opcode];
      byte_code_p++;

      switch(opcode)
      {
        case CBC_ADD_TWO_LITERALS:
        {
          ecma_value_t left_value;
          ecma_value_t right_value;
          ecma_value_t result;

          uint16_t left_literal_index = *(byte_code_p++);

          if (left_literal_index >= encoding_limit)
          {
            left_literal_index = ((left_literal_index << 8) | *(byte_code_p++)) - encoding_delta;
          }

          uint16_t right_literal_index = *(byte_code_p++);

          if (right_literal_index >= encoding_limit)
          {
            right_literal_index = ((right_literal_index << 8) | *(byte_code_p++)) - encoding_delta;
          }

          if (left_literal_index < bytecode_header_p->argument_end)
          {
          }
          else if (left_literal_index < bytecode_header_p->register_end)
          {
          }
          else if (left_literal_index < bytecode_header_p->ident_end)
          {
          }
          else
          {
            left_value = literal_start_p[left_literal_index];
          }

          if (right_value < bytecode_header_p->argument_end)
          {
          }
          else if (right_value < bytecode_header_p->register_end)
          {
          }
          else if (right_value < bytecode_header_p->ident_end)
          {
          }
          else
          {
            right_value = literal_start_p[right_literal_index];
          }

          result = vm_op_add(left_value, right_value);

          *(stack_p++) = result;
          break;
        }
        case CBC_RETURN_WITH_UNDEFINED:
        {
          leave = true;
          break;
        }
        case CBC_POP_BLOCK:
        {
          break;
        }
        case CBC_CALL_IDENT_PUSH_RESULT:
        {

          uint8_t byte_arg = 0u;

          if (flags & CBC_HAS_BYTE_ARG)
          {
            byte_arg = *(byte_code_p++);
          }

          uint16_t literal_index = *(byte_code_p++);
          if (literal_index >= encoding_limit)
          {
            literal_index = ((literal_index << 8) | *(byte_code_p++)) - encoding_delta;
          }

          opfunc_call_n (frame_ctx_p, literal_index, byte_arg, stack_p);
//          ecma_string_t *func_name = ecma_get_string_from_value (literal_start_p[literal_index]);
//          ecma_object_t *ref_base_lex_env_p = ecma_op_resolve_reference_base (frame_ctx_p->lex_env_p,
//                                                                              func_name);

//          ecma_completion_value_t func_value = ecma_op_get_value_lex_env_base (ref_base_lex_env_p,
//                                                                               func_name,
//                                                                               vm_is_strict_mode ());

//          ecma_object_t *func_obj_p = ecma_get_object_from_value (func_value);

//          ecma_collection_header_t *arg_collection_p = ecma_new_values_collection (NULL, 0, true);
//          ecma_append_to_values_collection (arg_collection_p,
//                                            ecma_make_string_value (func_name),
//                                            true);

//          ecma_completion_value_t get_this_completion_value = ecma_op_implicit_this_value (ref_base_lex_env_p);
//          ecma_value_t this_value = ecma_get_completion_value_value (get_this_completion_value);

//          ecma_op_function_call (func_obj_p,
//                                 this_value,
//                                 arg_collection_p);

//          printf (" byte_arg:%d", byte_arg);

          break;
        }
        default:
        {
          JERRY_UNIMPLEMENTED ("Unimlemented opcode!");
          return;
        }
      }
    }
    else
    {
      ext_opcode = byte_code_p[1];
      flags = cbc_ext_flags[ext_opcode];
      byte_code_p += 2;

      switch (ext_opcode)
      {
        case CBC_EXT_ASSIGN_IDENT_LITERAL_BLOCK:
        {


          while (flags & (CBC_HAS_LITERAL_ARG | CBC_HAS_LITERAL_ARG2))
          {
            uint16_t literal_index = *byte_code_p;

            byte_code_p++;
            if (literal_index >= encoding_limit)
            {
              byte_code_p++;
            }

            if (flags & CBC_HAS_LITERAL_ARG)
            {
              flags = (uint8_t) (flags - CBC_HAS_LITERAL_ARG);
            }
            else
            {
              break;
            }
          }
          break;
        }
        default:
        {

          return;
        }
      }
    }
  }

  return ret_value;
}

/**
 * Run the code, starting from specified instruction position
 */
ecma_completion_value_t
vm_run_from_pos (const cbc_compiled_code_t *bytecode_header_p, /**< byte-code data header */
                 vm_instr_counter_t start_pos, /**< position of starting instruction */
                 ecma_value_t this_binding_value, /**< value of 'ThisBinding' */
                 ecma_object_t *lex_env_p, /**< lexical environment to use */
                 bool is_strict, /**< is the code is strict mode code (ECMA-262 v5, 10.1.1) */
                 bool is_eval_code) /**< is the code is eval code (ECMA-262 v5, 10.1) */
{
  ecma_completion_value_t completion = ecma_make_empty_completion_value ();

  vm_frame_ctx_t frame_ctx;
  frame_ctx.bytecode_header_p = bytecode_header_p;
  frame_ctx.instr_pos = (vm_instr_counter_t) (start_pos + 1);
  frame_ctx.lex_env_p = lex_env_p;
  frame_ctx.is_eval_code = is_eval_code;
  frame_ctx.is_call_in_direct_eval_form = false;

//  vm_stack_add_frame (&frame_ctx.stack_frame, regs, regs_num, local_var_regs_num, arg_regs_num, arg_collection_p);
//  vm_stack_frame_set_reg_value (&frame_ctx.stack_frame,
//                                VM_REG_SPECIAL_THIS_BINDING,
//                                ecma_copy_value (this_binding_value, false));

  vm_frame_ctx_t *prev_context_p = vm_top_context_p;
  vm_top_context_p = &frame_ctx;

  completion = vm_loop (&frame_ctx);

//  JERRY_ASSERT (ecma_is_completion_value_throw (completion)
//                || ecma_is_completion_value_return (completion));

  vm_top_context_p = prev_context_p;

//  vm_stack_free_frame (&frame_ctx.stack_frame);

  return completion;
}

/**
 * Get scope code flags from instruction at specified position
 *
 * @return mask of scope code flags
 */
opcode_scope_code_flags_t
vm_get_scope_flags (const cbc_compiled_code_t *bytecode_header_p) /**< byte-code data */
{
  return bytecode_header_p->status_flags;
} /* vm_get_scope_flags */

/**
 * Check whether currently executed code is strict mode code
 *
 * @return true - current code is executed in strict mode,
 *         false - otherwise.
 */
bool
vm_is_strict_mode (void)
{
  JERRY_ASSERT (vm_top_context_p != NULL);

  return vm_top_context_p->bytecode_header_p->status_flags & CBC_CODE_FLAGS_STRICT_MODE;
} /* vm_is_strict_mode */

/**
 * Check whether currently performed call (on top of call-stack) is performed in form,
 * meeting conditions of 'Direct Call to Eval' (see also: ECMA-262 v5, 15.1.2.1.1)
 *
 * Warning:
 *         the function should only be called from implementation
 *         of built-in 'eval' routine of Global object
 *
 * @return true - currently performed call is performed through 'eval' identifier,
 *                without 'this' argument,
 *         false - otherwise.
 */
bool
vm_is_direct_eval_form_call (void)
{
  if (vm_top_context_p != NULL)
  {
    return vm_top_context_p->is_call_in_direct_eval_form;
  }
  else
  {
    /*
     * There is no any interpreter context, so call is performed not from a script.
     * This implies that the call is indirect.
     */
    return false;
  }
} /* vm_is_direct_eval_form_call */

ecma_value_t vm_get_this_binding (void)
{
// FIXME: Implement this
}

/**
 * Get top lexical environment (variable environment) of current execution context
 *
 * @return lexical environment
 */
ecma_object_t*
vm_get_lex_env (void)
{
  JERRY_ASSERT (vm_top_context_p != NULL);

  ecma_ref_object (vm_top_context_p->lex_env_p);

  return vm_top_context_p->lex_env_p;
} /* vm_get_lex_env */
