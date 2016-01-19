/* Copyright 2015-2016 Samsung Electronics Co., Ltd.
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

#ifndef OPCODES_H
#define OPCODES_H

#include "ecma-globals.h"
#include "vm-defines.h"

typedef enum : uint8_t
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
vm_var_decl (vm_frame_ctx_t *, ecma_string_t *);

ecma_completion_value_t
opfunc_call_n (vm_frame_ctx_t *,
               ecma_value_t,
               ecma_value_t,
               const ecma_value_t *,
               ecma_length_t);

ecma_completion_value_t
opfunc_construct_n (ecma_value_t, uint8_t, ecma_value_t *);

ecma_completion_value_t
opfunc_equal_value (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_not_equal_value (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_equal_value_type (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_not_equal_value_type (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_addition (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_substraction (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_multiplication (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_division (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_remainder (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_unary_plus (ecma_value_t);

ecma_completion_value_t
opfunc_unary_minus (ecma_value_t);

ecma_completion_value_t
opfunc_b_or (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_b_xor (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_b_and (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_b_shift_left (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_b_shift_right (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_b_shift_uright (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_b_not (ecma_value_t);

ecma_completion_value_t
opfunc_less_than (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_greater_than (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_less_or_equal_than (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_greater_or_equal_than (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_in (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_instanceof (ecma_value_t, ecma_value_t);

ecma_completion_value_t
opfunc_logical_not (ecma_value_t);

ecma_completion_value_t
opfunc_typeof (ecma_value_t);

void
opfunc_set_accessor (bool, ecma_value_t, ecma_value_t, ecma_value_t);

ecma_completion_value_t
vm_op_delete_prop (ecma_value_t, ecma_value_t, bool);

ecma_completion_value_t
vm_op_delete_var (lit_cpointer_t, ecma_object_t *, bool);

ecma_collection_header_t *
opfunc_for_in (ecma_value_t);

#endif /* OPCODES_H */
