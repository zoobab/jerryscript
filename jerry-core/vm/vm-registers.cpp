/* Copyright 2015 Samsung Electronics Co., Ltd.
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

#include "ecma-globals.h"
#include "ecma-helpers.h"

/** \addtogroup vm Virtual machine
 * @{
 *
 * \addtogroup stack VM stack
 * @{
 */

ecma_value_t r[CONFIG_VM_REG_COUNT] = {0};

/**
 * Initialize stack
 */
void
vm_stack_init (void)
{
} /* vm_stack_init */

/**
 * Add the frame to stack
 */
void
vm_spill_regs (uint32_t regs_num __attr_unused___, /**< total number of register variables */
               uint32_t local_vars_regs_num __attr_unused___, /**< number of register variables,
                                                               *   used for local variables */
               uint32_t arg_regs_num __attr_unused___, /**< number of register variables,
                                                        *   used for arguments */
               ecma_collection_header_t *arguments_p __attr_unused___) /**< collection of arguments
                                                                        *   (for case, their values
                                                                        *    are moved to registers) */
{
  JERRY_ASSERT (regs_num >= VM_SPECIAL_REGS_NUMBER);

  for (uint32_t i = 0; i < regs_num - local_vars_regs_num - arg_regs_num; i++)
  {
    r[i] = ecma_make_simple_value (ECMA_SIMPLE_VALUE_EMPTY);
  }

  for (uint32_t i = regs_num - local_vars_regs_num - arg_regs_num;
       i < regs_num;
       i++)
  {
    r[i] = ecma_make_simple_value (ECMA_SIMPLE_VALUE_UNDEFINED);
  }

  if (arg_regs_num != 0)
  {
    ecma_collection_iterator_t args_iterator;
    ecma_collection_iterator_init (&args_iterator, arguments_p);

    for (uint32_t i = regs_num - arg_regs_num;
         i < regs_num && ecma_collection_iterator_next (&args_iterator);
         i++)
    {
      r[i] = ecma_copy_value (*args_iterator.current_value_p, false);
    }
  }
} /* vm_spill_regs */

/**
 * Get value of specified register variable
 *
 * @return ecma-value
 */
ecma_value_t __attr_always_inline___
vm_get_reg_value (uint32_t reg_index) /**< index of register variable */
{
  return r[reg_index - VM_REG_FIRST];
} /* vm_get_reg_value */

/**
 * Set value of specified register variable
 */
void __attr_always_inline___
vm_set_reg_value (uint32_t reg_index, /**< index of register variable */
                  ecma_value_t value) /**< ecma-value */
{
  r[reg_index - VM_REG_FIRST] = value;
} /* vm_set_reg_value */

/**
 * @}
 * @}
 */
