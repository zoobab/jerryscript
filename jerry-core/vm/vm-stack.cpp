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

#include "vm-stack.h"

void vm_stack_init (void)
{
// FIXME: Implement this
}

void vm_stack_finalize (void)
{
// FIXME: Implement this
}

vm_stack_frame_t *vm_stack_get_top_frame (void)
{
  return NULL;
// FIXME: Implement this
}

void vm_stack_add_frame (vm_stack_frame_t *, ecma_value_t *, uint32_t, uint32_t, uint32_t, ecma_collection_header_t *)
{
// FIXME: Implement this
}

void vm_stack_free_frame (vm_stack_frame_t *)
{
// FIXME: Implement this
}

ecma_value_t vm_stack_frame_get_reg_value (vm_stack_frame_t *, uint32_t)
{
// FIXME: Implement this
}

void vm_stack_frame_set_reg_value (vm_stack_frame_t *, uint32_t, ecma_value_t)
{
// FIXME: Implement this
}

void vm_stack_push_value (vm_stack_frame_t *, ecma_value_t)
{
// FIXME: Implement this
}

ecma_value_t vm_stack_top_value (vm_stack_frame_t *)
{
// FIXME: Implement this
}

void vm_stack_pop (vm_stack_frame_t *)
{
// FIXME: Implement this
}

void vm_stack_pop_multiple (vm_stack_frame_t *, uint32_t)
{
// FIXME: Implement this
}
