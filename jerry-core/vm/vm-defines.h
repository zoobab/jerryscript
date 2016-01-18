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
#ifndef VM_DEFINES_H
#define VM_DEFINES_H

#include "byte-code.h"
#include "ecma-globals.h"

/**
 * Helpers for updating uint16_t values.
 */
#define VM_PLUS_EQUAL_U16(base, value) (base) = (uint16_t) ((base) + (value))
#define VM_MINUS_EQUAL_U16(base, value) (base) = (uint16_t) ((base) - (value))

/**
 * Instruction counter / position
 */
typedef const uint8_t *vm_instr_counter_t;

/**
 * Flags indicating various properties of a scope's code
 */
typedef enum : uint8_t
{
  OPCODE_SCOPE_CODE_FLAGS__EMPTY                       = (0u),      /**< initializer for empty flag set */
  OPCODE_SCOPE_CODE_FLAGS_STRICT                       = (1u << 0), /**< code is strict mode code */
  OPCODE_SCOPE_CODE_FLAGS_NOT_REF_ARGUMENTS_IDENTIFIER = (1u << 1), /**< code doesn't reference
                                                                     *   'arguments' identifier */
  OPCODE_SCOPE_CODE_FLAGS_NOT_REF_EVAL_IDENTIFIER      = (1u << 2)  /**< code doesn't reference
                                                                     *   'eval' identifier */
} opcode_scope_code_flags_t;

/**
 * Context of interpreter, related to a JS stack frame
 */
typedef struct
{
  const cbc_compiled_code_t *bytecode_header_p;       /**< currently executed byte-code data */
  uint8_t *byte_code_p;                               /**< current byte code pointer */
  uint8_t *byte_code_start_p;                         /**< byte code start pointer */
  ecma_value_t *registers_p;                          /**< register start pointer */
  lit_cpointer_t *literal_start_p;                    /**< literal list start pointer */
  ecma_object_t *lex_env_p;                           /**< current lexical environment */
  ecma_value_t this_binding;                          /**< this binding */
  uint16_t context_depth;                             /**< current context depth */
  bool is_eval_code;                                  /**< eval mode flag */
} vm_frame_ctx_t;

#endif /* VM_DEFINES_H */
