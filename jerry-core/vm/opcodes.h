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

#ifndef OPCODES_H
#define OPCODES_H

#include "ecma-globals.h"
#include "vm-defines.h"

typedef enum : vm_idx_t
{
  OPCODE_CALL_FLAGS__EMPTY                   = (0u),      /**< initializer for empty flag set */
  OPCODE_CALL_FLAGS_HAVE_THIS_ARG            = (1u << 0), /**< flag, indicating that call is performed
                                                           *   with 'this' argument specified */
  OPCODE_CALL_FLAGS_DIRECT_CALL_TO_EVAL_FORM = (1u << 1)  /**< flag, indicating that call is performed
                                                           *   in form 'eval (...)', i.e. through 'eval' string
                                                           *   without object base (i.e. with lexical environment
                                                           *   as base), so it can be a direct call to eval
                                                           *   See also: ECMA-262 v5, 15.1.2.1.1
                                                           */
} opcode_call_flags_t;

ecma_completion_value_t
opfunc_call_n (vm_frame_ctx_t *, ecma_value_t, uint32_t, ecma_value_t **);

#endif /* OPCODES_H */
