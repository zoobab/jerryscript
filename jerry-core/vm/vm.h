/* Copyright 2014-2016 Samsung Electronics Co., Ltd.
 * Copyright 2015-2016 University of Szeged.
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

typedef enum {
  VM_OC_GET_NONE = VM_OC_GET_DATA_CREATE_ID(0),
  VM_OC_GET_STACK = VM_OC_GET_DATA_CREATE_ID(1),
  VM_OC_GET_STACK_STACK = VM_OC_GET_DATA_CREATE_ID(2),
  VM_OC_GET_BYTE = VM_OC_GET_DATA_CREATE_ID(3),

  VM_OC_GET_LITERAL = VM_OC_GET_DATA_CREATE_ID(4),
  VM_OC_GET_STACK_LITERAL = VM_OC_GET_DATA_CREATE_ID(5),
  VM_OC_GET_LITERAL_BYTE = VM_OC_GET_DATA_CREATE_ID(6),
  VM_OC_GET_LITERAL_LITERAL = VM_OC_GET_DATA_CREATE_ID(7),
  VM_OC_GET_THIS_LITERAL = VM_OC_GET_DATA_CREATE_ID(8),
} vm_oc_get_types;

#define VM_OC_GROUP_MASK 0xff
#define VM_OC_GROUP_GET_INDEX(O) \
  ((O) & VM_OC_GROUP_MASK)

typedef enum {
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
  VM_OC_SET_PROPERTY,
  VM_OC_SET_GETTER,
  VM_OC_SET_SETTER,
  VM_OC_PUSH_UNDEFINED_BASE,
  VM_OC_PUSH_ARRAY,
  VM_OC_PUSH_ELISON,
  VM_OC_APPEND_ARRAY,
  VM_OC_IDENT_REFERENCE,
  VM_OC_PROP_REFERENCE,
  VM_OC_PROP_GET,

  /* These eight opcodes must be in this order. */
  VM_OC_PROP_PRE_INCR,
  VM_OC_PRE_INCR,
  VM_OC_PROP_PRE_DECR,
  VM_OC_PRE_DECR,
  VM_OC_PROP_POST_INCR,
  VM_OC_POST_INCR,
  VM_OC_PROP_POST_DECR,
  VM_OC_POST_DECR,

  VM_OC_PROP_DELETE,
  VM_OC_DELETE,

  VM_OC_ASSIGN,
  VM_OC_ASSIGN_PROP,
  VM_OC_ASSIGN_PROP_THIS,

  VM_OC_RET,
  VM_OC_THROW,

  /* The PROP forms must get the highest opcodes. */
  VM_OC_EVAL,
  VM_OC_CALL_N,
  VM_OC_CALL,
  VM_OC_CALL_PROP_N,
  VM_OC_CALL_PROP,

  VM_OC_NEW_N,
  VM_OC_NEW,

  VM_OC_JUMP,
  VM_OC_BRANCH_IF_STRICT_EQUAL,

  /* These four opcodes must be in this order. */
  VM_OC_BRANCH_IF_TRUE,
  VM_OC_BRANCH_IF_FALSE,
  VM_OC_BRANCH_IF_LOGICAL_TRUE,
  VM_OC_BRANCH_IF_LOGICAL_FALSE,

  VM_OC_PLUS,
  VM_OC_MINUS,
  VM_OC_NOT,
  VM_OC_BIT_NOT,
  VM_OC_VOID,
  VM_OC_TYPEOF_IDENT,
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

  VM_OC_WITH,
  VM_OC_FOR_IN_CREATE_CONTEXT,
  VM_OC_FOR_IN_GET_NEXT,
  VM_OC_FOR_IN_HAS_NEXT,
  VM_OC_TRY,
  VM_OC_CATCH,
  VM_OC_FINALLY,
  VM_OC_CONTEXT_END,
  VM_OC_JUMP_AND_EXIT_CONTEXT,
} vm_oc_types;

#define VM_OC_PUT_DATA_SHIFT 12
#define VM_OC_PUT_DATA_MASK 0xf
#define VM_OC_PUT_DATA_CREATE_FLAG(V) \
  (((V) & VM_OC_PUT_DATA_MASK) << VM_OC_PUT_DATA_SHIFT)

typedef enum {
  VM_OC_PUT_IDENT = VM_OC_PUT_DATA_CREATE_FLAG (0x1),
  VM_OC_PUT_REFERENCE = VM_OC_PUT_DATA_CREATE_FLAG (0x2),
  VM_OC_PUT_STACK = VM_OC_PUT_DATA_CREATE_FLAG (0x4),
  VM_OC_PUT_BLOCK = VM_OC_PUT_DATA_CREATE_FLAG (0x8),
} vm_oc_put_types;

extern void vm_init (const cbc_compiled_code_t *, bool);
extern void vm_finalize (void);
extern jerry_completion_code_t vm_run_global (void);
extern ecma_completion_value_t vm_run_eval (const cbc_compiled_code_t *, bool);

extern ecma_completion_value_t vm_loop (vm_frame_ctx_t *);
extern ecma_completion_value_t vm_run (const cbc_compiled_code_t *,
                                       ecma_value_t,
                                       ecma_object_t *,
                                       bool,
                                       ecma_collection_header_t *);

extern ecma_completion_value_t vm_run_array_args (const cbc_compiled_code_t *,
                                                  ecma_value_t,
                                                  ecma_object_t *,
                                                  bool,
                                                  const ecma_value_t*,
                                                  ecma_length_t);

extern bool vm_is_strict_mode (void);
extern bool vm_is_direct_eval_form_call (void);

#endif /* VM_H */
