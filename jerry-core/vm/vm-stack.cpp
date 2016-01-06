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

#include "ecma-helpers.h"
#include "vm-stack.h"

/** \addtogroup vm Virtual machine
 * @{
 *
 * \addtogroup stack VM stack
 * @{
 */

/**
 * The top-most stack frame
 */
vm_stack_frame_t* vm_stack_top_frame_p = NULL;

/**
 * Get stack's top frame
 *
 * @return pointer to the top frame descriptor
 */
vm_stack_frame_t*
vm_stack_get_top_frame (void)
{
  return vm_stack_top_frame_p;
} /* vm_stack_get_top_frame */

/**
 * Add the frame to stack
 */
void
vm_stack_add_frame (vm_stack_frame_t *frame_p, /**< frame to initialize */
                    ecma_value_t *regs_p, /**< array of register variables' values */
                    uint32_t regs_num) /**< total number of register variables */
{
  frame_p->prev_frame_p = vm_stack_top_frame_p;
  vm_stack_top_frame_p = frame_p;

  frame_p->top_chunk_p = NULL;
  frame_p->dynamically_allocated_value_slots_p = frame_p->inlined_values;
  frame_p->current_slot_index = 0;
  frame_p->regs_p = regs_p;
  frame_p->regs_number = regs_num;
} /* vm_stack_add_frame */

/**
 * Free the stack frame
 *
 * Note:
 *      the frame should be the top-most frame
 */
void
vm_stack_free_frame (vm_stack_frame_t *frame_p) /**< frame to initialize */
{
  /* the frame should be the top-most frame */
  JERRY_ASSERT (vm_stack_top_frame_p == frame_p);

  vm_stack_top_frame_p = frame_p->prev_frame_p;

  while (frame_p->top_chunk_p != NULL)
  {
    vm_stack_pop (frame_p);
  }

  for (uint32_t reg_index = 0;
       reg_index < frame_p->regs_number;
       reg_index++)
  {
    ecma_free_value (frame_p->regs_p[reg_index], false);
  }
} /* vm_stack_free_frame */

/**
 * Get value of specified register variable
 *
 * @return ecma-value
 */
ecma_value_t
vm_stack_frame_get_reg_value (vm_stack_frame_t *frame_p, /**< frame */
                              uint32_t reg_index) /**< index of register variable */
{
  return frame_p->regs_p[reg_index];
} /* vm_stack_frame_get_reg_value */

/**
 * Set value of specified register variable
 */
void
vm_stack_frame_set_reg_value (vm_stack_frame_t *frame_p, /**< frame */
                              uint32_t reg_index, /**< index of register variable */
                              ecma_value_t value) /**< ecma-value */
{
  frame_p->regs_p[reg_index] = value;
} /* vm_stack_frame_set_reg_value */

/**
 * Push ecma-value to stack
 */
void
vm_stack_push_value (vm_stack_frame_t *frame_p, /**< stack frame */
                     ecma_value_t value) /**< ecma-value */
{
// FIXME: Implement this
} /* vm_stack_push_value */

/**
 * Get top value from stack
 */
ecma_value_t __attr_always_inline___
vm_stack_top_value (vm_stack_frame_t *frame_p) /**< stack frame */
{
// FIXME: Implement this
} /* vm_stack_top_value */

/**
 * Pop top value from stack and free it
 */
void
vm_stack_pop (vm_stack_frame_t *frame_p) /**< stack frame */
{
// FIXME: Implement this
} /* vm_stack_pop */

/**
 * @}
 * @}
 */
