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

#include "ecma-alloc.h"
#include "ecma-builtins.h"
#include "ecma-conversion.h"
#include "ecma-function-object.h"
#include "ecma-gc.h"
#include "ecma-helpers.h"
#include "ecma-lex-env.h"
#include "ecma-try-catch-macro.h"
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
static ecma_value_t vm_stack[1024];
static ecma_value_t *vm_stack_top_p = vm_stack;

static ecma_completion_value_t
resolve_ident (ecma_value_t ident, vm_frame_ctx_t *frame_ctx_p)
{
  ecma_string_t *func_name = ecma_get_string_from_value (ident);
  ecma_object_t *ref_base_lex_env_p = ecma_op_resolve_reference_base (frame_ctx_p->lex_env_p,
                                                                      func_name);

  JERRY_ASSERT (ref_base_lex_env_p != NULL);

  frame_ctx_p->ref_base_lex_env_p = ref_base_lex_env_p;
  return ecma_op_get_value_lex_env_base (ref_base_lex_env_p,
                                         func_name,
                                         frame_ctx_p->is_strict);
}

static ecma_value_t
vm_op_return (cbc_opcode_t opcode, ecma_value_t left_value)
{
  JERRY_ASSERT (opcode == CBC_RETURN || opcode == CBC_RETURN_WITH_UNDEFINED);
  if (opcode == CBC_RETURN)
  {
    return left_value;
  }
  return ecma_make_simple_value (ECMA_SIMPLE_VALUE_UNDEFINED);
}

/**
 * Initialize interpreter.
 */
void
vm_init (const cbc_compiled_code_t *program_p, /**< pointer to byte-code data */
         bool dump_mem_stats) /** dump per-instruction memory usage change statistics */
{
  JERRY_ASSERT (!dump_mem_stats);

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
    ecma_value_t *literal_p = (ecma_value_t *) (((uint8_t *) __program) + sizeof (cbc_compiled_code_t));
    ecma_value_t *literal_end_p = literal_p + __program->literal_end;

    while (literal_p && literal_p < literal_end_p)
    {
      ecma_free_value (*(literal_p++), true);
    }

    mem_heap_free_block (__program);
  }
  __program = NULL;
} /* vm_finalize */


#define CBC_OPCODE(arg1, arg2, arg3, arg4) arg4,

uint32_t vm_decode_table[] =
{
  CBC_OPCODE_LIST
};

uint32_t vm_ext_decode_table[] =
{
  CBC_EXT_OPCODE_LIST
};

#undef CBC_OPCODE

static uint32_t
vm_decode_cbc(uint8_t opcode, uint8_t ext_opcode)
{
  if(opcode != CBC_EXT_OPCODE) {
    return vm_decode_table[opcode];
  } else {
    return vm_ext_decode_table[ext_opcode];
  }
  JERRY_UNREACHABLE ();
}

/**
 * Run global code
 */
jerry_completion_code_t
vm_run_global (void)
{
  jerry_completion_code_t ret_code;

  JERRY_ASSERT (__program != NULL);

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

  if (ecma_is_completion_value_return (completion))
  {
    JERRY_ASSERT (ecma_is_value_undefined (ecma_get_completion_value_value (completion)));

    ret_code = JERRY_COMPLETION_CODE_OK;
  }
  else
  {
    JERRY_ASSERT (ecma_is_completion_value_throw (completion));

    ret_code = JERRY_COMPLETION_CODE_UNHANDLED_EXCEPTION;
  }

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
  //ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

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

  /* Prepare */
  if (!(bytecode_header_p->status_flags & CBC_CODE_FLAGS_FULL_LITERAL_ENCODING))
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
  byte_code_p = byte_code_start_p = (uint8_t *) (literal_start_p + bytecode_header_p->literal_end);

  /* start execution */
  while (!leave)
  {
    size_t cbc_offset;
    size_t branch_offset;
    uint32_t decoded_opcode;

    cbc_offset = (size_t) (byte_code_p - byte_code_start_p);
    opcode = (cbc_opcode_t) *(byte_code_p++);
    ext_opcode = CBC_EXT_NOP;

    if(opcode == CBC_EXT_OPCODE) {
      ext_opcode = (cbc_ext_opcode_t) *(byte_code_p++);
      flags = cbc_ext_flags[ext_opcode];
    } else {
      flags = cbc_flags[opcode];
    }

    branch_offset = 0;

    decoded_opcode = vm_decode_cbc (opcode, ext_opcode);
    uint16_t left_literal_index = 0;
    uint16_t right_literal_index = 0;
    ecma_value_t left_value = 0;
    ecma_value_t right_value = 0;
    ecma_value_t result;
    uint8_t byte_arg = 0u;

    switch (VM_OC_BRANCH_OPERAND (decoded_opcode))
    {
      case VM_OC_BRANCH_3:
      {
        branch_offset |= *(byte_code_p++) << 16;
      }
      case VM_OC_BRANCH_2:
      {
        branch_offset |= *(byte_code_p++) << 8;
      }
      case VM_OC_BRANCH_1:
      {
        branch_offset |= *(byte_code_p++);
        break;
      }
      case VM_OC_BRANCH_NONE:
      {
        break;
      }
      default:
      {
        JERRY_UNREACHABLE ();
      }
    }

    switch (VM_OC_LEFT_OPERAND (decoded_opcode))
    {
      case VM_OC_OP_STACK:
      {
        JERRY_ASSERT (vm_stack_top_p > vm_stack);
        left_value = *(--vm_stack_top_p);
        break;
      }
      case VM_OC_OP_BYTE:
      {
        byte_arg = *(byte_code_p++);
        break;
      }
      case VM_OC_OP_LITERAL:
      {
        left_literal_index = *(byte_code_p++);
        if (left_literal_index >= encoding_limit)
        {
          left_literal_index = (uint16_t) (((left_literal_index << 8) | *(byte_code_p++)) - encoding_delta);
        }
        if (left_literal_index < bytecode_header_p->argument_end)
        {
          JERRY_ASSERT (false);
        }
        else if (left_literal_index < bytecode_header_p->register_end)
        {
          JERRY_ASSERT (false);
        }
        else if (left_literal_index < bytecode_header_p->ident_end)
        {
          ecma_completion_value_t func_comp_value = resolve_ident (literal_start_p[left_literal_index],
                                                                   frame_ctx_p);
          if (ecma_is_completion_value_throw (func_comp_value))
          {
            return func_comp_value;
          }
          left_value = ecma_get_completion_value_value (func_comp_value);
        }
        else
        {
          left_value = literal_start_p[left_literal_index];
        }
        break;
      }
      case VM_OC_OP_NONE:
      {
        break;
      }
      default:
      {
        JERRY_UNREACHABLE ();
      }
    }

    switch (VM_OC_RIGHT_OPERAND (decoded_opcode))
    {
      case VM_OC_OP_STACK:
      {
        JERRY_ASSERT (vm_stack_top_p > vm_stack);
        right_value = *(--vm_stack_top_p);
        break;
      }
      case VM_OC_OP_LITERAL:
      {
        right_literal_index = *(byte_code_p++);
        if (right_literal_index >= encoding_limit)
        {
          right_literal_index = (uint16_t) (((right_literal_index << 8) | *(byte_code_p++)) - encoding_delta);
        }
        if (right_literal_index < bytecode_header_p->argument_end)
        {
          JERRY_ASSERT (false);
        }
        else if (right_literal_index < bytecode_header_p->register_end)
        {
          JERRY_ASSERT (false);
        }
        else if (right_literal_index < bytecode_header_p->ident_end)
        {
          ecma_completion_value_t func_comp_value = resolve_ident (literal_start_p[right_literal_index],
                                                                   frame_ctx_p);
          if (ecma_is_completion_value_throw (func_comp_value))
          {
            return func_comp_value;
          }
          right_value = ecma_get_completion_value_value (func_comp_value);
        }
        else
        {
          right_value = literal_start_p[right_literal_index];
        }
        break;
      }
      case VM_OC_OP_NONE:
      {
        break;
      }
      default:
      {
        JERRY_UNREACHABLE ();
      }
    }

    switch (VM_OC_GROUP (decoded_opcode))
    {
      case VM_OC_GROUP_ASSIGN:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ret_value = opfunc_assignment (frame_ctx_p, literal_start_p[left_literal_index], right_value);

        ecma_free_value (right_value, true);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_PLUS:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_unary_plus (left_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_MINUS:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_unary_minus (left_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_ADD:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_addition (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_SUB:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_substraction (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_MUL:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_multiplication (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_DIV:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_division (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_MOD:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_remainder (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }

      case VM_OC_GROUP_POP:
      {
        JERRY_ASSERT (vm_stack_top_p > vm_stack);
        ecma_free_value (*(--vm_stack_top_p), true);
        break;
      }
      case VM_OC_GROUP_RET:
      {
        result = vm_op_return (opcode, left_value);
        break;
      }
      case  VM_OC_GROUP_PUSH:
      {
        result = ecma_copy_value (left_value, true);
        break;
      }
      case  VM_OC_GROUP_PUSH_TWO:
      {
        *(vm_stack_top_p++) = ecma_copy_value (left_value, true);
        *(vm_stack_top_p++) = ecma_copy_value (right_value, true);
        break;
      }
      case VM_OC_GROUP_CALL:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value,
                        opfunc_call_n (frame_ctx_p, right_value, byte_arg, &vm_stack_top_p),
                        ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        ecma_free_value (right_value, true);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_EQUAL:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_equal_value (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_NOT_EQUAL:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_not_equal_value (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_STRICT_EQUAL:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_equal_value_type (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_STRICT_NOT_EQUAL:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_not_equal_value_type (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_BIT_OR:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_b_or (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_BIT_XOR:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_b_xor (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_BIT_AND:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_b_and (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_LEFT_SHIFT:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_b_shift_left (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_RIGHT_SHIFT:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_b_shift_right (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_UNS_RIGHT_SHIFT:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_b_shift_uright (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_BIT_NOT:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_b_not (left_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_LESS:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_less_than (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_GREATER:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_greater_than (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_LESS_EQUAL:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_less_or_equal_than (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_GREATER_EQUAL:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_greater_or_equal_than (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_IN:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_in (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_INSTANCEOF:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_instanceof (left_value, right_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_NONE:
      {
        switch (opcode)
        {
          case CBC_PUSH_TRUE:
          {
            result = ecma_make_simple_value (ECMA_SIMPLE_VALUE_TRUE);
            break;
          }
          case CBC_PUSH_FALSE:
          {
            result = ecma_make_simple_value (ECMA_SIMPLE_VALUE_FALSE);
            break;
          }
          case CBC_PUSH_UNDEFINED:
          {
            result = ecma_make_simple_value (ECMA_SIMPLE_VALUE_UNDEFINED);
            break;
          }
          case CBC_DEFINE_VARS:
          {
            uint16_t literal_idx = *(byte_code_p++);

            if (literal_idx >= encoding_limit)
            {
              literal_idx = ((literal_idx << 8) | *(byte_code_p++)) - encoding_delta;
            }

            opfunc_var_decl (frame_ctx_p, literal_start_p[literal_idx]);

            break;
          }
          default:
          {
            JERRY_UNREACHABLE ();
          }
        }

        break;
      }
      case VM_OC_GROUP_JUMP:
      {
        byte_code_p = byte_code_start_p + (cbc_offset + ((CBC_BRANCH_IS_FORWARD (flags)) ? 1 : -1) * branch_offset);
        break;
      }
      case VM_OC_GROUP_BRANCH_IF:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, ecma_op_to_boolean (left_value), ret_value);

          if (value == ecma_make_simple_value ((VM_OC_FLAGS (decoded_opcode) & VM_OC_FLAGS_BRANCH_IF_TRUE) ? ECMA_SIMPLE_VALUE_TRUE : ECMA_SIMPLE_VALUE_FALSE))
          {
            byte_code_p = byte_code_start_p + (cbc_offset + ((CBC_BRANCH_IS_FORWARD (flags)) ? 1 : -1) * branch_offset);
            if (VM_OC_FLAGS (decoded_opcode) & VM_OC_FLAGS_BRANCH_LOGICAL)
            {
              left_value = ecma_make_simple_value (ECMA_SIMPLE_VALUE_UNDEFINED);
              ++vm_stack_top_p;
            }
          }

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      case VM_OC_GROUP_BRANCH_STRICT_EQUAL:
      {
        JERRY_ASSERT (vm_stack_top_p > vm_stack);

        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_equal_value_type (left_value, vm_stack_top_p[-1]), ret_value);

        if (value == ecma_make_simple_value (ECMA_SIMPLE_VALUE_TRUE))
        {
          byte_code_p = byte_code_start_p + cbc_offset + branch_offset;
          ecma_free_value (*--vm_stack_top_p, true);
        }

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

        break;
      }
      default:
      {
        JERRY_UNREACHABLE ();
      }
    }

    if (VM_OC_LEFT_OPERAND (decoded_opcode) == VM_OC_OP_STACK)
    {
      ecma_free_value (left_value, true);
    }

    if (VM_OC_RIGHT_OPERAND (decoded_opcode) == VM_OC_OP_STACK)
    {
      ecma_free_value (right_value, true);
    }

    switch (VM_OC_POST_PROCESS (decoded_opcode))
    {
      case VM_OC_POST_NONE:
      {
        break;
      }
      case VM_OC_POST_PUSH_RESULT:
      {
        *(vm_stack_top_p++) = result;
        break;
      }
      default:
      {
        JERRY_UNREACHABLE ();
      }
    }

    if(VM_OC_GROUP(decoded_opcode) == VM_OC_GROUP_RET) {
      leave = true;
    }

  }

  if (ecma_is_completion_value_empty (ret_value))
  {
    return ecma_make_completion_value (ECMA_COMPLETION_TYPE_RETURN,
                                       ecma_make_simple_value (ECMA_SIMPLE_VALUE_UNDEFINED));
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
  //return __program->status_flags & CBC_CODE_FLAGS_STRICT_MODE;
}

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
