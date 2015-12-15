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

#include "ecma-alloc.h"
#include "ecma-conversion.h"
#include "ecma-helpers.h"
#include "ecma-try-catch-macro.h"
#include "opcodes.h"

/**
 * Number bitwise logic operations.
 */
typedef enum
{
  number_bitwise_logic_and, /**< bitwise AND calculation */
  number_bitwise_logic_or, /**< bitwise OR calculation */
  number_bitwise_logic_xor, /**< bitwise XOR calculation */
  number_bitwise_shift_left, /**< bitwise LEFT SHIFT calculation */
  number_bitwise_shift_right, /**< bitwise RIGHT_SHIFT calculation */
  number_bitwise_shift_uright, /**< bitwise UNSIGNED RIGHT SHIFT calculation */
  number_bitwise_not, /**< bitwise NOT calculation */
} number_bitwise_logic_op;

/**
 * Perform ECMA number logic operation.
 *
 * The algorithm of the operation is following:
 *   leftNum = ToNumber (leftValue);
 *   rightNum = ToNumber (rightValue);
 *   result = leftNum BitwiseLogicOp rightNum;
 *
 * @return completion value
 *         Returned value must be freed with ecma_free_completion_value
 */
static ecma_completion_value_t
do_number_bitwise_logic (number_bitwise_logic_op op, /**< number bitwise logic operation */
                         ecma_value_t left_value, /**< left value */
                         ecma_value_t right_value) /** right value */
{
  ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

  ECMA_OP_TO_NUMBER_TRY_CATCH (num_left, left_value, ret_value);
  ECMA_OP_TO_NUMBER_TRY_CATCH (num_right, right_value, ret_value);

  ecma_number_t* res_p = ecma_alloc_number ();

  int32_t left_int32 = ecma_number_to_int32 (num_left);

  uint32_t left_uint32 = ecma_number_to_uint32 (num_left);
  uint32_t right_uint32 = ecma_number_to_uint32 (num_right);

  switch (op)
  {
    case number_bitwise_logic_and:
    {
      *res_p = ecma_int32_to_number ((int32_t) (left_uint32 & right_uint32));
      break;
    }
    case number_bitwise_logic_or:
    {
      *res_p = ecma_int32_to_number ((int32_t) (left_uint32 | right_uint32));
      break;
    }
    case number_bitwise_logic_xor:
    {
      *res_p = ecma_int32_to_number ((int32_t) (left_uint32 ^ right_uint32));
      break;
    }
    case number_bitwise_shift_left:
    {
      *res_p = ecma_int32_to_number (left_int32 << (right_uint32 & 0x1F));
      break;
    }
    case number_bitwise_shift_right:
    {
      *res_p = ecma_int32_to_number (left_int32 >> (right_uint32 & 0x1F));
      break;
    }
    case number_bitwise_shift_uright:
    {
      *res_p = ecma_uint32_to_number (left_uint32 >> (right_uint32 & 0x1F));
      break;
    }
    case number_bitwise_not:
    {
      *res_p = ecma_int32_to_number ((int32_t) ~right_uint32);
      break;
    }
  }

  ret_value = ecma_make_normal_completion_value (ecma_make_number_value (res_p));

  ECMA_OP_TO_NUMBER_FINALIZE (num_right);
  ECMA_OP_TO_NUMBER_FINALIZE (num_left);

  return ret_value;
} /* do_number_bitwise_logic */

/**
 * 'Bitwise AND' opcode handler.
 *
 * See also: ECMA-262 v5, 11.10
 *
 * @return completion value
 *         Returned value must be freed with ecma_free_completion_value
 */
ecma_completion_value_t
opfunc_b_and (ecma_value_t left_value, /**< left value */
              ecma_value_t right_value) /**< right value */
{
  ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

  ret_value = do_number_bitwise_logic (number_bitwise_logic_and,
                                       left_value,
                                       right_value);

  return ret_value;
} /* opfunc_b_and */

/**
 * 'Bitwise OR' opcode handler.
 *
 * See also: ECMA-262 v5, 11.10
 *
 * @return completion value
 *         Returned value must be freed with ecma_free_completion_value
 */
ecma_completion_value_t
opfunc_b_or (ecma_value_t left_value, /**< left value */
             ecma_value_t right_value) /**< right value */
{
  ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

  ret_value = do_number_bitwise_logic (number_bitwise_logic_or,
                                       left_value,
                                       right_value);

  return ret_value;
} /* opfunc_b_or */

/**
 * 'Bitwise XOR' opcode handler.
 *
 * See also: ECMA-262 v5, 11.10
 *
 * @return completion value
 *         Returned value must be freed with ecma_free_completion_value
 */
ecma_completion_value_t
opfunc_b_xor (ecma_value_t left_value, /**< left value */
              ecma_value_t right_value) /**< right value */
{
  ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

  ret_value = do_number_bitwise_logic (number_bitwise_logic_xor,
                                       left_value,
                                       right_value);

  return ret_value;
} /* opfunc_b_xor */

/**
 * 'Left Shift Operator' opcode handler.
 *
 * See also: ECMA-262 v5, 11.7.1
 *
 * @return completion value
 *         Returned value must be freed with ecma_free_completion_value
 */
ecma_completion_value_t
opfunc_b_shift_left (ecma_value_t left_value, /**< left value */
                     ecma_value_t right_value) /**< right value */
{
  ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

  ret_value = do_number_bitwise_logic (number_bitwise_shift_left,
                                       left_value,
                                       right_value);

  return ret_value;
} /* opfunc_b_shift_left */

/**
 * 'Right Shift Operator' opcode handler.
 *
 * See also: ECMA-262 v5, 11.7.2
 *
 * @return completion value
 *         Returned value must be freed with ecma_free_completion_value
 */
ecma_completion_value_t
opfunc_b_shift_right (ecma_value_t left_value, /**< left value */
                      ecma_value_t right_value) /**< right value */
{
  ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

  ret_value = do_number_bitwise_logic (number_bitwise_shift_right,
                                       left_value,
                                       right_value);

  return ret_value;
} /* opfunc_b_shift_right */

/**
 * 'Unsigned Right Shift Operator' opcode handler.
 *
 * See also: ECMA-262 v5, 11.7.3
 *
 * @return completion value
 *         Returned value must be freed with ecma_free_completion_value
 */
ecma_completion_value_t
opfunc_b_shift_uright (ecma_value_t left_value, /**< left value */
                       ecma_value_t right_value) /**< right value */
{
  ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

  ret_value = do_number_bitwise_logic (number_bitwise_shift_uright,
                                       left_value,
                                       right_value);

  return ret_value;
} /* opfunc_b_shift_uright */

/**
 * 'Bitwise NOT Operator' opcode handler.
 *
 * See also: ECMA-262 v5, 10.4
 *
 * @return completion value
 *         Returned value must be freed with ecma_free_completion_value
 */
ecma_completion_value_t
opfunc_b_not (ecma_value_t left_value) /**< left value */
{
  ecma_completion_value_t ret_value = ecma_make_empty_completion_value ();

  ret_value = do_number_bitwise_logic (number_bitwise_not,
                                       left_value,
                                       left_value);

  return ret_value;
} /* opfunc_b_not */
