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

#ifndef VM_STACK_H
#define VM_STACK_H

#include "config.h"
#include "ecma-globals.h"

/** \addtogroup vm Virtual machine
 * @{
 *
 * \addtogroup stack VM stack
 * @{
 */

extern void vm_stack_init (void);

extern void vm_spill_regs (uint32_t, uint32_t, uint32_t, ecma_collection_header_t *);

extern ecma_value_t vm_get_reg_value (uint32_t);
extern void vm_set_reg_value (uint32_t, ecma_value_t);

/**
 * @}
 * @}
 */

#endif /* !VM_STACK_H */
