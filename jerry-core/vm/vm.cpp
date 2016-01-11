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

#include "common.h"

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

  ecma_free_values_collection (formal_params_collection_p, true);

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
        (target_value) = frame_ctx_p->registers_p[literal_index];\
      } \
      else \
      { \
        ecma_string_t *name_p = ecma_new_ecma_string_from_lit_cp (literal_start_p[literal_index]); \
        ecma_object_t *ref_base_lex_env_p = ecma_op_resolve_reference_base (frame_ctx_p->lex_env_p, \
                                                                            name_p); \
        JERRY_ASSERT (ref_base_lex_env_p != NULL); \
        last_completion_value = ecma_op_get_value_lex_env_base (ref_base_lex_env_p, \
                                                                name_p, \
                                                                frame_ctx_p->is_strict); \
        \
        ecma_deref_ecma_string (name_p); \
        \
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
      lit_cpointer_t lit_cpointer = literal_start_p[literal_index]; \
      literal_t lit = lit_cpointer_t::decompress (lit_cpointer); \
      if (lit->get_type () != LIT_NUMBER_T) \
      { \
        ecma_string_t *string_p = ecma_new_ecma_string_from_lit_cp (lit_cpointer); \
        (target_value) = ecma_make_string_value (string_p); \
      } \
      else \
      { \
        ecma_number_t *number_p = ecma_alloc_number (); \
        *number_p = lit_charset_literal_get_number (lit); \
        (target_value) = ecma_make_number_value (number_p); \
      } \
      target_free_op; \
    } \
    else \
    { \
      /* Object construction. */ \
      (target_value) = vm_construct_literal_object (frame_ctx_p, literal_start_p[literal_index].value.base_cp); \
      target_free_op; \
    } \
  } \
  while (0)

/**
 * Cleanup compact bytecode
 */
void
vm_cleanup_cbc (const cbc_compiled_code_t *bytecode_p) /**< Bytecode */
{
  lit_cpointer_t *literal_start_p = NULL;
  uint16_t literal_end;
  uint16_t const_literal_end;

  if (bytecode_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
  {
    uint8_t *byte_p = ((uint8_t *) bytecode_p + sizeof (cbc_uint16_arguments_t));
    literal_start_p = (lit_cpointer_t *) byte_p;
    cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) bytecode_p;
    literal_end = args_p->literal_end;
    const_literal_end = args_p->const_literal_end;
  }
  else
  {
    uint8_t *byte_p = ((uint8_t *) bytecode_p + sizeof (cbc_uint8_arguments_t));
    literal_start_p = (lit_cpointer_t *) byte_p;
    cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) bytecode_p;
    literal_end = args_p->literal_end;
    const_literal_end = args_p->const_literal_end;
  }

  for (int i = const_literal_end; i < literal_end; i++)
  {
    cbc_compiled_code_t *func_bytecode_p = ECMA_GET_NON_NULL_POINTER (cbc_compiled_code_t,
                                                                      literal_start_p[i].value.base_cp);
    vm_cleanup_cbc (func_bytecode_p);
  }

  mem_heap_free_block (bytecode_p);
} /* vm_cleanup_cbc */

/**
 * Cleanup interpreter
 */
void
vm_finalize (void)
{
  if (__program)
  {
    vm_cleanup_cbc (__program);
  }

  __program = NULL;
} /* vm_finalize */

/**
 * Run initializer byte codes.
 *
 * @return completion value
 */
static ecma_completion_value_t
vm_init_loop (vm_frame_ctx_t *frame_ctx_p) /**< frame context */
{
  const cbc_compiled_code_t *bytecode_header_p = frame_ctx_p->bytecode_header_p;
  uint8_t *byte_code_p = frame_ctx_p->byte_code_p;
  ecma_completion_value_t last_completion_value = ecma_make_empty_completion_value ();
  uint16_t encoding_limit;
  uint16_t encoding_delta;
  uint16_t register_end;
  uint16_t ident_end;
  uint16_t const_literal_end;
  lit_cpointer_t *literal_start_p = frame_ctx_p->literal_start_p;

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
        uint32_t literal_index;

        if (frame_ctx_p->bytecode_header_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
        {
          cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) (frame_ctx_p->bytecode_header_p);
          literal_index = args_p->register_end;
          register_end = args_p->register_end;
          ident_end = args_p->ident_end;
          const_literal_end = args_p->const_literal_end;
        }
        else
        {
          cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) (frame_ctx_p->bytecode_header_p);
          literal_index = args_p->register_end;
          register_end = args_p->register_end;
          ident_end = args_p->ident_end;
          const_literal_end = args_p->const_literal_end;
        }

        byte_code_p++;
        READ_LITERAL_INDEX (literal_index_end);

        while (literal_index <= literal_index_end)
        {
          ecma_string_t *name_p = ecma_new_ecma_string_from_lit_cp (literal_start_p[literal_index]);
          vm_var_decl (frame_ctx_p, name_p);
          ecma_deref_ecma_string (name_p);
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
          uint8_t free_flags = 0;
          ecma_value_t lit_value;
          ecma_string_t *name_p = ecma_new_ecma_string_from_lit_cp (literal_start_p[literal_index]);

          vm_var_decl (frame_ctx_p, name_p);

          READ_LITERAL_INDEX (value_index);
          READ_LITERAL (value_index, lit_value, free_flags);

          ecma_object_t *ref_base_lex_env_p = ecma_op_resolve_reference_base (frame_ctx_p->lex_env_p, name_p);

          // FIXME: check the return value
          ecma_op_put_value_lex_env_base (ref_base_lex_env_p,
                                          name_p,
                                          frame_ctx_p->is_strict,
                                          lit_value);

          ecma_free_value (lit_value, true);
          ecma_deref_ecma_string (name_p);
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

error:
  return last_completion_value;
}

/**
 * Run generic byte code.
 *
 * @return completion value
 */
ecma_completion_value_t
vm_loop (vm_frame_ctx_t *frame_ctx_p) /**< frame context */
{
  const cbc_compiled_code_t *bytecode_header_p = frame_ctx_p->bytecode_header_p;
  uint8_t *byte_code_p = frame_ctx_p->byte_code_p;
  lit_cpointer_t *literal_start_p = frame_ctx_p->literal_start_p;
  ecma_completion_value_t last_completion_value = ecma_make_empty_completion_value ();
  uint16_t encoding_limit;
  uint16_t encoding_delta;
  uint16_t register_end;
  uint16_t ident_end;
  uint16_t const_literal_end;
  int32_t branch_offset = 0;
  ecma_value_t left_value = 0;
  ecma_value_t right_value = 0;
  ecma_value_t result = 0;
  ecma_value_t block_result = ecma_make_simple_value (ECMA_SIMPLE_VALUE_UNDEFINED);
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

  if (bytecode_header_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
  {
    cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) (bytecode_header_p);
    register_end = args_p->register_end;
    ident_end = args_p->ident_end;
    const_literal_end = args_p->const_literal_end;
  }
  else
  {
    cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) (bytecode_header_p);
    register_end = args_p->register_end;
    ident_end = args_p->ident_end;
    const_literal_end = args_p->const_literal_end;
  }

  /* Start execution. */
  while (true)
  {
    uint8_t *byte_code_start_p = byte_code_p;
    uint8_t opcode;
    uint8_t opcode_flags;
    uint32_t opcode_data;

    opcode = *byte_code_p++;

#ifndef JERRY_NDEBUG
    char *opcode_name = NULL;
#endif

    if (opcode == CBC_EXT_OPCODE)
    {
      opcode = *byte_code_p++;
      opcode_flags = cbc_ext_flags[opcode];
      opcode_data = vm_ext_decode_table[opcode];
#ifndef JERRY_NDEBUG
      opcode_name = cbc_ext_names[opcode];
#endif
    }
    else
    {
      opcode_flags = cbc_flags[opcode];
      opcode_data = vm_decode_table[opcode];
#ifndef JERRY_NDEBUG
      opcode_name = cbc_names[opcode];
#endif
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
#ifndef JERRY_NDEBUG
        fprintf (stderr, "Unimplemented opcode: %s\n", opcode_name);
#endif /* JERRY_NDEBUG */
        JERRY_UNREACHABLE ();
        break;
      }
      case VM_OC_POP:
      {
        JERRY_ASSERT (vm_stack_top_p > vm_stack);
        ecma_free_value (*(--vm_stack_top_p), true);
        break;
      }
      case VM_OC_POP_BLOCK:
      {
        result = *(--vm_stack_top_p);
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
      case VM_OC_PUSH_THREE:
      {
        uint16_t literal_index;

        *(vm_stack_top_p++) = ecma_copy_value (left_value, true);
        *(vm_stack_top_p++) = ecma_copy_value (right_value, true);

        if (free_flags & VM_FREE_RIGHT_VALUE)
        {
          ecma_free_value (right_value, true);
          free_flags = (uint8_t) (free_flags & ~VM_FREE_RIGHT_VALUE);
        }

        READ_LITERAL_INDEX (literal_index);
        READ_LITERAL (literal_index, right_value, free_flags |= VM_FREE_RIGHT_VALUE);
        *(vm_stack_top_p++) = ecma_copy_value (right_value, true);
        break;
      }
      case VM_OC_PUSH_UNDEFINED:
      case VM_OC_VOID:
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
      case VM_OC_PUSH_NUMBER:
      {
        ecma_number_t *number_p = ecma_alloc_number ();

        if (opcode == CBC_PUSH_NUMBER_0)
        {
          *number_p = 0;
        }
        else
        {
          int value = *byte_code_p++;

          JERRY_ASSERT (opcode == CBC_PUSH_NUMBER_1);

          if (value >= CBC_PUSH_NUMBER_1_RANGE_END)
          {
            value = -(value - CBC_PUSH_NUMBER_1_RANGE_END);
          }
          *number_p = (ecma_number_t) value;
        }

        result = ecma_make_number_value (number_p);
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
      case VM_OC_PUSH_ELISON:
      {
        result = ecma_make_simple_value (ECMA_SIMPLE_VALUE_ARRAY_HOLE);
        break;
      }
      case VM_OC_APPEND_ARRAY:
      {
        vm_stack_top_p -= right_value;

        ecma_object_t *array_obj_p = ecma_get_object_from_value (vm_stack_top_p[-1]);
        ecma_string_t *magic_str_p = ecma_get_magic_string (LIT_MAGIC_STRING_LENGTH);
        ecma_property_t *length_prop_p = ecma_get_named_property (array_obj_p, magic_str_p);
        ecma_number_t *length_num_p = ecma_get_number_from_value (length_prop_p->u.named_data_property.value);
        ecma_deref_ecma_string (magic_str_p);

        for (uint32_t i = 0; i < right_value; i++)
        {
          if (ecma_is_value_array_hole (vm_stack_top_p[i]))
          {
            (*length_num_p)++;
          }
          else
          {
            ecma_string_t *index_str_p = ecma_new_ecma_string_from_uint32 ((*length_num_p)++);
            ecma_op_object_put (array_obj_p, index_str_p, vm_stack_top_p[i], true);
            ecma_deref_ecma_string (index_str_p);

            ecma_free_value (vm_stack_top_p[i], true);
          }
        }

        break;
      }
      case VM_OC_PUSH_UNDEFINED_BASE:
      {
        result = vm_stack_top_p[-1];
        vm_stack_top_p[-1] = ecma_make_simple_value (ECMA_SIMPLE_VALUE_UNDEFINED);
        break;
      }
      case VM_OC_IDENT_REFERENCE:
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
          ecma_string_t *name_p = ecma_new_ecma_string_from_lit_cp (literal_start_p[literal_index]);
          ecma_object_t *ref_base_lex_env_p = ecma_op_resolve_reference_base (frame_ctx_p->lex_env_p,
                                                                              name_p);
                                                                              JERRY_ASSERT (ref_base_lex_env_p != NULL);

          last_completion_value = ecma_op_get_value_lex_env_base (ref_base_lex_env_p,
                                                                  name_p,
                                                                  frame_ctx_p->is_strict);

          if (ecma_is_completion_value_throw (last_completion_value))
          {
            ecma_deref_ecma_string (name_p);
            goto error;
          }

          ecma_ref_object (ref_base_lex_env_p);
          *vm_stack_top_p++ = ecma_make_object_value (ref_base_lex_env_p);
          *vm_stack_top_p++ = ecma_make_string_value (name_p);

          result = ecma_get_completion_value_value (last_completion_value);
        }
        break;
      }
      case VM_OC_PROP_REFERENCE:
      {
        /* Forms with reference requires preserving the base and offset. */

        if (opcode == CBC_PUSH_PROP_REFERENCE)
        {
          left_value = vm_stack_top_p[-2];
          right_value = vm_stack_top_p[-1];
        }
        else if (opcode == CBC_PUSH_PROP_LITERAL_REFERENCE)
        {
          *vm_stack_top_p++ = ecma_copy_value (left_value, true);
          right_value = left_value;
          left_value = vm_stack_top_p[-2];
        }
        else
        {
          JERRY_ASSERT (opcode == CBC_PUSH_PROP_LITERAL_LITERAL_REFERENCE
                         || opcode == CBC_PUSH_PROP_THIS_LITERAL_REFERENCE);
          *vm_stack_top_p++ = ecma_copy_value (left_value, true);
          *vm_stack_top_p++ = ecma_copy_value (right_value, true);
        }
        /* FALLTHRU */
      }
      case VM_OC_PROP_GET:
      {
        last_completion_value = vm_op_get_value (left_value, right_value, frame_ctx_p->is_strict);

        if (ecma_is_completion_value_throw (last_completion_value))
        {
          goto error;
        }
        result = ecma_get_completion_value_value (last_completion_value);
        break;
      }
      case VM_OC_ASSIGN:
      {
        result = left_value;
        free_flags = 0;
        break;
      }
      case VM_OC_ASSIGN_PROP:
      {
        result = vm_stack_top_p[-1];
        vm_stack_top_p[-1] = left_value;
        free_flags = 0;
        break;
      }
      case VM_OC_RET:
      {
        JERRY_ASSERT (opcode == CBC_RETURN
                      || opcode == CBC_RETURN_WITH_UNDEFINED
                      || opcode == CBC_RETURN_WITH_LITERAL);

        if (opcode == CBC_RETURN_WITH_UNDEFINED)
        {
          left_value = block_result;
          block_result = ecma_make_simple_value (ECMA_SIMPLE_VALUE_UNDEFINED);
        }

        last_completion_value = ecma_make_completion_value (ECMA_COMPLETION_TYPE_RETURN, left_value);
        goto error;
      }
      case VM_OC_CALL_N:
      case VM_OC_CALL_PROP_N:
      {
        if (opcode >= CBC_CALL2)
        {
          right_value = 2;
        }
        else if (opcode >= CBC_CALL1)
        {
          right_value = 1;
        }
        else
        {
          right_value = 0;
        }
        /* FALLTHRU */
      }
      case VM_OC_NEW_N:
      {
        if (opcode == CBC_NEW1)
        {
          right_value = 1;
        }
        else if (opcode == CBC_NEW0)
        {
          right_value = 0;
        }
        /* FALLTHRU */
      }
      case VM_OC_NEW:
      case VM_OC_CALL:
      case VM_OC_CALL_PROP:
      {
        last_completion_value = ecma_make_empty_completion_value ();

        vm_stack_top_p -= right_value;

        if (opcode >= CBC_CALL)
        {
          ECMA_TRY_CATCH (returned_value,
                          opfunc_call_n (frame_ctx_p, vm_stack_top_p[-1], right_value, vm_stack_top_p),
                          last_completion_value);

          if (opcode_data & (VM_OC_PUT_DATA_MASK << VM_OC_PUT_DATA_SHIFT))
          {
            result = ecma_copy_value (returned_value, true);
          }

          ECMA_FINALIZE (returned_value);
        }
        else
        {
          ECMA_TRY_CATCH (returned_value,
                          opfunc_construct_n (frame_ctx_p, vm_stack_top_p[-1], right_value, vm_stack_top_p),
                          last_completion_value);

          if (opcode_data & (VM_OC_PUT_DATA_MASK << VM_OC_PUT_DATA_SHIFT))
          {
            result = ecma_copy_value (returned_value, true);
          }

          ECMA_FINALIZE (returned_value);
        }

        for (uint32_t i = 0; i < right_value; i++)
        {
          ecma_free_value (vm_stack_top_p[i], true);
        }

        ecma_free_value (*(--vm_stack_top_p), true);

        if (VM_OC_GROUP_GET_INDEX (opcode_data) >= VM_OC_CALL_PROP)
        {
          ecma_free_value (*(--vm_stack_top_p), true);
          ecma_free_value (*(--vm_stack_top_p), true);
        }

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

        JERRY_ASSERT (free_flags & VM_FREE_LEFT_VALUE);
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
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_logical_not (left_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

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
      case VM_OC_TYPEOF:
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

        ECMA_TRY_CATCH (value, opfunc_typeof (left_value), ret_value);

        result = ecma_copy_value (value, true);

        ECMA_FINALIZE (value);

        if (ecma_is_completion_value_throw (ret_value))
        {
          // FIXME: Early exit may cause memory leak.
          return ret_value;
        }

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
      case VM_OC_TRY:
      {
        uint32_t end = byte_code_p - frame_ctx_p->byte_code_start_p + branch_offset;

        JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == vm_stack_top_p);

        VM_PLUS_EQUAL_U16 (frame_ctx_p->context_depth, PARSER_TRY_CONTEXT_STACK_ALLOCATION);
        vm_stack_top_p += PARSER_TRY_CONTEXT_STACK_ALLOCATION;

        vm_stack_top_p[-1] = VM_SET_CONTEXT_DESCRIPTOR (VM_CONTEXT_TRY, end);
        break;
      }
      case VM_OC_CATCH:
      {
        /* Catches are ignored and turned to jumps. */
        JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == vm_stack_top_p);
        JERRY_ASSERT (VM_GET_CONTEXT_TYPE (vm_stack_top_p[-1]) == VM_CONTEXT_TRY);

        byte_code_p = byte_code_start_p + branch_offset;
        break;
      }
      case VM_OC_FINALLY:
      {
        uint32_t end = byte_code_p - frame_ctx_p->byte_code_start_p + branch_offset;

        JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == vm_stack_top_p);
        JERRY_ASSERT (VM_GET_CONTEXT_TYPE (vm_stack_top_p[-1]) == VM_CONTEXT_TRY);

        vm_stack_top_p[-1] = VM_SET_CONTEXT_DESCRIPTOR (VM_CONTEXT_FINALLY, end);
        vm_stack_top_p[-2] = 0;
        break;
      }
      case VM_OC_CONTEXT_END:
      {
        JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == vm_stack_top_p);
        vm_stack_top_p = opfunc_context_end (frame_ctx_p, vm_stack_top_p);
        JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == vm_stack_top_p);
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
      if (opcode_data & VM_OC_PUT_IDENT)
      {
        ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();
        uint16_t literal_index;
        ecma_string_t *var_name_str_p;
        ecma_object_t *ref_base_lex_env_p;

        READ_LITERAL_INDEX (literal_index);

        var_name_str_p = ecma_new_ecma_string_from_lit_cp (literal_start_p[literal_index]);
        ref_base_lex_env_p = ecma_op_resolve_reference_base (frame_ctx_p->lex_env_p, var_name_str_p);

        ret_value = ecma_op_put_value_lex_env_base (ref_base_lex_env_p,
                                                    var_name_str_p,
                                                    frame_ctx_p->is_strict,
                                                    result);

        ecma_deref_ecma_string (var_name_str_p);

        if (!(opcode_data & (VM_OC_PUT_STACK | VM_OC_PUT_BLOCK)))
        {
          ecma_free_value (result, true);
        }

        if (ecma_is_completion_value_throw (ret_value))
        {
          goto error;
        }
      }
      else if (opcode_data & VM_OC_PUT_REFERENCE)
      {
        ecma_value_t property = *(--vm_stack_top_p);
        ecma_value_t object = *(--vm_stack_top_p);

        last_completion_value = vm_op_set_value (object,
                                                 property,
                                                 result,
                                                 frame_ctx_p->is_strict);

        ecma_free_value (object, true);
        ecma_free_value (property, true);

        if (!(opcode_data & (VM_OC_PUT_STACK | VM_OC_PUT_BLOCK)))
        {
          ecma_free_value (result, true);
        }

        if (ecma_is_completion_value_throw (last_completion_value))
        {
          goto error;
        }
      }

      if (opcode_data & VM_OC_PUT_STACK)
      {
        *vm_stack_top_p++ = result;
      }
      else if (opcode_data & VM_OC_PUT_BLOCK)
      {
        ecma_free_value (block_result, true);
        block_result = result;
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

  ecma_free_value (block_result, true);

  if (free_flags & VM_FREE_LEFT_VALUE)
  {
    ecma_free_value (left_value, true);
  }

  if (free_flags & VM_FREE_RIGHT_VALUE)
  {
    ecma_free_value (right_value, true);
  }

  return last_completion_value;
} /* vm_loop */

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
  lit_cpointer_t *literal_p;
  vm_frame_ctx_t frame_ctx;

  if (bytecode_header_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
  {
    uint8_t *byte_p = ((uint8_t *) bytecode_header_p + sizeof (cbc_uint16_arguments_t));
    literal_p = (lit_cpointer_t *) byte_p;
    frame_ctx.literal_start_p = literal_p;
    literal_p += ((cbc_uint16_arguments_t *) bytecode_header_p)->literal_end;
  }
  else
  {
    uint8_t *byte_p = ((uint8_t *) bytecode_header_p + sizeof (cbc_uint8_arguments_t));
    literal_p = (lit_cpointer_t *) byte_p;
    frame_ctx.literal_start_p = literal_p;
    literal_p += ((cbc_uint8_arguments_t *) bytecode_header_p)->literal_end;
  }

  frame_ctx.bytecode_header_p = bytecode_header_p;
  frame_ctx.byte_code_p = (uint8_t *) literal_p;
  frame_ctx.byte_code_start_p = (uint8_t *) literal_p;
  frame_ctx.registers_p = vm_stack_top_p;
  frame_ctx.lex_env_p = lex_env_p;
  frame_ctx.ref_base_lex_env_p = lex_env_p;
  frame_ctx.context_depth = 0;
  frame_ctx.is_strict = is_strict;
  frame_ctx.is_eval_code = is_eval_code;
  frame_ctx.is_call_in_direct_eval_form = false;

  vm_frame_ctx_t *prev_context_p = vm_top_context_p;
  vm_top_context_p = &frame_ctx;

  vm_init_loop (&frame_ctx);

  completion = vm_loop (&frame_ctx);

//  JERRY_ASSERT (ecma_is_completion_value_throw (completion)
//                || ecma_is_completion_value_return (completion));

  vm_top_context_p = prev_context_p;

  return completion;
} /* vm_run */

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
