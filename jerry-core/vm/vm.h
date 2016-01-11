/* Copyright 2014-2015 Samsung Electronics Co., Ltd.
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

#ifndef VM_H
#define VM_H

#include "ecma-globals.h"
#include "jrt.h"
#include "vm-defines.h"

#define VM_OC_GET_DATA_SHIFT 24
#define VM_OC_GET_DATA_MASK 0x1f
#define VM_OC_GET_DATA_CREATE_ID(V) \
  (((V) & VM_OC_GET_DATA_MASK) << VM_OC_GET_DATA_SHIFT)
#define VM_OC_GET_DATA_GET_ID(O) \
  (((O) >> VM_OC_GET_DATA_SHIFT) & VM_OC_GET_DATA_MASK)

enum {
  VM_OC_GET_NONE = VM_OC_GET_DATA_CREATE_ID(0),
  VM_OC_GET_STACK = VM_OC_GET_DATA_CREATE_ID(1),
  VM_OC_GET_STACK_STACK = VM_OC_GET_DATA_CREATE_ID(2),
  VM_OC_GET_BYTE = VM_OC_GET_DATA_CREATE_ID(3),

  VM_OC_GET_LITERAL = VM_OC_GET_DATA_CREATE_ID(4),
  VM_OC_GET_STACK_LITERAL = VM_OC_GET_DATA_CREATE_ID(5),
  VM_OC_GET_LITERAL_BYTE = VM_OC_GET_DATA_CREATE_ID(6),
  VM_OC_GET_LITERAL_LITERAL = VM_OC_GET_DATA_CREATE_ID(7),
  VM_OC_GET_THIS_LITERAL = VM_OC_GET_DATA_CREATE_ID(8),
};

#define VM_OC_GROUP_MASK 0xff
#define VM_OC_GROUP_GET_INDEX(O) \
  ((O) & VM_OC_GROUP_MASK)

enum {
  VM_OC_NONE,
  VM_OC_POP,
  VM_OC_POP_BLOCK,
  VM_OC_PUSH,
  VM_OC_PUSH_TWO,
  VM_OC_PUSH_THREE,
  VM_OC_PUSH_UNDEFINED,
  VM_OC_PUSH_TRUE,
  VM_OC_PUSH_FALSE,
  VM_OC_PUSH_NULL,
  VM_OC_PUSH_THIS,
  VM_OC_PUSH_NUMBER,
  VM_OC_PUSH_OBJECT,
  VM_OC_PUSH_UNDEFINED_BASE,
  VM_OC_PUSH_ARRAY,
  VM_OC_APPEND_ARRAY,
  VM_OC_IDENT_REFERENCE,
  VM_OC_PROP_REFERENCE,
  VM_OC_PROP_GET,

  VM_OC_ASSIGN,
  VM_OC_ASSIGN_PROP,

  VM_OC_RET,
  VM_OC_NEW,
  VM_OC_NEW_N,
  VM_OC_CALL,
  VM_OC_CALL_N,
  VM_OC_CALL_PROP,
  VM_OC_CALL_PROP_N,

  VM_OC_JUMP,
  VM_OC_BRANCH_IF_TRUE,
  VM_OC_BRANCH_IF_FALSE,
  VM_OC_BRANCH_IF_TRUE_LOGICAL,
  VM_OC_BRANCH_IF_FALSE_LOGICAL,
  VM_OC_BRANCH_STRICT_EQUAL,

  VM_OC_PLUS,
  VM_OC_MINUS,
  VM_OC_NOT,
  VM_OC_BIT_NOT,
  VM_OC_VOID,
  VM_OC_TYPEOF,

  VM_OC_ADD,
  VM_OC_SUB,
  VM_OC_MUL,
  VM_OC_DIV,
  VM_OC_MOD,

  VM_OC_EQUAL,
  VM_OC_NOT_EQUAL,
  VM_OC_STRICT_EQUAL,
  VM_OC_STRICT_NOT_EQUAL,

  VM_OC_BIT_OR,
  VM_OC_BIT_XOR,
  VM_OC_BIT_AND,
  VM_OC_LEFT_SHIFT,
  VM_OC_RIGHT_SHIFT,
  VM_OC_UNS_RIGHT_SHIFT,

  VM_OC_LESS,
  VM_OC_GREATER,
  VM_OC_LESS_EQUAL,
  VM_OC_GREATER_EQUAL,
  VM_OC_IN,
  VM_OC_INSTANCEOF,

  VM_OC_TRY,
  VM_OC_CATCH,
  VM_OC_FINALLY,
  VM_OC_CONTEXT_END,
};

#define VM_OC_PUT_DATA_SHIFT 12
#define VM_OC_PUT_DATA_MASK 0xf
#define VM_OC_PUT_DATA_CREATE_FLAG(V) \
  (((V) & VM_OC_PUT_DATA_MASK) << VM_OC_PUT_DATA_SHIFT)

enum {
  VM_OC_PUT_IDENT = VM_OC_PUT_DATA_CREATE_FLAG (0x1),
  VM_OC_PUT_REFERENCE = VM_OC_PUT_DATA_CREATE_FLAG (0x2),
  VM_OC_PUT_STACK = VM_OC_PUT_DATA_CREATE_FLAG (0x4),
  VM_OC_PUT_BLOCK = VM_OC_PUT_DATA_CREATE_FLAG (0x8),
};

#define VM_SET_CONTEXT_DESCRIPTOR(type, end) ((type) | (end) << 4)
#define VM_GET_CONTEXT_TYPE(value) ((value) & 0xf)

enum {
  VM_CONTEXT_TRY,                             /**< try context */
  VM_CONTEXT_CATCH,                           /**< catch context */
  VM_CONTEXT_FINALLY,                         /**< finally context */
};

#define VM_PLUS_EQUAL_U16(base, value) (base) = (uint16_t) ((base) + (value))
#define VM_MINUS_EQUAL_U16(base, value) (base) = (uint16_t) ((base) - (value))

extern void vm_init (const cbc_compiled_code_t *, bool);
extern void vm_finalize (void);
extern jerry_completion_code_t vm_run_global (void);
extern ecma_completion_value_t vm_run_eval (const cbc_compiled_code_t *, bool);

extern ecma_completion_value_t vm_loop (vm_frame_ctx_t *);
extern ecma_completion_value_t vm_run (const cbc_compiled_code_t *,
                                       ecma_value_t,
                                       ecma_object_t *,
                                       bool,
                                       bool);

extern opcode_scope_code_flags_t vm_get_scope_flags (const cbc_compiled_code_t *);

extern bool vm_is_strict_mode (void);
extern bool vm_is_direct_eval_form_call (void);

extern ecma_value_t vm_get_this_binding (void);
extern ecma_object_t *vm_get_lex_env (void);

#endif /* VM_H */
