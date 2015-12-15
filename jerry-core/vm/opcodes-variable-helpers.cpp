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

#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-lex-env.h"
#include "opcodes-variable-helpers.h"
#include "vm-defines.h"

/**
 * Check if the variable is register variable.
 *
 * @return true - if var_idx is a register variable,
 *         false - otherwise.
 */
bool
vm_is_reg_variable (ecma_value_t var_idx) /**< variable identifier */
{
  // FIXME: Implement this!
  return false;
} /* vm_is_reg_variable */

/**
 * Get variable's value.
 *
 * @return completion value
 *         Returned value must be freed with ecma_free_completion_value
 */
ecma_completion_value_t
get_variable_value (vm_frame_ctx_t *frame_ctx_p, /**< interpreter context */
                    ecma_value_t var_idx, /**< variable identifier */
                    bool do_eval_or_arguments_check) /** run 'strict eval or arguments reference' check
                                                          See also: do_strict_eval_arguments_check */
{
  ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

  return ret_value;
} /* get_variable_value */

/**
 * Set variable's value.
 *
 * @return completion value
 *         Returned value must be freed with ecma_free_completion_value
 */
ecma_completion_value_t
set_variable_value (vm_frame_ctx_t *frame_ctx_p, /**< interpreter context */
                    ecma_string_t *var_name_str_p, /**< variable name */
                    ecma_value_t value) /**< value to set */
{
  ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

  // FIXME: Handle register variables

  ret_value = ecma_op_put_value_lex_env_base (frame_ctx_p->ref_base_lex_env_p,
                                              var_name_str_p,
                                              frame_ctx_p->is_strict,
                                              value);

  return ret_value;
} /* set_variable_value */
