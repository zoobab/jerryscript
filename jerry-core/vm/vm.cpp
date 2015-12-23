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
#include "ecma-array-object.h"
#include "ecma-builtins.h"
#include "ecma-conversion.h"
#include "ecma-function-object.h"
#include "ecma-gc.h"
#include "ecma-helpers.h"
#include "ecma-lex-env.h"
#include "ecma-objects.h"
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

/**
 * Get the value of object[property].
 *
 * @return completion value
 */
static ecma_completion_value_t
vm_op_get_value (ecma_value_t object, /**< base object */
                 ecma_value_t property, /**< property name */
                 bool is_strict) /**< strict mode */
{
  ecma_completion_value_t completion_value = ecma_make_empty_completion_value ();
  ecma_object_t *object_p;
  ecma_string_t *property_p;

  ECMA_TRY_CATCH (obj_val,
                  ecma_op_to_object (object),
                  completion_value);

  ECMA_TRY_CATCH (property_val,
                  ecma_op_to_string (property),
                  completion_value);

  object_p = ecma_get_object_from_value (obj_val);
  property_p = ecma_get_string_from_value (property_val);

  if (ecma_is_lexical_environment (object_p))
  {
    completion_value = ecma_op_get_value_lex_env_base (object_p,
                                                       property_p,
                                                       is_strict);
  }
  else
  {
    completion_value = ecma_op_object_get (object_p,
                                           property_p);
  }

  ECMA_FINALIZE (property_val);
  ECMA_FINALIZE (obj_val);

  return completion_value;
} /* vm_op_get_value */

/**
 * Set the value of object[property].
 *
 * @return completion value
 */
static ecma_completion_value_t
vm_op_set_value (ecma_value_t object, /**< base object */
                 ecma_value_t property, /**< property name */
                 ecma_value_t value, /**< ecma value */
                 bool is_strict) /**< strict mode */
{
  ecma_completion_value_t completion_value = ecma_make_empty_completion_value ();
  ecma_object_t *object_p;
  ecma_string_t *property_p;

  ECMA_TRY_CATCH (obj_val,
                  ecma_op_to_object (object),
                  completion_value);

  ECMA_TRY_CATCH (property_val,
                  ecma_op_to_string (property),
                  completion_value);

  object_p = ecma_get_object_from_value (obj_val);
  property_p = ecma_get_string_from_value (property_val);

  if (ecma_is_lexical_environment (object_p))
  {
    completion_value = ecma_op_put_value_lex_env_base (object_p,
                                                       property_p,
                                                       is_strict,
                                                       value);
  }
  else
  {
    completion_value = ecma_op_object_put (object_p,
                                           property_p,
                                           value,
                                           true);
  }

  ECMA_FINALIZE (property_val);
  ECMA_FINALIZE (obj_val);

  return completion_value;
} /* vm_op_set_value */

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

/**
 * Run global code
 */
jerry_completion_code_t
vm_run_global (void)
{
  jerry_completion_code_t ret_code;

  JERRY_ASSERT (__program != NULL);

  bool is_strict = false;

  opcode_scope_code_flags_t scope_flags = vm_get_scope_flags (__program);

  if (scope_flags & OPCODE_SCOPE_CODE_FLAGS_STRICT)
  {
    is_strict = true;
  }

  ecma_object_t *glob_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_GLOBAL);
  ecma_object_t *lex_env_p = ecma_get_global_environment ();

  ecma_completion_value_t completion = vm_run (__program,
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

  ecma_completion_value_t completion = vm_run (bytecode_data_p,
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

/**
 * Construct object
 *
 * @return object value
 */
static ecma_value_t
vm_construct_literal_object (vm_frame_ctx_t *frame_ctx_p, /**< frame context */
                             ecma_value_t literal /**< literal */)
{
  ecma_collection_header_t *formal_params_collection_p = ecma_new_strings_collection (NULL, 0);
  cbc_compiled_code_t *bytecode_p = ECMA_GET_NON_NULL_POINTER (cbc_compiled_code_t, literal);

  ecma_object_t *func_obj_p = ecma_op_create_function_object (formal_params_collection_p,
                                                              frame_ctx_p->lex_env_p,
                                                              frame_ctx_p->is_strict,
                                                              bytecode_p);

  return ecma_make_object_value (func_obj_p);
} /* vm_construct_literal_object */

enum
{
  VM_FREE_LEFT_VALUE = 0x1,
  VM_FREE_RIGHT_VALUE = 0x2,
};

#define READ_LITERAL_INDEX(destination) \
  do \
  { \
    (destination) = *byte_code_p++; \
    if ((destination) >= encoding_limit) \
    { \
      (destination) = (((destination) << 8) | *byte_code_p++) - encoding_delta; \
    } \
  } \
  while (0)

/**
 * Run initializer byte codes.
 *
 * @return completion value
 */
ecma_completion_value_t
vm_init_loop (vm_frame_ctx_t *frame_ctx_p /**< frame context */)
{
  const cbc_compiled_code_t *bytecode_header_p = frame_ctx_p->bytecode_header_p;
  uint8_t *byte_code_p = frame_ctx_p->byte_code_p;
  uint16_t encoding_limit;
  uint16_t encoding_delta;
  ecma_value_t *literal_start_p = VM_GET_LITERAL_START_P (bytecode_header_p);

  /* Prepare. */
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

  while (true)
  {
    switch (*byte_code_p)
    {
      case CBC_DEFINE_VARS:
      {
        uint32_t literal_index_end;
        uint32_t literal_index = frame_ctx_p->bytecode_header_p->register_end;

        byte_code_p++;
        READ_LITERAL_INDEX (literal_index_end);

        while (literal_index <= literal_index_end)
        {
          vm_var_decl (frame_ctx_p, literal_start_p[literal_index]);
          literal_index++;
        }
        break;
      }

      case CBC_INITIALIZE_VAR:
      case CBC_INITIALIZE_VARS:
      {
        uint8_t type = *byte_code_p;
        uint32_t literal_index;
        uint32_t literal_index_end;

        byte_code_p++;
        READ_LITERAL_INDEX (literal_index);

        if (type == CBC_INITIALIZE_VAR)
        {
          literal_index_end = literal_index;
        }
        else
        {
          READ_LITERAL_INDEX (literal_index_end);
        }

        while (literal_index <= literal_index_end)
        {
          uint32_t value_index;

          vm_var_decl (frame_ctx_p, literal_start_p[literal_index]);
          READ_LITERAL_INDEX (value_index);

          literal_index++;
        }
        break;
      }

      default:
      {
        frame_ctx_p->byte_code_p = byte_code_p;
        return ecma_make_simple_completion_value (ECMA_SIMPLE_VALUE_UNDEFINED);
      }
    }
  }
}

/* FIXME: For performance reasons, we define this as a macro.
 * When we are able to construct a function with similar speed,
 * we can remove this macro. */
#define READ_LITERAL(literal_index, target_value, target_free_op) \
  do \
  { \
    if ((literal_index) < ident_end) \
    { \
      if ((literal_index) < register_end) \
      { \
        /* Note: There should be no specialization for arguments. */ \
        JERRY_ASSERT (false); \
      } \
      else \
      { \
        ecma_string_t *name_p = ecma_get_string_from_value (literal_start_p[literal_index]); \
        ecma_object_t *ref_base_lex_env_p = ecma_op_resolve_reference_base (frame_ctx_p->lex_env_p, \
                                                                            name_p); \
        JERRY_ASSERT (ref_base_lex_env_p != NULL); \
        last_completion_value = ecma_op_get_value_lex_env_base (ref_base_lex_env_p, \
                                                                name_p, \
                                                                frame_ctx_p->is_strict); \
        if (ecma_is_completion_value_throw (last_completion_value)) \
        { \
          goto error; \
        } \
        (target_value) = ecma_get_completion_value_value (last_completion_value); \
        target_free_op; \
      } \
    } \
    else if (literal_index < const_literal_end) \
    { \
      (target_value) = literal_start_p[literal_index]; \
    } \
    else \
    { \
      /* Object construction. */ \
      (target_value) = vm_construct_literal_object (frame_ctx_p, literal_start_p[literal_index]); \
      target_free_op; \
    } \
  } \
  while (0)

/**
 * Run generic byte code.
 *
 * @return completion value
 */
ecma_completion_value_t
vm_loop (vm_frame_ctx_t *frame_ctx_p /**< frame context */)
{
  const cbc_compiled_code_t *bytecode_header_p = frame_ctx_p->bytecode_header_p;
  uint8_t *byte_code_p = frame_ctx_p->byte_code_p;
  ecma_value_t *literal_start_p = VM_GET_LITERAL_START_P (bytecode_header_p);
  uint16_t encoding_limit;
  uint16_t encoding_delta;
  uint16_t register_end = bytecode_header_p->register_end;
  uint16_t ident_end = bytecode_header_p->ident_end;
  uint16_t const_literal_end = bytecode_header_p->const_literal_end;
  ecma_completion_value_t last_completion_value = ecma_make_empty_completion_value ();
  int32_t branch_offset = 0;
  ecma_value_t left_value = 0;
  ecma_value_t right_value = 0;
  ecma_value_t result = 0;
  uint8_t opcode = 0;
  uint8_t opcode_flags = 0;
  uint8_t free_flags = 0;

  /* Prepare. */
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

  /* Start execution. */
  while (true)
  {
    uint8_t *byte_code_start_p = byte_code_p;
    uint32_t opcode_data;

    opcode = *byte_code_p++;

    if (opcode == CBC_EXT_OPCODE)
    {
      opcode = *byte_code_p++;
      opcode_flags = cbc_ext_flags[opcode];
      opcode_data = vm_ext_decode_table[opcode];
    }
    else
    {
      opcode_flags = cbc_flags[opcode];
      opcode_data = vm_decode_table[opcode];
    }

    if (opcode_flags & CBC_HAS_BRANCH_ARG)
    {
      branch_offset = 0;
      switch (CBC_BRANCH_OFFSET_LENGTH (opcode))
      {
        case 3:
        {
          branch_offset = *(byte_code_p++);
          /* FALLTHRU */
        }
        case 2:
        {
          branch_offset <<= 8;
          branch_offset |= *(byte_code_p++);
          /* FALLTHRU */
        }
        default:
        {
          JERRY_ASSERT (CBC_BRANCH_OFFSET_LENGTH (opcode) > 0);
          branch_offset <<= 8;
          branch_offset |= *(byte_code_p++);
          break;
        }
      }
      if (CBC_BRANCH_IS_BACKWARD (opcode_flags))
      {
        branch_offset = -branch_offset;
      }
    }

    free_flags = 0;
    if (opcode_data & (VM_OC_GET_DATA_MASK << VM_OC_GET_DATA_SHIFT))
    {
      uint32_t operands = VM_OC_GET_DATA_GET_ID (opcode_data);

      if (operands >= VM_OC_GET_DATA_GET_ID (VM_OC_GET_LITERAL))
      {
        uint16_t literal_index;
        READ_LITERAL_INDEX (literal_index);
        READ_LITERAL (literal_index, left_value, free_flags = VM_FREE_LEFT_VALUE);

        switch (operands)
        {
          case VM_OC_GET_DATA_GET_ID (VM_OC_GET_STACK_LITERAL):
          {
            JERRY_ASSERT (vm_stack_top_p > vm_stack);
            right_value = left_value;
            left_value = *(--vm_stack_top_p);
            free_flags = (free_flags << 1) | VM_FREE_LEFT_VALUE;
            break;
          }
          case VM_OC_GET_DATA_GET_ID (VM_OC_GET_LITERAL_BYTE):
          {
            right_value = *(byte_code_p++);
            break;
          }
          case VM_OC_GET_DATA_GET_ID (VM_OC_GET_LITERAL_LITERAL):
          {
            uint16_t literal_index;
            READ_LITERAL_INDEX (literal_index);
            READ_LITERAL (literal_index, right_value, free_flags |= VM_FREE_RIGHT_VALUE);
            break;
          }
          default:
          {
            JERRY_ASSERT (operands == VM_OC_GET_DATA_GET_ID (VM_OC_GET_LITERAL));
            break;
          }
        }
      }
      else
      {
        switch (operands)
        {
          case VM_OC_GET_DATA_GET_ID (VM_OC_GET_STACK):
          {
            JERRY_ASSERT (vm_stack_top_p > vm_stack);
            left_value = *(--vm_stack_top_p);
            free_flags = VM_FREE_LEFT_VALUE;
            break;
          }
          case VM_OC_GET_DATA_GET_ID (VM_OC_GET_STACK_STACK):
          {
            JERRY_ASSERT (vm_stack_top_p > vm_stack + 1);
            right_value = *(--vm_stack_top_p);
            left_value = *(--vm_stack_top_p);
            free_flags = VM_FREE_LEFT_VALUE | VM_FREE_RIGHT_VALUE;
            break;
          }
          case VM_OC_GET_DATA_GET_ID (VM_OC_GET_BYTE):
          {
            right_value = *(byte_code_p++);
            break;
          }
          default:
          {
            JERRY_UNREACHABLE ();
            break;
          }
        }
      }
    }

    switch (VM_OC_GROUP_GET_INDEX (opcode_data))
    {
      case VM_OC_NONE:
      {
        JERRY_UNREACHABLE ();
        break;
      }
      case VM_OC_POP:
      {
        JERRY_ASSERT (vm_stack_top_p > vm_stack);
        ecma_free_value (*(--vm_stack_top_p), true);
        break;
      }
      case VM_OC_PUSH:
      {
        result = ecma_copy_value (left_value, true);
        break;
      }
      case VM_OC_PUSH_TWO:
      {
        *(vm_stack_top_p++) = ecma_copy_value (left_value, true);
        *(vm_stack_top_p++) = ecma_copy_value (right_value, true);
        break;
      }
      case VM_OC_PUSH_UNDEFINED:
      {
        result = ecma_make_simple_value (ECMA_SIMPLE_VALUE_UNDEFINED);
        break;
      }
      case VM_OC_PUSH_TRUE:
      {
        result = ecma_make_simple_value (ECMA_SIMPLE_VALUE_TRUE);
        break;
      }
      case VM_OC_PUSH_FALSE:
      {
        result = ecma_make_simple_value (ECMA_SIMPLE_VALUE_FALSE);
        break;
      }
      case VM_OC_PUSH_NULL:
      {
        result = ecma_make_simple_value (ECMA_SIMPLE_VALUE_NULL);
        break;
      }
      case VM_OC_PUSH_THIS:
      {
        JERRY_UNREACHABLE ();
        break;
      }
      case VM_OC_PUSH_OBJECT:
      {
        ecma_object_t *prototype_p = ecma_builtin_get (ECMA_BUILTIN_ID_OBJECT_PROTOTYPE);
        ecma_object_t *obj_p = ecma_create_object (prototype_p, true, ECMA_OBJECT_TYPE_GENERAL);
        result = ecma_make_object_value (obj_p);
        ecma_deref_object (prototype_p);
        break;
      }
      case VM_OC_PUSH_ARRAY:
      {
        last_completion_value = ecma_op_create_array_object (NULL, 0, false);

        if (ecma_is_completion_value_throw (last_completion_value))
        {
          goto error;
        }
        result = ecma_get_completion_value_value (last_completion_value);
        break;
      }
      case VM_OC_PROP_GET:
      {
        if (opcode == CBC_ASSIGN_PROP_GET)
        {
          left_value = vm_stack_top_p[-2];
          right_value = vm_stack_top_p[-1];
        }
        else if (opcode == CBC_ASSIGN_PROP_LITERAL_GET)
        {
          *vm_stack_top_p++ = ecma_copy_value (left_value, true);
          right_value = left_value;
          left_value = vm_stack_top_p[-2];
        }
        else if (opcode == CBC_ASSIGN_PROP_LITERAL_LITERAL_GET)
        {
          *vm_stack_top_p++ = ecma_copy_value (left_value, true);
          *vm_stack_top_p++ = ecma_copy_value (right_value, true);
        }

        last_completion_value = vm_op_get_value (left_value, right_value, frame_ctx_p->is_strict);

        if (ecma_is_completion_value_throw (last_completion_value))
        {
          goto error;
        }
        result = ecma_get_completion_value_value (last_completion_value);
        break;
      }
      case VM_OC_ASSIGN_IDENT:
      {
        uint16_t literal_index;
        READ_LITERAL_INDEX (literal_index);

        JERRY_ASSERT (literal_index < ident_end);

        if ((literal_index) < register_end)
        {
          JERRY_ASSERT (false);
        }
        else
        {
          ecma_value_t name = literal_start_p[literal_index];
          ecma_string_t *name_p = ecma_get_string_from_value (name);
          ecma_object_t *ref_base_lex_env_p = ecma_op_resolve_reference_base (frame_ctx_p->lex_env_p,
                                                                              name_p);
                                                                              JERRY_ASSERT (ref_base_lex_env_p != NULL);

          last_completion_value = ecma_op_get_value_lex_env_base (ref_base_lex_env_p,
                                                                  name_p,
                                                                  frame_ctx_p->is_strict);

          if (ecma_is_completion_value_throw (last_completion_value))
          {
            goto error;
          }

          ecma_ref_object (ref_base_lex_env_p);
          *vm_stack_top_p++ = ecma_make_object_value (ref_base_lex_env_p);
          *vm_stack_top_p++ = ecma_make_string_value (ecma_copy_or_ref_ecma_string (name_p));

          result = ecma_get_completion_value_value (last_completion_value);
        }
        break;
      }
      case VM_OC_ASSIGN:
      {
        result = left_value;
        break;
      }
      case VM_OC_ASSIGN_PROP:
      {
        result = vm_stack_top_p[-1];
        vm_stack_top_p[-1] = ecma_copy_value (left_value, true);
        break;
      }
      case VM_OC_RET:
      {
        JERRY_ASSERT (opcode == CBC_RETURN
                      || opcode == CBC_RETURN_WITH_UNDEFINED
                      || opcode == CBC_RETURN_WITH_LITERAL);

        if (opcode == CBC_RETURN_WITH_UNDEFINED)
        {
          left_value = ecma_make_simple_value (ECMA_SIMPLE_VALUE_UNDEFINED);
        }

        last_completion_value = ecma_make_completion_value (ECMA_COMPLETION_TYPE_RETURN, left_value);
        goto error;
      }
      case VM_OC_CALL:
      {
        last_completion_value = ecma_make_empty_completion_value ();

        vm_stack_top_p -= right_value;

        ECMA_TRY_CATCH (value,
                        opfunc_call_n (frame_ctx_p, vm_stack_top_p[-1], right_value, vm_stack_top_p),
                        last_completion_value);

        // FIXME: check push result
        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        for (uint32_t i = 0; i < right_value; i++)
        {
          ecma_free_value (vm_stack_top_p[i], true);
        }
        ecma_free_value (*(--vm_stack_top_p), true);

        if (ecma_is_completion_value_throw (last_completion_value))
        {
          goto error;
        }
        break;
      }
      case VM_OC_JUMP:
      {
        byte_code_p = byte_code_start_p + branch_offset;
        break;
      }
      case VM_OC_BRANCH_IF_TRUE:
      case VM_OC_BRANCH_IF_FALSE:
      case VM_OC_BRANCH_IF_TRUE_LOGICAL:
      case VM_OC_BRANCH_IF_FALSE_LOGICAL:
      {
        last_completion_value = ecma_make_empty_completion_value ();
        uint32_t base = VM_OC_GROUP_GET_INDEX (opcode_data) - VM_OC_BRANCH_IF_TRUE;

        ECMA_TRY_CATCH (value,
                        ecma_op_to_boolean (left_value),
                        last_completion_value);

        PARSER_ASSERT (free_flags & VM_FREE_LEFT_VALUE);
        if (value == ecma_make_simple_value ((base & 0x1) ? ECMA_SIMPLE_VALUE_FALSE : ECMA_SIMPLE_VALUE_TRUE))
        {
          byte_code_p = byte_code_start_p + branch_offset;
          if (base & 0x2)
          {
            free_flags = 0;
            ++vm_stack_top_p;
          }
        }

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (last_completion_value))
        {
          goto error;
        }

        break;
      }
      case VM_OC_BRANCH_STRICT_EQUAL:
      {
        last_completion_value = ecma_make_empty_completion_value ();

        JERRY_ASSERT (vm_stack_top_p > vm_stack);

        ECMA_TRY_CATCH (value,
                        opfunc_equal_value_type (left_value, vm_stack_top_p[-1]),
                        last_completion_value);

        if (value == ecma_make_simple_value (ECMA_SIMPLE_VALUE_TRUE))
        {
          byte_code_p = byte_code_start_p + branch_offset;
          ecma_free_value (*--vm_stack_top_p, true);
        }

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (last_completion_value))
        {
          goto error;
        }
        break;
      }
      case VM_OC_PLUS:
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
      case VM_OC_MINUS:
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
      case VM_OC_NOT:
      {
        JERRY_UNREACHABLE ();
        break;
      }
      case VM_OC_BIT_NOT:
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
      case VM_OC_VOID:
      {
        JERRY_UNREACHABLE ();
        break;
      }
      case VM_OC_TYPEOF:
      {
        JERRY_UNREACHABLE ();
        break;
      }
      case VM_OC_ADD:
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
      case VM_OC_SUB:
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
      case VM_OC_MUL:
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
      case VM_OC_DIV:
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
      case VM_OC_MOD:
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
      case VM_OC_EQUAL:
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
      case VM_OC_NOT_EQUAL:
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
      case VM_OC_STRICT_EQUAL:
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
      case VM_OC_STRICT_NOT_EQUAL:
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
      case VM_OC_BIT_OR:
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
      case VM_OC_BIT_XOR:
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
      case VM_OC_BIT_AND:
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
      case VM_OC_LEFT_SHIFT:
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
      case VM_OC_RIGHT_SHIFT:
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
      case VM_OC_UNS_RIGHT_SHIFT:
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
      case VM_OC_LESS:
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
      case VM_OC_GREATER:
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
      case VM_OC_LESS_EQUAL:
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
      case VM_OC_GREATER_EQUAL:
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
      case VM_OC_IN:
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
      case VM_OC_INSTANCEOF:
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
      default:
      {
        JERRY_UNREACHABLE ();
        break;
      }
    }

    if (opcode_data & (VM_OC_PUT_DATA_MASK << VM_OC_PUT_DATA_SHIFT))
    {
      switch (VM_OC_PUT_DATA_GET_ID (opcode_data))
      {
        case VM_OC_PUT_DATA_GET_ID (VM_OC_PUT_STACK):
        {
          *(vm_stack_top_p++) = result;
          break;
        }
        case VM_OC_PUT_DATA_GET_ID (VM_OC_PUT_IDENT):
        {
          ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();
          uint16_t literal_index;
          ecma_string_t *var_name_str_p;
          ecma_object_t *ref_base_lex_env_p;

          READ_LITERAL_INDEX (literal_index);

          var_name_str_p = ecma_get_string_from_value (literal_start_p[literal_index]);
          ref_base_lex_env_p = ecma_op_resolve_reference_base (frame_ctx_p->lex_env_p, var_name_str_p);

          ret_value = ecma_op_put_value_lex_env_base (ref_base_lex_env_p,
                                                      var_name_str_p,
                                                      frame_ctx_p->is_strict,
                                                      result);

          if (ecma_is_completion_value_throw (ret_value))
          {
            goto error;
          }
          break;
        }
        case VM_OC_PUT_DATA_GET_ID (VM_OC_PUT_REFERENCE):
        {
          ecma_value_t property = *(--vm_stack_top_p);
          ecma_value_t object = *(--vm_stack_top_p);

          last_completion_value = vm_op_set_value (object,
                                                   property,
                                                   result,
                                                   frame_ctx_p->is_strict);

          ecma_free_value (object, true);
          ecma_free_value (property, true);

          if (ecma_is_completion_value_throw (last_completion_value))
          {
            goto error;
          }
          break;
        }
        default:
        {
          JERRY_UNREACHABLE ();
          break;
        }
      }
    }

    if (free_flags & VM_FREE_LEFT_VALUE)
    {
      ecma_free_value (left_value, true);
    }

    if (free_flags & VM_FREE_RIGHT_VALUE)
    {
      ecma_free_value (right_value, true);
    }
  }
error:

  if (free_flags & VM_FREE_LEFT_VALUE)
  {
    ecma_free_value (left_value, true);
  }

  if (free_flags & VM_FREE_RIGHT_VALUE)
  {
    ecma_free_value (right_value, true);
  }

  return last_completion_value;
}

#undef READ_LITERAL
#undef READ_LITERAL_INDEX

/**
 * Run the code, starting from specified instruction position
 */
ecma_completion_value_t
vm_run (const cbc_compiled_code_t *bytecode_header_p, /**< byte-code data header */
        ecma_value_t this_binding_value, /**< value of 'ThisBinding' */
        ecma_object_t *lex_env_p, /**< lexical environment to use */
        bool is_strict, /**< is the code is strict mode code (ECMA-262 v5, 10.1.1) */
        bool is_eval_code) /**< is the code is eval code (ECMA-262 v5, 10.1) */
{
  ecma_completion_value_t completion = ecma_make_empty_completion_value ();
  ecma_value_t *literal_start_p = VM_GET_LITERAL_START_P (bytecode_header_p);
  vm_frame_ctx_t frame_ctx;

  frame_ctx.bytecode_header_p = bytecode_header_p;
  frame_ctx.byte_code_p = (uint8_t *) (literal_start_p + bytecode_header_p->literal_end);
  frame_ctx.lex_env_p = lex_env_p;
  frame_ctx.is_strict = is_strict;
  frame_ctx.is_eval_code = is_eval_code;
  frame_ctx.is_call_in_direct_eval_form = false;
  frame_ctx.ref_base_lex_env_p = lex_env_p;

//  vm_stack_add_frame (&frame_ctx.stack_frame, regs, regs_num, local_var_regs_num, arg_regs_num, arg_collection_p);
//  vm_stack_frame_set_reg_value (&frame_ctx.stack_frame,
//                                VM_REG_SPECIAL_THIS_BINDING,
//                                ecma_copy_value (this_binding_value, false));

  vm_frame_ctx_t *prev_context_p = vm_top_context_p;
  vm_top_context_p = &frame_ctx;

  vm_init_loop (&frame_ctx);

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
