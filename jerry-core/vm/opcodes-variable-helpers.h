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

#ifndef OPCODES_ECMA_SUPPORT_H
#define OPCODES_ECMA_SUPPORT_H

#include "ecma-globals.h"
#include "vm-defines.h"

bool vm_is_reg_variable (ecma_value_t);
ecma_completion_value_t get_variable_value (vm_frame_ctx_t *, ecma_value_t, bool);
ecma_completion_value_t set_variable_value (vm_frame_ctx_t *, ecma_string_t *var_name_str_p, ecma_value_t);
#endif /* OPCODES_ECMA_SUPPORT_H */
