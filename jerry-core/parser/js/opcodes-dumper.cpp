/* Copyright 2014-2015 Samsung Electronics Co., Ltd.
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
#include "ecma-number-arithmetic.h"

#include "opcodes-dumper.h"

#include "serializer.h"
#include "stack.h"
#include "jsp-early-error.h"
#include "opcodes-native-call.h"

static vm_idx_t temp_name, max_temp_name;

enum
{
  U8_global_size
};
STATIC_STACK (U8, uint8_t)

enum
{
  varg_headers_global_size
};
STATIC_STACK (varg_headers, vm_instr_counter_t)

enum
{
  function_ends_global_size
};
STATIC_STACK (function_ends, vm_instr_counter_t)

enum
{
  logical_and_checks_global_size
};
STATIC_STACK (logical_and_checks, vm_instr_counter_t)

enum
{
  logical_or_checks_global_size
};
STATIC_STACK (logical_or_checks, vm_instr_counter_t)

enum
{
  conditional_checks_global_size
};
STATIC_STACK (conditional_checks, vm_instr_counter_t)

enum
{
  jumps_to_end_global_size
};
STATIC_STACK (jumps_to_end, vm_instr_counter_t)

enum
{
  prop_getters_global_size
};
STATIC_STACK (prop_getters, op_meta)

enum
{
  next_iterations_global_size
};
STATIC_STACK (next_iterations, vm_instr_counter_t)

enum
{
  case_clauses_global_size
};
STATIC_STACK (case_clauses, vm_instr_counter_t)

enum
{
  tries_global_size
};
STATIC_STACK (tries, vm_instr_counter_t)

enum
{
  catches_global_size
};
STATIC_STACK (catches, vm_instr_counter_t)

enum
{
  finallies_global_size
};
STATIC_STACK (finallies, vm_instr_counter_t)

enum
{
  temp_names_global_size
};
STATIC_STACK (temp_names, vm_idx_t)

enum
{
  reg_var_decls_global_size
};
STATIC_STACK (reg_var_decls, vm_instr_counter_t)

/**
 * Reset counter of register variables allocator
 * to identifier of first general register
 */
static void
reset_temp_name (void)
{
  temp_name = VM_REG_GENERAL_FIRST;
} /* reset_temp_name */

/**
 * Allocate next register variable
 *
 * @return identifier of the allocated variable
 */
static vm_idx_t
next_temp_name (void)
{
  vm_idx_t next_reg = temp_name++;

  if (next_reg > VM_REG_GENERAL_LAST)
  {
    /*
     * FIXME:
     *       Implement mechanism, allowing reusage of register variables
     */
    PARSE_ERROR (JSP_EARLY_ERROR_SYNTAX, "Not enough register variables", LIT_ITERATOR_POS_ZERO);
  }

  if (max_temp_name < next_reg)
  {
    max_temp_name = next_reg;
  }

  return next_reg;
} /* next_temp_name */

static op_meta
create_op_meta (vm_instr_t op, lit_cpointer_t lit_id1, lit_cpointer_t lit_id2, lit_cpointer_t lit_id3)
{
  op_meta ret;

  ret.op = op;
  ret.lit_id[0] = lit_id1;
  ret.lit_id[1] = lit_id2;
  ret.lit_id[2] = lit_id3;

  return ret;
}

static operand
tmp_operand (void)
{
  operand ret;

  ret.type = OPERAND_TMP;
  ret.uid = next_temp_name ();
  ret.lit_id = NOT_A_LITERAL;

  return ret;
}

operand
empty_operand (void)
{
  operand ret;

  ret.type = OPERAND_TMP;
  ret.uid = VM_IDX_EMPTY;
  ret.lit_id = NOT_A_LITERAL;

  return ret;
}

bool
operand_is_empty (operand op)
{
  return op.type == OPERAND_TMP && op.uid == VM_IDX_EMPTY && op.lit_id == NOT_A_LITERAL;
}

bool
operand_is_reference (operand op)
{
  JERRY_ASSERT (!operand_is_empty (op));

  /*
   * TODO:
   *      Implement and add object-based reference operand type
   */
  return (op.type == OPERAND_IDENTIFIER);
}

static bool
operand_is_boolean (operand op)
{
  JERRY_ASSERT (!operand_is_empty (op));

  return (op.type == OPERAND_SIMPLE
          && (op.uid == ECMA_SIMPLE_VALUE_TRUE
              || op.uid == ECMA_SIMPLE_VALUE_FALSE));
}

bool
operand_is_number (operand op)
{
  JERRY_ASSERT (!operand_is_empty (op));

  return (op.type == OPERAND_NUMBER);
}

bool
operand_is_constant (operand op)
{
  JERRY_ASSERT (!operand_is_empty (op));

  return (op.type == OPERAND_SIMPLE
          || op.type == OPERAND_INTEGER_CONST
          || op.type == OPERAND_STRING
          || op.type == OPERAND_NUMBER);
}

static bool
operand_get_boolean (operand op)
{
  JERRY_ASSERT (operand_is_boolean (op));

  return (op.uid == ECMA_SIMPLE_VALUE_TRUE);
}

ecma_number_t
operand_get_number (operand op)
{
  JERRY_ASSERT (operand_is_number (op));

  literal_t lit = lit_get_literal_by_cp (op.lit_id);
  JERRY_ASSERT (lit_literal_is_num (lit));

  return lit_charset_literal_get_number (lit);
}

operand
identifier_operand (lit_cpointer_t lit_cp)
{
  operand ret;

  ret.type = OPERAND_IDENTIFIER;
  ret.uid = VM_IDX_REWRITE_LITERAL_UID;
  ret.lit_id = lit_cp;

  return ret;
}

operand
string_operand (lit_cpointer_t lit_cp)
{
  operand ret;

  ret.type = OPERAND_STRING;
  ret.uid = VM_IDX_REWRITE_LITERAL_UID;
  ret.lit_id = lit_cp;

  return ret;
}

static operand
int_const_operand (vm_idx_t value)
{
  operand ret;

  /*
   * Suppressing 'comparison always true due to limited range of data type' warning.
   *
   * Comparison can be always true, if VM_IDX_GENERAL_VALUE_FIRST == 0, but this can be changed.
   */
  int32_t signed_value = value;
  JERRY_ASSERT (signed_value >= VM_IDX_GENERAL_VALUE_FIRST && signed_value <= VM_IDX_GENERAL_VALUE_LAST);

  ret.type = OPERAND_INTEGER_CONST;
  ret.uid = value;
  ret.lit_id = NOT_A_LITERAL;

  return ret;
}

operand
null_operand (void)
{
  operand ret;

  ret.type = OPERAND_SIMPLE;
  ret.uid = ECMA_SIMPLE_VALUE_NULL;
  ret.lit_id = NOT_A_LITERAL;

  return ret;
}

operand
bool_operand (bool value)
{
  operand ret;

  ret.type = OPERAND_SIMPLE;
  ret.uid = value ? ECMA_SIMPLE_VALUE_TRUE : ECMA_SIMPLE_VALUE_FALSE;
  ret.lit_id = NOT_A_LITERAL;

  return ret;
}

operand
number_operand (lit_cpointer_t lit_cp)
{
  operand ret;

  ret.type = OPERAND_NUMBER;
  ret.uid = VM_IDX_REWRITE_LITERAL_UID;
  ret.lit_id = lit_cp;

  return ret;
}

static operand
create_operand_from_tmp_and_lit (vm_idx_t tmp, lit_cpointer_t lit_id)
{
  if (tmp != VM_IDX_REWRITE_LITERAL_UID)
  {
    JERRY_ASSERT (lit_id.packed_value == MEM_CP_NULL);

    operand ret;

    ret.type = OPERAND_TMP;
    ret.uid = tmp;
    ret.lit_id = NOT_A_LITERAL;

    return ret;
  }
  else
  {
    JERRY_ASSERT (lit_id.packed_value != MEM_CP_NULL);
    JERRY_ASSERT (lit_literal_is_utf8_string (lit_get_literal_by_cp (lit_id)));

    operand ret;

    ret.type = OPERAND_IDENTIFIER;
    ret.uid = VM_IDX_REWRITE_LITERAL_UID;
    ret.lit_id = lit_id;

    return ret;
  }
}

/**
 * Creates operand for eval's return value
 *
 * @return constructed operand
 */
operand
eval_ret_operand (void)
{
  operand ret;

  ret.type = OPERAND_TMP;
  ret.uid = EVAL_RET_VALUE;
  ret.lit_id = NOT_A_LITERAL;

  return ret;
} /* eval_ret_operand */

/**
 * Creates operand for taking iterator value (next property name)
 * from for-in opcode handler.
 *
 * @return constructed operand
 */
operand
jsp_create_operand_for_in_special_reg (void)
{
  operand ret;

  ret.type = OPERAND_TMP;
  ret.uid = VM_REG_SPECIAL_FOR_IN_PROPERTY_NAME;
  ret.lit_id = NOT_A_LITERAL;

  return ret;
} /* jsp_create_operand_for_in_special_reg */

static uint8_t
name_to_native_call_id (operand obj)
{
  if (obj.type != OPERAND_IDENTIFIER)
  {
    return OPCODE_NATIVE_CALL__COUNT;
  }
  if (lit_literal_equal_type_cstr (lit_get_literal_by_cp (obj.lit_id), "LEDToggle"))
  {
    return OPCODE_NATIVE_CALL_LED_TOGGLE;
  }
  else if (lit_literal_equal_type_cstr (lit_get_literal_by_cp (obj.lit_id), "LEDOn"))
  {
    return OPCODE_NATIVE_CALL_LED_ON;
  }
  else if (lit_literal_equal_type_cstr (lit_get_literal_by_cp (obj.lit_id), "LEDOff"))
  {
    return OPCODE_NATIVE_CALL_LED_OFF;
  }
  else if (lit_literal_equal_type_cstr (lit_get_literal_by_cp (obj.lit_id), "LEDOnce"))
  {
    return OPCODE_NATIVE_CALL_LED_ONCE;
  }
  else if (lit_literal_equal_type_cstr (lit_get_literal_by_cp (obj.lit_id), "wait"))
  {
    return OPCODE_NATIVE_CALL_WAIT;
  }
  else if (lit_literal_equal_type_cstr (lit_get_literal_by_cp (obj.lit_id), "print"))
  {
    return OPCODE_NATIVE_CALL_PRINT;
  }
  return OPCODE_NATIVE_CALL__COUNT;
}

static bool
is_native_call (operand obj)
{
  return name_to_native_call_id (obj) < OPCODE_NATIVE_CALL__COUNT;
}

static void
split_instr_counter (vm_instr_counter_t oc, vm_idx_t *id1, vm_idx_t *id2)
{
  JERRY_ASSERT (id1 != NULL);
  JERRY_ASSERT (id2 != NULL);
  *id1 = (vm_idx_t) (oc >> JERRY_BITSINBYTE);
  *id2 = (vm_idx_t) (oc & ((1 << JERRY_BITSINBYTE) - 1));
  JERRY_ASSERT (oc == vm_calc_instr_counter_from_idx_idx (*id1, *id2));
}

static op_meta
last_dumped_op_meta (void)
{
  return serializer_get_op_meta ((vm_instr_counter_t) (serializer_get_current_instruction_counter () - 1));
}

static void
dump_instruction (vm_op_t opcode,
                  const operand ops [],
                  uint32_t ops_num)
{
  if (ops_num == 0)
  {
    vm_assert_opcode_args_num_0 (opcode);
  }
  else if (ops_num == 1)
  {
    vm_assert_opcode_args_num_1 (opcode);
  }
  else if (ops_num == 2)
  {
    vm_assert_opcode_args_num_2 (opcode);
  }
  else
  {
    JERRY_ASSERT (ops_num == 3);

    vm_assert_opcode_args_num_3 (opcode);
  }

  vm_instr_t instr;

  /*
   * FIXME:
   *       Some instructions can contain an argument, describing type of another argument.
   *       For the instructions, the two arguments should be updated correspondingly.
   *
   *       Currently, there is no general mechanism for correct handling of the cases.
   *       Information about the cases should be somehow encoded in vm-opcodes.inc.h,
   *       maybe using something like 'type of next argument' argument type.
   *
   *       List of the instructions:
   *         - assignment (see also dump_variable_assignment).
   */

  vm_idx_t instruction_args[3] = { VM_IDX_EMPTY, VM_IDX_EMPTY, VM_IDX_EMPTY };
  lit_cpointer_t lit_ids[3] = { NOT_A_LITERAL, NOT_A_LITERAL, NOT_A_LITERAL };

  for (uint32_t i = 0; i < ops_num; i++)
  {
    vm_op_arg_type_t type_mask = vm_get_opcode_type_mask_for_arg (opcode, i);

    if (operand_is_empty (ops[i]))
    {
      JERRY_ASSERT (type_mask & VM_OP_ARG_TYPE_EMPTY);
    }
    else
    {
      switch (ops[i].type)
      {
        case OPERAND_IDENTIFIER:
        {
          JERRY_ASSERT (type_mask & VM_OP_ARG_TYPE_IDENTIFIER);

          instruction_args[i] = ops[i].uid;
          lit_ids[i] = ops[i].lit_id;
          break;
        }
        case OPERAND_NUMBER:
        {
          operand op = ops[i];
          if (!(type_mask & VM_OP_ARG_TYPE_NUMBER))
          {
            JERRY_ASSERT (type_mask & VM_OP_ARG_TYPE_REGISTER);

            op = dump_number_assignment_res (op.lit_id);
          }

          instruction_args[i] = op.uid;
          lit_ids[i] = op.lit_id;
          break;
        }
        case OPERAND_STRING:
        {
          operand op = ops[i];
          if (!(type_mask & VM_OP_ARG_TYPE_STRING))
          {
            JERRY_ASSERT (type_mask & VM_OP_ARG_TYPE_REGISTER);

            op = dump_string_assignment_res (op.lit_id);
          }

          instruction_args[i] = op.uid;
          lit_ids[i] = op.lit_id;
          break;
        }
        case OPERAND_SIMPLE:
        {
          operand op = ops[i];
          if (!(type_mask & VM_OP_ARG_TYPE_INTEGER_CONST))
          {
            JERRY_ASSERT (type_mask & VM_OP_ARG_TYPE_REGISTER);

            ecma_simple_value_t simple_value = (ecma_simple_value_t) op.uid;

            if (simple_value == ECMA_SIMPLE_VALUE_NULL)
            {
              op = dump_null_assignment_res ();
            }
            else
            {
              JERRY_ASSERT (simple_value == ECMA_SIMPLE_VALUE_TRUE
                            || simple_value == ECMA_SIMPLE_VALUE_FALSE);

              op = dump_boolean_assignment_res (simple_value == ECMA_SIMPLE_VALUE_TRUE);
            }
          }

          instruction_args[i] = op.uid;
          lit_ids[i] = NOT_A_LITERAL;
          break;
        }
        case OPERAND_INTEGER_CONST:
        {
          JERRY_ASSERT (type_mask & VM_OP_ARG_TYPE_INTEGER_CONST);

          instruction_args[i] = ops[i].uid;
          lit_ids[i] = NOT_A_LITERAL;
          break;
        }
        case OPERAND_TMP:
        {
          JERRY_ASSERT (type_mask & VM_OP_ARG_TYPE_REGISTER);

          instruction_args[i] = ops[i].uid;
          lit_ids[i] = NOT_A_LITERAL;
          break;
        }
      }
    }
  }

  instr.op_idx = opcode;
  instr.args_set.arg1 = instruction_args[0];
  instr.args_set.arg2 = instruction_args[1];
  instr.args_set.arg3 = instruction_args[2];

  serializer_dump_op_meta (create_op_meta (instr, lit_ids[0], lit_ids[1], lit_ids[2]));
}

static void
dump_no_args (vm_op_t opcode)
{
  dump_instruction (opcode, NULL, 0);
}

static void
dump_single_address (vm_op_t opcode, operand op)
{
  dump_instruction (opcode, &op, 1);
}

static void
dump_double_address (vm_op_t opcode, operand res, operand obj)
{
  operand ops[2] = { res, obj };
  dump_instruction (opcode, ops, 2);
}

static void
dump_triple_address (vm_op_t opcode, operand res, operand lhs, operand rhs)
{
  operand ops[3] = { res, lhs, rhs };
  dump_instruction (opcode, ops, 3);
}

static bool
dumper_try_calculate_if_const (vm_op_t opcode,
                               operand first_arg,
                               operand second_arg,
                               operand *out_operand_p)
{
  JERRY_ASSERT (out_operand_p != NULL);

  if (opcode == VM_OP_B_SHIFT_LEFT
      || opcode == VM_OP_B_SHIFT_RIGHT
      || opcode == VM_OP_B_SHIFT_URIGHT
      || opcode == VM_OP_B_AND
      || opcode == VM_OP_B_OR
      || opcode == VM_OP_B_XOR
      || opcode == VM_OP_ADDITION
      || opcode == VM_OP_SUBSTRACTION
      || opcode == VM_OP_MULTIPLICATION
      || opcode == VM_OP_DIVISION
      || opcode == VM_OP_REMAINDER)
  {
    JERRY_ASSERT (!operand_is_empty (first_arg));
    JERRY_ASSERT (!operand_is_empty (second_arg));

    if (operand_is_number (first_arg)
        && operand_is_number (second_arg))
    {
      ecma_number_t ret_num;

      /*
       * TODO:
       *      Combine with do_number_bitwise_logic and do_number_arithmetic
       */
      ecma_number_t num_left = operand_get_number (first_arg);
      ecma_number_t num_right = operand_get_number (second_arg);

      int32_t left_int32 = ecma_number_to_int32 (num_left);
      // int32_t right_int32 = ecma_number_to_int32 (num_right);

      uint32_t left_uint32 = ecma_number_to_uint32 (num_left);
      uint32_t right_uint32 = ecma_number_to_uint32 (num_right);

      if (opcode == VM_OP_B_SHIFT_LEFT)
      {
        ret_num = ecma_int32_to_number (left_int32 << (right_uint32 & 0x1F));
      }
      else if (opcode == VM_OP_B_SHIFT_RIGHT)
      {
        ret_num = ecma_int32_to_number (left_int32 >> (right_uint32 & 0x1F));
      }
      else if (opcode == VM_OP_B_SHIFT_URIGHT)
      {
        ret_num = ecma_uint32_to_number (left_uint32 >> (right_uint32 & 0x1F));
      }
      else if (opcode == VM_OP_B_AND)
      {
        ret_num = ecma_int32_to_number ((int32_t) (left_uint32 & right_uint32));
      }
      else if (opcode == VM_OP_B_OR)
      {
        ret_num = ecma_int32_to_number ((int32_t) (left_uint32 | right_uint32));
      }
      else if (opcode == VM_OP_B_XOR)
      {
        ret_num = ecma_int32_to_number ((int32_t) (left_uint32 ^ right_uint32));
      }
      else if (opcode == VM_OP_ADDITION)
      {
        ret_num = ecma_number_add (num_left, num_right);
      }
      else if (opcode == VM_OP_SUBSTRACTION)
      {
        ret_num = ecma_number_substract (num_left, num_right);
      }
      else if (opcode == VM_OP_MULTIPLICATION)
      {
        ret_num = ecma_number_multiply (num_left, num_right);
      }
      else if (opcode == VM_OP_DIVISION)
      {
        ret_num = ecma_number_divide (num_left, num_right);
      }
      else
      {
        JERRY_ASSERT (opcode == VM_OP_REMAINDER);

        ret_num = ecma_op_number_remainder (num_left, num_right);
      }

      literal_t ret_num_lit = lit_find_or_create_literal_from_num (ret_num);

      *out_operand_p = number_operand (lit_cpointer_t::compress (ret_num_lit));

      return true;
    }
  }
  else if (opcode == VM_OP_LESS_THAN
           || opcode == VM_OP_GREATER_THAN
           || opcode == VM_OP_GREATER_OR_EQUAL_THAN
           || opcode == VM_OP_LESS_OR_EQUAL_THAN
           || opcode == VM_OP_EQUAL_VALUE
           || opcode == VM_OP_NOT_EQUAL_VALUE
           || opcode == VM_OP_EQUAL_VALUE_TYPE
           || opcode == VM_OP_NOT_EQUAL_VALUE_TYPE)
  {
    JERRY_ASSERT (!operand_is_empty (first_arg));
    JERRY_ASSERT (!operand_is_empty (second_arg));

    if (operand_is_number (first_arg)
        && operand_is_number (second_arg))
    {
      ecma_number_t num_left = operand_get_number (first_arg);
      ecma_number_t num_right = operand_get_number (second_arg);

      if (!ecma_number_is_nan (num_left)
          && !ecma_number_is_nan (num_right))
      {
        bool is_true;

        if (opcode == VM_OP_LESS_THAN)
        {
          is_true = (num_left < num_right);
        }
        else if (opcode == VM_OP_GREATER_THAN)
        {
          is_true = (num_right < num_left);
        }
        else if (opcode == VM_OP_GREATER_OR_EQUAL_THAN)
        {
          is_true = !(num_left < num_right);
        }
        else if (opcode == VM_OP_LESS_OR_EQUAL_THAN)
        {
          is_true = !(num_right < num_left);
        }
        else if (opcode == VM_OP_EQUAL_VALUE
                 || opcode == VM_OP_EQUAL_VALUE_TYPE)
        {
          is_true = (num_left == num_right);
        }
        else
        {
          JERRY_ASSERT (opcode == VM_OP_NOT_EQUAL_VALUE
                        || opcode == VM_OP_NOT_EQUAL_VALUE_TYPE);

          is_true = !(num_left == num_right);
        }

        *out_operand_p = bool_operand ((is_true));

        return true;
      }
    }
  }
  else if (opcode == VM_OP_B_NOT
           || opcode == VM_OP_UNARY_PLUS
           || opcode == VM_OP_UNARY_MINUS)
  {
    JERRY_ASSERT (!operand_is_empty (first_arg));
    JERRY_ASSERT (operand_is_empty (second_arg));

    if (operand_is_number (first_arg))
    {
      ecma_number_t ret_num;

      ecma_number_t num = operand_get_number (first_arg);

      if (opcode == VM_OP_B_NOT)
      {
        uint32_t num_uint32 = ecma_number_to_uint32 (num);
        ret_num = ecma_int32_to_number ((int32_t) ~num_uint32);
      }
      else if (opcode == VM_OP_UNARY_PLUS)
      {
        ret_num = num;
      }
      else
      {
        JERRY_ASSERT (opcode == VM_OP_UNARY_MINUS);

        ret_num = ecma_number_negate (num);
      }

      literal_t ret_num_lit = lit_find_or_create_literal_from_num (ret_num);

      *out_operand_p = number_operand (lit_cpointer_t::compress (ret_num_lit));

      return true;
    }
  }
  else if (opcode == VM_OP_LOGICAL_NOT)
  {
    JERRY_ASSERT (!operand_is_empty (first_arg));
    JERRY_ASSERT (operand_is_empty (second_arg));

    if (operand_is_number (first_arg))
    {
      ecma_number_t num = operand_get_number (first_arg);

      bool value = !(ecma_number_is_nan (num)
                     || ecma_number_is_zero (num));

      *out_operand_p = bool_operand (!value);

      return true;
    }
    else if (operand_is_boolean (first_arg))
    {
      bool value = operand_get_boolean (first_arg);

      *out_operand_p = bool_operand (!value);

      return true;
    }
  }

  return false;
}

static operand
dump_double_address_res (vm_op_t opcode, operand rhs)
{
  operand res;
  if (dumper_try_calculate_if_const (opcode, rhs, empty_operand (), &res))
  {
    return res;
  }
  else
  {
    res = tmp_operand ();
    dump_double_address (opcode, res, rhs);
    return res;
  }
}

static operand
dump_triple_address_res (vm_op_t opcode, operand lhs, operand rhs)
{
  operand res;
  if (dumper_try_calculate_if_const (opcode, lhs, rhs, &res))
  {
    return res;
  }
  else
  {
    res = tmp_operand ();
    dump_triple_address (opcode, res, lhs, rhs);
    return res;
  }
}

static void
dump_prop_setter_op_meta (op_meta last, operand op)
{
  JERRY_ASSERT (last.op.op_idx == VM_OP_PROP_GETTER);

  operand obj = create_operand_from_tmp_and_lit (last.op.data.prop_getter.obj,
                                                 last.lit_id[1]);
  operand prop = create_operand_from_tmp_and_lit (last.op.data.prop_getter.prop,
                                                  last.lit_id[2]);
  dump_triple_address (VM_OP_PROP_SETTER, obj, prop, op);
}

static operand
dump_triple_address_and_prop_setter_res (vm_op_t opcode, op_meta last, operand op)
{
  JERRY_ASSERT (last.op.op_idx == VM_OP_PROP_GETTER);
  const operand obj = create_operand_from_tmp_and_lit (last.op.data.prop_getter.obj, last.lit_id[1]);
  const operand prop = create_operand_from_tmp_and_lit (last.op.data.prop_getter.prop, last.lit_id[2]);
  const operand tmp = dump_prop_getter_res (obj, prop);
  dump_triple_address (opcode, tmp, tmp, op);
  dump_prop_setter (obj, prop, tmp);
  return tmp;
}

static operand
dump_prop_setter_or_triple_address_res (vm_op_t opcode, operand res, operand op)
{
  const op_meta last = STACK_TOP (prop_getters);
  if (last.op.op_idx == VM_OP_PROP_GETTER)
  {
    res = dump_triple_address_and_prop_setter_res (opcode, last, op);
  }
  else
  {
    if (res.type == OPERAND_TMP)
    {
      /*
       * FIXME:
       *       Implement correct handling of references through parser operands
       */
      PARSE_ERROR (JSP_EARLY_ERROR_REFERENCE, "Invalid left-hand-side expression", LIT_ITERATOR_POS_ZERO);
    }

    dump_triple_address (opcode, res, res, op);
  }
  STACK_DROP (prop_getters, 1);
  return res;
}

static void
dump_op_meta_for_vlt (varg_list_type vlt, operand *res, operand *obj)
{
  switch (vlt)
  {
    case VARG_FUNC_EXPR:
    {
      vm_instr_t instr;

      instr.op_idx = VM_OP_FUNC_EXPR_N;
      instr.args_set.arg1 = res->uid;
      instr.args_set.arg2 = obj->uid;
      instr.args_set.arg3 = VM_IDX_REWRITE_GENERAL_CASE;

      serializer_dump_op_meta (create_op_meta (instr, res->lit_id, obj->lit_id, NOT_A_LITERAL));
      break;
    }
    case VARG_CONSTRUCT_EXPR:
    {
      vm_instr_t instr;

      instr.op_idx = VM_OP_CONSTRUCT_N;
      instr.args_set.arg1 = res->uid;
      instr.args_set.arg2 = obj->uid;
      instr.args_set.arg3 = VM_IDX_REWRITE_GENERAL_CASE;

      serializer_dump_op_meta (create_op_meta (instr, res->lit_id, obj->lit_id, NOT_A_LITERAL));
      break;
    }
    case VARG_CALL_EXPR:
    {
      JERRY_ASSERT (obj != NULL);
      if (is_native_call (*obj))
      {
        uint8_t native_call_id = name_to_native_call_id (*obj);

        vm_instr_t instr;

        instr.op_idx = VM_OP_NATIVE_CALL;
        instr.args_set.arg1 = res->uid;
        instr.args_set.arg2 = native_call_id;
        instr.args_set.arg3 = VM_IDX_REWRITE_GENERAL_CASE;

        serializer_dump_op_meta (create_op_meta (instr, res->lit_id, NOT_A_LITERAL, NOT_A_LITERAL));
      }
      else
      {
        vm_instr_t instr;

        instr.op_idx = VM_OP_CALL_N;
        instr.args_set.arg1 = res->uid;
        instr.args_set.arg2 = obj->uid;
        instr.args_set.arg3 = VM_IDX_REWRITE_GENERAL_CASE;

        serializer_dump_op_meta (create_op_meta (instr, res->lit_id, obj->lit_id, NOT_A_LITERAL));
      }
      break;
    }
    case VARG_FUNC_DECL:
    {
      JERRY_ASSERT (res == NULL);

      vm_instr_t instr;

      instr.op_idx = VM_OP_FUNC_DECL_N;
      instr.args_set.arg1 = obj->uid;
      instr.args_set.arg2 = VM_IDX_REWRITE_GENERAL_CASE;
      instr.args_set.arg3 = VM_IDX_EMPTY;

      serializer_dump_op_meta (create_op_meta (instr, obj->lit_id, NOT_A_LITERAL, NOT_A_LITERAL));
      break;
    }
    case VARG_ARRAY_DECL:
    {
      JERRY_ASSERT (obj == NULL);

      vm_instr_t instr;

      instr.op_idx = VM_OP_ARRAY_DECL;
      instr.args_set.arg1 = res->uid;
      instr.args_set.arg2 = VM_IDX_REWRITE_GENERAL_CASE;
      instr.args_set.arg3 = VM_IDX_EMPTY;

      serializer_dump_op_meta (create_op_meta (instr, res->lit_id, NOT_A_LITERAL, NOT_A_LITERAL));
      break;
    }
    case VARG_OBJ_DECL:
    {
      JERRY_ASSERT (obj == NULL);

      vm_instr_t instr;

      instr.op_idx = VM_OP_OBJ_DECL;
      instr.args_set.arg1 = res->uid;
      instr.args_set.arg2 = VM_IDX_REWRITE_GENERAL_CASE;
      instr.args_set.arg3 = VM_IDX_EMPTY;

      serializer_dump_op_meta (create_op_meta (instr, res->lit_id, NOT_A_LITERAL, NOT_A_LITERAL));
      break;
    }
  }
}

static vm_instr_counter_t
get_diff_from (vm_instr_counter_t oc)
{
  return (vm_instr_counter_t) (serializer_get_current_instruction_counter () - oc);
}

void
dumper_new_statement (void)
{
  reset_temp_name ();
}

void
dumper_new_scope (void)
{
  STACK_PUSH (temp_names, temp_name);
  STACK_PUSH (temp_names, max_temp_name);
  reset_temp_name ();
  max_temp_name = temp_name;
}

void
dumper_finish_scope (void)
{
  max_temp_name = STACK_TOP (temp_names);
  STACK_DROP (temp_names, 1);
  temp_name = STACK_TOP (temp_names);
  STACK_DROP (temp_names, 1);
}

/**
 * Handle start of argument preparation instruction sequence generation
 *
 * Note:
 *      Values of registers, allocated for the code sequence, are not used outside of the sequence,
 *      so they can be reused, reducing register pressure.
 *
 *      To reuse the registers, counter of register allocator is saved, and restored then,
 *      after finishing generation of the code sequence, using dumper_finish_varg_code_sequence.
 *
 * FIXME:
 *       Implement general register allocation mechanism
 *
 * See also:
 *          dumper_finish_varg_code_sequence
 */
void
dumper_start_varg_code_sequence (void)
{
  STACK_PUSH (temp_names, temp_name);
} /* dumper_start_varg_code_sequence */

/**
 * Handle finish of argument preparation instruction sequence generation
 *
 * See also:
 *          dumper_start_varg_code_sequence
 */
void
dumper_finish_varg_code_sequence (void)
{
  temp_name = STACK_TOP (temp_names);
  STACK_DROP (temp_names, 1);
} /* dumper_finish_varg_code_sequence */

/**
 * Check that byte-code operand refers to 'eval' string
 *
 * @return true - if specified byte-code operand's type is literal, and value of corresponding
 *                literal is equal to LIT_MAGIC_STRING_EVAL string,
 *         false - otherwise.
 */
bool
dumper_is_eval_literal (operand obj) /**< byte-code operand */
{
  /*
   * FIXME: Switch to corresponding magic string
   */
  bool is_eval_lit = (obj.type == OPERAND_IDENTIFIER
                      && lit_literal_equal_type_cstr (lit_get_literal_by_cp (obj.lit_id), "eval"));

  return is_eval_lit;
} /* dumper_is_eval_literal */

operand
dump_boolean_assignment_res (bool is_true)
{
  return dump_triple_address_res (VM_OP_ASSIGNMENT,
                                  int_const_operand (VM_OP_ASSIGNMENT_VAL_TYPE_SIMPLE),
                                  int_const_operand (is_true ? ECMA_SIMPLE_VALUE_TRUE : ECMA_SIMPLE_VALUE_FALSE));
}

operand
dump_string_assignment_res (lit_cpointer_t lit_id)
{
  return dump_triple_address_res (VM_OP_ASSIGNMENT,
                                  int_const_operand (VM_OP_ASSIGNMENT_VAL_TYPE_STRING),
                                  string_operand (lit_id));
}

operand
dump_number_assignment_res (lit_cpointer_t lit_id)
{
  return dump_triple_address_res (VM_OP_ASSIGNMENT,
                                  int_const_operand (VM_OP_ASSIGNMENT_VAL_TYPE_NUMBER),
                                  number_operand (lit_id));
}

operand
dump_regexp_assignment_res (lit_cpointer_t lit_id)
{
  return dump_triple_address_res (VM_OP_ASSIGNMENT,
                                  int_const_operand (VM_OP_ASSIGNMENT_VAL_TYPE_REGEXP),
                                  string_operand (lit_id));
}

void
dump_undefined_assignment (operand op)
{
  dump_triple_address (VM_OP_ASSIGNMENT,
                       op,
                       int_const_operand (VM_OP_ASSIGNMENT_VAL_TYPE_SIMPLE),
                       int_const_operand (ECMA_SIMPLE_VALUE_UNDEFINED));
}

operand
dump_undefined_assignment_res (void)
{
  operand op = tmp_operand ();
  dump_undefined_assignment (op);
  return op;
}

operand
dump_null_assignment_res (void)
{
  return dump_triple_address_res (VM_OP_ASSIGNMENT,
                                  int_const_operand (VM_OP_ASSIGNMENT_VAL_TYPE_SIMPLE),
                                  int_const_operand (ECMA_SIMPLE_VALUE_NULL));
}

void
dump_variable_assignment (operand res, operand var)
{
  vm_op_assignment_val_type_t assignment_type;

  if (var.type == OPERAND_TMP
      || var.type == OPERAND_IDENTIFIER)
  {
    assignment_type = VM_OP_ASSIGNMENT_VAL_TYPE_VARIABLE;
  }
  else if (var.type == OPERAND_STRING)
  {
    assignment_type = VM_OP_ASSIGNMENT_VAL_TYPE_STRING;
  }
  else if (var.type == OPERAND_NUMBER)
  {
    assignment_type = VM_OP_ASSIGNMENT_VAL_TYPE_NUMBER;
  }
  else
  {
    JERRY_ASSERT (var.type == OPERAND_SIMPLE);

    assignment_type = VM_OP_ASSIGNMENT_VAL_TYPE_SIMPLE;
  }

  dump_triple_address (VM_OP_ASSIGNMENT,
                       res,
                       int_const_operand (assignment_type),
                       var);
}

operand
dump_variable_assignment_res (operand var)
{
  operand op = tmp_operand ();
  dump_variable_assignment (op, var);
  return op;
}

void
dump_varg_header_for_rewrite (varg_list_type vlt, operand obj)
{
  STACK_PUSH (varg_headers, serializer_get_current_instruction_counter ());
  switch (vlt)
  {
    case VARG_FUNC_EXPR:
    case VARG_CONSTRUCT_EXPR:
    case VARG_CALL_EXPR:
    {
      operand res = empty_operand ();
      dump_op_meta_for_vlt (vlt, &res, &obj);
      break;
    }
    case VARG_FUNC_DECL:
    {
      dump_op_meta_for_vlt (vlt, NULL, &obj);
      break;
    }
    case VARG_ARRAY_DECL:
    case VARG_OBJ_DECL:
    {
      operand res = empty_operand ();
      dump_op_meta_for_vlt (vlt, &res, NULL);
      break;
    }
  }
}

operand
rewrite_varg_header_set_args_count (uint8_t args_count)
{
  op_meta om = serializer_get_op_meta (STACK_TOP (varg_headers));
  switch (om.op.op_idx)
  {
    case VM_OP_FUNC_EXPR_N:
    case VM_OP_CONSTRUCT_N:
    case VM_OP_CALL_N:
    case VM_OP_NATIVE_CALL:
    {
      const operand res = tmp_operand ();
      om.op.data.func_expr_n.arg_list = args_count;
      om.op.data.func_expr_n.lhs = res.uid;
      serializer_rewrite_op_meta (STACK_TOP (varg_headers), om);
      STACK_DROP (varg_headers, 1);
      return res;
    }
    case VM_OP_FUNC_DECL_N:
    {
      om.op.data.func_decl_n.arg_list = args_count;
      serializer_rewrite_op_meta (STACK_TOP (varg_headers), om);
      STACK_DROP (varg_headers, 1);
      return empty_operand ();
    }
    case VM_OP_ARRAY_DECL:
    case VM_OP_OBJ_DECL:
    {
      const operand res = tmp_operand ();
      om.op.data.obj_decl.list = args_count;
      om.op.data.obj_decl.lhs = res.uid;
      serializer_rewrite_op_meta (STACK_TOP (varg_headers), om);
      STACK_DROP (varg_headers, 1);
      return res;
    }
    default:
    {
      JERRY_UNREACHABLE ();
    }
  }
}

/**
 * Dump 'meta' instruction of 'call additional information' type,
 * containing call flags and, optionally, 'this' argument
 */
void
dump_call_additional_info (opcode_call_flags_t flags, /**< call flags */
                           operand this_arg) /**< 'this' argument - if flags include OPCODE_CALL_FLAGS_HAVE_THIS_ARG,
                                              *   or empty operand - otherwise */
{
  if (flags & OPCODE_CALL_FLAGS_HAVE_THIS_ARG)
  {
    JERRY_ASSERT (this_arg.type == OPERAND_TMP);
    JERRY_ASSERT (!operand_is_empty (this_arg));
  }
  else
  {
    JERRY_ASSERT (operand_is_empty (this_arg));
  }

  vm_idx_t this_arg_idx = ((flags & OPCODE_CALL_FLAGS_HAVE_THIS_ARG) != 0 ? this_arg.uid : (vm_idx_t) VM_IDX_EMPTY);
  const vm_instr_t instr = getop_meta (OPCODE_META_TYPE_CALL_SITE_INFO, flags, this_arg_idx);

  serializer_dump_op_meta (create_op_meta (instr, NOT_A_LITERAL, NOT_A_LITERAL, NOT_A_LITERAL));
} /* dump_call_additional_info */

void
dump_varg (operand op)
{
  if (operand_is_constant (op))
  {
    op = dump_variable_assignment_res (op);
  }
  dump_triple_address (VM_OP_META, int_const_operand (OPCODE_META_TYPE_VARG), op, empty_operand ());
}

void
dump_prop_name_and_value (operand name, operand value)
{
  operand tmp;

  literal_t lit = lit_get_literal_by_cp (name.lit_id);

  if (lit_literal_is_utf8_string (lit))
  {
    JERRY_ASSERT (name.type == OPERAND_STRING);
    tmp = dump_string_assignment_res (name.lit_id);
  }
  else
  {
    JERRY_ASSERT (name.type == OPERAND_NUMBER);
    JERRY_ASSERT (lit_literal_is_num (lit));
    tmp = dump_number_assignment_res (name.lit_id);
  }

  if (operand_is_constant (value))
  {
    value = dump_variable_assignment_res (value);
  }

  dump_triple_address (VM_OP_META, int_const_operand (OPCODE_META_TYPE_VARG_PROP_DATA), tmp, value);
}

void
dump_prop_getter_decl (operand name, operand func)
{
  JERRY_ASSERT (name.type == OPERAND_STRING
                || name.type == OPERAND_NUMBER);
  JERRY_ASSERT (func.type == OPERAND_TMP);

  operand tmp;

  literal_t lit = lit_get_literal_by_cp (name.lit_id);

  if (lit_literal_is_utf8_string (lit))
  {
    tmp = dump_string_assignment_res (name.lit_id);
  }
  else
  {
    JERRY_ASSERT (lit_literal_is_num (lit));
    tmp = dump_number_assignment_res (name.lit_id);
  }

  dump_triple_address (VM_OP_META, int_const_operand (OPCODE_META_TYPE_VARG_PROP_GETTER), tmp, func);
}

void
dump_prop_setter_decl (operand name, operand func)
{
  JERRY_ASSERT (name.type == OPERAND_STRING
                || name.type == OPERAND_NUMBER);
  JERRY_ASSERT (func.type == OPERAND_TMP);

  operand tmp;

  literal_t lit = lit_get_literal_by_cp (name.lit_id);

  if (lit_literal_is_utf8_string (lit))
  {
    tmp = dump_string_assignment_res (name.lit_id);
  }
  else
  {
    JERRY_ASSERT (lit_literal_is_num (lit));
    tmp = dump_number_assignment_res (name.lit_id);
  }

  dump_triple_address (VM_OP_META, int_const_operand (OPCODE_META_TYPE_VARG_PROP_SETTER), tmp, func);
}

operand
dump_prop_getter_res (operand obj, operand prop)
{
  const operand res = tmp_operand ();
  dump_triple_address (VM_OP_PROP_GETTER, res, obj, prop);
  return res;
}

void
dump_prop_setter (operand res, operand obj, operand prop)
{
  dump_triple_address (VM_OP_PROP_SETTER, res, obj, prop);
}

void
dump_function_end_for_rewrite (void)
{
  STACK_PUSH (function_ends, serializer_get_current_instruction_counter ());
  const vm_instr_t instr = getop_meta (OPCODE_META_TYPE_FUNCTION_END, VM_IDX_EMPTY, VM_IDX_EMPTY);
  serializer_dump_op_meta (create_op_meta (instr, NOT_A_LITERAL, NOT_A_LITERAL, NOT_A_LITERAL));
}

void
rewrite_function_end (varg_list_type vlt)
{
  vm_instr_counter_t oc;
  if (vlt == VARG_FUNC_DECL)
  {
    oc = (vm_instr_counter_t) (get_diff_from (STACK_TOP (function_ends))
                                            + serializer_count_instrs_in_subscopes ());
  }
  else
  {
    JERRY_ASSERT (vlt == VARG_FUNC_EXPR);
    oc = (vm_instr_counter_t) (get_diff_from (STACK_TOP (function_ends)));
  }
  vm_idx_t id1, id2;
  split_instr_counter (oc, &id1, &id2);
  const vm_instr_t instr = getop_meta (OPCODE_META_TYPE_FUNCTION_END, id1, id2);
  serializer_rewrite_op_meta (STACK_TOP (function_ends),
                              create_op_meta (instr, NOT_A_LITERAL, NOT_A_LITERAL, NOT_A_LITERAL));
  STACK_DROP (function_ends, 1);
}

operand
dump_this_res (void)
{
  const operand res = tmp_operand ();
  dump_single_address (VM_OP_THIS_BINDING, res);
  return res;
}

operand
dump_post_increment_res (operand op)
{
  return dump_double_address_res (VM_OP_POST_INCR, op);
}

operand
dump_post_decrement_res (operand op)
{
  return dump_double_address_res (VM_OP_POST_DECR, op);
}

operand
dump_pre_increment_res (operand op)
{
  return dump_double_address_res (VM_OP_PRE_INCR, op);
}

operand
dump_pre_decrement_res (operand op)
{
  return dump_double_address_res (VM_OP_PRE_DECR, op);
}

operand
dump_unary_plus_res (operand op)
{
  return dump_double_address_res (VM_OP_UNARY_PLUS, op);
}

operand
dump_unary_minus_res (operand op)
{
  return dump_double_address_res (VM_OP_UNARY_MINUS, op);
}

operand
dump_bitwise_not_res (operand op)
{
  return dump_double_address_res (VM_OP_B_NOT, op);
}

operand
dump_logical_not_res (operand op)
{
  return dump_double_address_res (VM_OP_LOGICAL_NOT, op);
}

operand
dump_delete_res (operand op, bool is_strict, locus loc)
{
  if (op.type == OPERAND_IDENTIFIER)
  {
    const operand res = tmp_operand ();

    literal_t lit = lit_get_literal_by_cp (op.lit_id);

    JERRY_ASSERT (lit_literal_is_utf8_string (lit));

    jsp_early_error_check_delete (is_strict, loc);

    dump_double_address (VM_OP_DELETE_VAR, res, op);

    return res;
  }
  else if (op.type == OPERAND_TMP)
  {
    const op_meta last_op_meta = last_dumped_op_meta ();
    if (last_op_meta.op.op_idx == VM_OP_PROP_GETTER)
    {
      const operand res = tmp_operand ();

      const vm_instr_counter_t oc = (vm_instr_counter_t) (serializer_get_current_instruction_counter () - 1);
      serializer_set_writing_position (oc);

      dump_triple_address (VM_OP_DELETE_PROP,
                           res,
                           create_operand_from_tmp_and_lit (last_op_meta.op.data.prop_getter.obj,
                                                            last_op_meta.lit_id[1]),
                           create_operand_from_tmp_and_lit (last_op_meta.op.data.prop_getter.prop,
                                                            last_op_meta.lit_id[2]));

      return res;
    }
    else
    {
      return dump_boolean_assignment_res (true);
    }
  }
  else
  {
    JERRY_ASSERT (op.type == OPERAND_STRING
                  || op.type == OPERAND_NUMBER);
    return dump_boolean_assignment_res (true);
  }
}

operand
dump_typeof_res (operand op)
{
  return dump_double_address_res (VM_OP_TYPEOF, op);
}

operand
dump_multiplication_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_MULTIPLICATION, lhs, rhs);
}

operand
dump_division_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_DIVISION, lhs, rhs);
}

operand
dump_remainder_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_REMAINDER, lhs, rhs);
}

operand
dump_addition_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_ADDITION, lhs, rhs);
}

operand
dump_substraction_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_SUBSTRACTION, lhs, rhs);
}

operand
dump_left_shift_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_B_SHIFT_LEFT, lhs, rhs);
}

operand
dump_right_shift_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_B_SHIFT_RIGHT, lhs, rhs);
}

operand
dump_right_shift_ex_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_B_SHIFT_URIGHT, lhs, rhs);
}

operand
dump_less_than_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_LESS_THAN, lhs, rhs);
}

operand
dump_greater_than_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_GREATER_THAN, lhs, rhs);
}

operand
dump_less_or_equal_than_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_LESS_OR_EQUAL_THAN, lhs, rhs);
}

operand
dump_greater_or_equal_than_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_GREATER_OR_EQUAL_THAN, lhs, rhs);
}

operand
dump_instanceof_res (operand lhs, operand rhs)
{
  const operand res = tmp_operand ();
  dump_triple_address (VM_OP_INSTANCEOF, res, lhs, rhs);
  return res;
}

operand
dump_in_res (operand lhs, operand rhs)
{
  const operand res = tmp_operand ();
  dump_triple_address (VM_OP_IN, res, lhs, rhs);
  return res;
}

operand
dump_equal_value_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_EQUAL_VALUE, lhs, rhs);
}

operand
dump_not_equal_value_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_NOT_EQUAL_VALUE, lhs, rhs);
}

operand
dump_equal_value_type_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_EQUAL_VALUE_TYPE, lhs, rhs);
}

operand
dump_not_equal_value_type_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_NOT_EQUAL_VALUE_TYPE, lhs, rhs);
}

operand
dump_bitwise_and_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_B_AND, lhs, rhs);
}

operand
dump_bitwise_xor_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_B_XOR, lhs, rhs);
}

operand
dump_bitwise_or_res (operand lhs, operand rhs)
{
  return dump_triple_address_res (VM_OP_B_OR, lhs, rhs);
}

void
start_dumping_logical_and_checks (void)
{
  STACK_PUSH (U8, (uint8_t) STACK_SIZE (logical_and_checks));
}

void
dump_logical_and_check_for_rewrite (operand op)
{
  if (operand_is_constant (op))
  {
    op = dump_variable_assignment_res (op);
  }

  STACK_PUSH (logical_and_checks, serializer_get_current_instruction_counter ());

  const vm_instr_t instr = getop_is_false_jmp_down (op.uid, VM_IDX_REWRITE_GENERAL_CASE, VM_IDX_REWRITE_GENERAL_CASE);
  serializer_dump_op_meta (create_op_meta (instr, op.lit_id, NOT_A_LITERAL, NOT_A_LITERAL));
}

void
rewrite_logical_and_checks (void)
{
  for (uint8_t i = STACK_TOP (U8); i < STACK_SIZE (logical_and_checks); i++)
  {
    op_meta jmp_op_meta = serializer_get_op_meta (STACK_ELEMENT (logical_and_checks, i));
    JERRY_ASSERT (jmp_op_meta.op.op_idx == VM_OP_IS_FALSE_JMP_DOWN);
    vm_idx_t id1, id2;
    split_instr_counter (get_diff_from (STACK_ELEMENT (logical_and_checks, i)), &id1, &id2);
    jmp_op_meta.op.data.is_false_jmp_down.opcode_1 = id1;
    jmp_op_meta.op.data.is_false_jmp_down.opcode_2 = id2;
    serializer_rewrite_op_meta (STACK_ELEMENT (logical_and_checks, i), jmp_op_meta);
  }
  STACK_DROP (logical_and_checks, STACK_SIZE (logical_and_checks) - STACK_TOP (U8));
  STACK_DROP (U8, 1);
}

void
start_dumping_logical_or_checks (void)
{
  STACK_PUSH (U8, (uint8_t) STACK_SIZE (logical_or_checks));
}

void
dump_logical_or_check_for_rewrite (operand op)
{
  if (operand_is_constant (op))
  {
    op = dump_variable_assignment_res (op);
  }

  STACK_PUSH (logical_or_checks, serializer_get_current_instruction_counter ());

  const vm_instr_t instr = getop_is_true_jmp_down (op.uid, VM_IDX_REWRITE_GENERAL_CASE, VM_IDX_REWRITE_GENERAL_CASE);
  serializer_dump_op_meta (create_op_meta (instr, op.lit_id, NOT_A_LITERAL, NOT_A_LITERAL));
}

void
rewrite_logical_or_checks (void)
{
  for (uint8_t i = STACK_TOP (U8); i < STACK_SIZE (logical_or_checks); i++)
  {
    op_meta jmp_op_meta = serializer_get_op_meta (STACK_ELEMENT (logical_or_checks, i));
    JERRY_ASSERT (jmp_op_meta.op.op_idx == VM_OP_IS_TRUE_JMP_DOWN);
    vm_idx_t id1, id2;
    split_instr_counter (get_diff_from (STACK_ELEMENT (logical_or_checks, i)), &id1, &id2);
    jmp_op_meta.op.data.is_true_jmp_down.opcode_1 = id1;
    jmp_op_meta.op.data.is_true_jmp_down.opcode_2 = id2;
    serializer_rewrite_op_meta (STACK_ELEMENT (logical_or_checks, i), jmp_op_meta);
  }
  STACK_DROP (logical_or_checks, STACK_SIZE (logical_or_checks) - STACK_TOP (U8));
  STACK_DROP (U8, 1);
}

void
dump_conditional_check_for_rewrite (operand op)
{
  if (operand_is_constant (op))
  {
    op = dump_variable_assignment_res (op);
  }

  STACK_PUSH (conditional_checks, serializer_get_current_instruction_counter ());

  const vm_instr_t instr = getop_is_false_jmp_down (op.uid, VM_IDX_REWRITE_GENERAL_CASE, VM_IDX_REWRITE_GENERAL_CASE);
  serializer_dump_op_meta (create_op_meta (instr, op.lit_id, NOT_A_LITERAL, NOT_A_LITERAL));
}

void
rewrite_conditional_check (void)
{
  op_meta jmp_op_meta = serializer_get_op_meta (STACK_TOP (conditional_checks));
  JERRY_ASSERT (jmp_op_meta.op.op_idx == VM_OP_IS_FALSE_JMP_DOWN);
  vm_idx_t id1, id2;
  split_instr_counter (get_diff_from (STACK_TOP (conditional_checks)), &id1, &id2);
  jmp_op_meta.op.data.is_false_jmp_down.opcode_1 = id1;
  jmp_op_meta.op.data.is_false_jmp_down.opcode_2 = id2;
  serializer_rewrite_op_meta (STACK_TOP (conditional_checks), jmp_op_meta);
  STACK_DROP (conditional_checks, 1);
}

void
dump_jump_to_end_for_rewrite (void)
{
  STACK_PUSH (jumps_to_end, serializer_get_current_instruction_counter ());
  const vm_instr_t instr = getop_jmp_down (VM_IDX_REWRITE_GENERAL_CASE, VM_IDX_REWRITE_GENERAL_CASE);
  serializer_dump_op_meta (create_op_meta (instr, NOT_A_LITERAL, NOT_A_LITERAL, NOT_A_LITERAL));
}

void
rewrite_jump_to_end (void)
{
  op_meta jmp_op_meta = serializer_get_op_meta (STACK_TOP (jumps_to_end));
  JERRY_ASSERT (jmp_op_meta.op.op_idx == VM_OP_JMP_DOWN);
  vm_idx_t id1, id2;
  split_instr_counter (get_diff_from (STACK_TOP (jumps_to_end)), &id1, &id2);
  jmp_op_meta.op.data.jmp_down.opcode_1 = id1;
  jmp_op_meta.op.data.jmp_down.opcode_2 = id2;
  serializer_rewrite_op_meta (STACK_TOP (jumps_to_end), jmp_op_meta);
  STACK_DROP (jumps_to_end, 1);
}

void
start_dumping_assignment_expression (void)
{
  const op_meta last = last_dumped_op_meta ();
  if (last.op.op_idx == VM_OP_PROP_GETTER)
  {
    serializer_set_writing_position ((vm_instr_counter_t) (serializer_get_current_instruction_counter () - 1));
  }
  STACK_PUSH (prop_getters, last);
}

operand
dump_prop_setter_or_variable_assignment_res (operand res, operand op)
{
  const op_meta last = STACK_TOP (prop_getters);
  if (last.op.op_idx == VM_OP_PROP_GETTER)
  {
    dump_prop_setter_op_meta (last, op);
  }
  else
  {
    if (res.type == OPERAND_TMP)
    {
      /*
       * FIXME:
       *       Implement correct handling of references through parser operands
       */
      PARSE_ERROR (JSP_EARLY_ERROR_REFERENCE, "Invalid left-hand-side expression", LIT_ITERATOR_POS_ZERO);
    }
    dump_variable_assignment (res, op);
  }
  STACK_DROP (prop_getters, 1);
  return op;
}

operand
dump_prop_setter_or_addition_res (operand res, operand op)
{
  return dump_prop_setter_or_triple_address_res (VM_OP_ADDITION, res, op);
}

operand
dump_prop_setter_or_multiplication_res (operand res, operand op)
{
  return dump_prop_setter_or_triple_address_res (VM_OP_MULTIPLICATION, res, op);
}

operand
dump_prop_setter_or_division_res (operand res, operand op)
{
  return dump_prop_setter_or_triple_address_res (VM_OP_DIVISION, res, op);
}

operand
dump_prop_setter_or_remainder_res (operand res, operand op)
{
  return dump_prop_setter_or_triple_address_res (VM_OP_REMAINDER, res, op);
}

operand
dump_prop_setter_or_substraction_res (operand res, operand op)
{
  return dump_prop_setter_or_triple_address_res (VM_OP_SUBSTRACTION, res, op);
}

operand
dump_prop_setter_or_left_shift_res (operand res, operand op)
{
  return dump_prop_setter_or_triple_address_res (VM_OP_B_SHIFT_LEFT, res, op);
}

operand
dump_prop_setter_or_right_shift_res (operand res, operand op)
{
  return dump_prop_setter_or_triple_address_res (VM_OP_B_SHIFT_RIGHT, res, op);
}

operand
dump_prop_setter_or_right_shift_ex_res (operand res, operand op)
{
  return dump_prop_setter_or_triple_address_res (VM_OP_B_SHIFT_URIGHT, res, op);
}

operand
dump_prop_setter_or_bitwise_and_res (operand res, operand op)
{
  return dump_prop_setter_or_triple_address_res (VM_OP_B_AND, res, op);
}

operand
dump_prop_setter_or_bitwise_xor_res (operand res, operand op)
{
  return dump_prop_setter_or_triple_address_res (VM_OP_B_XOR, res, op);
}

operand
dump_prop_setter_or_bitwise_or_res (operand res, operand op)
{
  return dump_prop_setter_or_triple_address_res (VM_OP_B_OR, res, op);
}

void
dumper_set_next_interation_target (void)
{
  STACK_PUSH (next_iterations, serializer_get_current_instruction_counter ());
}

void
dump_continue_iterations_check (operand op)
{
  if (!operand_is_empty (op) && operand_is_constant (op))
  {
    op = dump_variable_assignment_res (op);
  }

  const vm_instr_counter_t next_iteration_target_diff = (vm_instr_counter_t) (serializer_get_current_instruction_counter ()
                                                                              - STACK_TOP (next_iterations));
  vm_idx_t id1, id2;
  split_instr_counter (next_iteration_target_diff, &id1, &id2);
  if (operand_is_empty (op))
  {
    const vm_instr_t instr = getop_jmp_up (id1, id2);
    serializer_dump_op_meta (create_op_meta (instr, NOT_A_LITERAL, NOT_A_LITERAL, NOT_A_LITERAL));
  }
  else
  {
    const vm_instr_t instr = getop_is_true_jmp_up (op.uid, id1, id2);
    serializer_dump_op_meta (create_op_meta (instr, op.lit_id, NOT_A_LITERAL, NOT_A_LITERAL));
  }
  STACK_DROP (next_iterations, 1);
}

/**
 * Dump template of 'jmp_break_continue' or 'jmp_down' instruction (depending on is_simple_jump argument).
 *
 * Note:
 *      the instruction's flags field is written later (see also: rewrite_simple_or_nested_jump_get_next).
 *
 * @return position of dumped instruction
 */
vm_instr_counter_t
dump_simple_or_nested_jump_for_rewrite (bool is_simple_jump, /**< flag indicating, whether simple jump
                                                              *   or 'jmp_break_continue' template should be dumped */
                                        vm_instr_counter_t next_jump_for_tgt_oc) /**< instruction counter of next
                                                                                  *   template targetted to
                                                                                  *   the same target - if any,
                                                                                  *   or MAX_OPCODES - otherwise */
{
  vm_idx_t id1, id2;
  split_instr_counter (next_jump_for_tgt_oc, &id1, &id2);

  vm_instr_t instr;
  if (is_simple_jump)
  {
    instr = getop_jmp_down (id1, id2);
  }
  else
  {
    instr = getop_jmp_break_continue (id1, id2);
  }

  vm_instr_counter_t ret = serializer_get_current_instruction_counter ();

  serializer_dump_op_meta (create_op_meta (instr, NOT_A_LITERAL, NOT_A_LITERAL, NOT_A_LITERAL));

  return ret;
} /* dump_simple_or_nested_jump_for_rewrite */

/**
 * Write jump target position into previously dumped template of jump (simple or nested) instruction
 *
 * @return instruction counter value that was encoded in the jump before rewrite
 */
vm_instr_counter_t
rewrite_simple_or_nested_jump_and_get_next (vm_instr_counter_t jump_oc, /**< position of jump to rewrite */
                                            vm_instr_counter_t target_oc) /**< the jump's target */
{
  op_meta jump_op_meta = serializer_get_op_meta (jump_oc);

  bool is_simple_jump = (jump_op_meta.op.op_idx == VM_OP_JMP_DOWN);

  JERRY_ASSERT (is_simple_jump
                || (jump_op_meta.op.op_idx == VM_OP_JMP_BREAK_CONTINUE));

  vm_idx_t id1, id2, id1_prev, id2_prev;
  split_instr_counter ((vm_instr_counter_t) (target_oc - jump_oc), &id1, &id2);

  if (is_simple_jump)
  {
    id1_prev = jump_op_meta.op.data.jmp_down.opcode_1;
    id2_prev = jump_op_meta.op.data.jmp_down.opcode_2;

    jump_op_meta.op.data.jmp_down.opcode_1 = id1;
    jump_op_meta.op.data.jmp_down.opcode_2 = id2;
  }
  else
  {
    JERRY_ASSERT (jump_op_meta.op.op_idx == VM_OP_JMP_BREAK_CONTINUE);

    id1_prev = jump_op_meta.op.data.jmp_break_continue.opcode_1;
    id2_prev = jump_op_meta.op.data.jmp_break_continue.opcode_2;

    jump_op_meta.op.data.jmp_break_continue.opcode_1 = id1;
    jump_op_meta.op.data.jmp_break_continue.opcode_2 = id2;
  }

  serializer_rewrite_op_meta (jump_oc, jump_op_meta);

  return vm_calc_instr_counter_from_idx_idx (id1_prev, id2_prev);
} /* rewrite_simple_or_nested_jump_get_next */

void
start_dumping_case_clauses (void)
{
  STACK_PUSH (U8, (uint8_t) STACK_SIZE (case_clauses));
  STACK_PUSH (U8, (uint8_t) STACK_SIZE (case_clauses));
}

void
dump_case_clause_check_for_rewrite (operand switch_expr, operand case_expr)
{
  const operand res = tmp_operand ();
  dump_triple_address (VM_OP_EQUAL_VALUE_TYPE, res, switch_expr, case_expr);
  STACK_PUSH (case_clauses, serializer_get_current_instruction_counter ());
  const vm_instr_t instr = getop_is_true_jmp_down (res.uid, VM_IDX_REWRITE_GENERAL_CASE, VM_IDX_REWRITE_GENERAL_CASE);
  serializer_dump_op_meta (create_op_meta (instr, NOT_A_LITERAL, NOT_A_LITERAL, NOT_A_LITERAL));
}

void
dump_default_clause_check_for_rewrite (void)
{
  STACK_PUSH (case_clauses, serializer_get_current_instruction_counter ());
  const vm_instr_t instr = getop_jmp_down (VM_IDX_REWRITE_GENERAL_CASE, VM_IDX_REWRITE_GENERAL_CASE);
  serializer_dump_op_meta (create_op_meta (instr, NOT_A_LITERAL, NOT_A_LITERAL, NOT_A_LITERAL));
}

void
rewrite_case_clause (void)
{
  const vm_instr_counter_t jmp_oc = STACK_ELEMENT (case_clauses, STACK_HEAD (U8, 2));
  vm_idx_t id1, id2;
  split_instr_counter (get_diff_from (jmp_oc), &id1, &id2);
  op_meta jmp_op_meta = serializer_get_op_meta (jmp_oc);
  JERRY_ASSERT (jmp_op_meta.op.op_idx == VM_OP_IS_TRUE_JMP_DOWN);
  jmp_op_meta.op.data.is_true_jmp_down.opcode_1 = id1;
  jmp_op_meta.op.data.is_true_jmp_down.opcode_2 = id2;
  serializer_rewrite_op_meta (jmp_oc, jmp_op_meta);
  STACK_INCR_HEAD (U8, 2);
}

void
rewrite_default_clause (void)
{
  const vm_instr_counter_t jmp_oc = STACK_TOP (case_clauses);
  vm_idx_t id1, id2;
  split_instr_counter (get_diff_from (jmp_oc), &id1, &id2);
  op_meta jmp_op_meta = serializer_get_op_meta (jmp_oc);
  JERRY_ASSERT (jmp_op_meta.op.op_idx == VM_OP_JMP_DOWN);
  jmp_op_meta.op.data.jmp_down.opcode_1 = id1;
  jmp_op_meta.op.data.jmp_down.opcode_2 = id2;
  serializer_rewrite_op_meta (jmp_oc, jmp_op_meta);
}

void
finish_dumping_case_clauses (void)
{
  STACK_DROP (case_clauses, STACK_SIZE (case_clauses) - STACK_TOP (U8));
  STACK_DROP (U8, 1);
  STACK_DROP (U8, 1);
}

/**
 * Dump template of 'with' instruction.
 *
 * Note:
 *      the instruction's flags field is written later (see also: rewrite_with).
 *
 * @return position of dumped instruction
 */
vm_instr_counter_t
dump_with_for_rewrite (operand op) /**< operand - result of evaluating Expression
                                    *   in WithStatement */
{
  op = dump_variable_assignment_res (op);

  vm_instr_counter_t oc = serializer_get_current_instruction_counter ();

  const vm_instr_t instr = getop_with (op.uid, VM_IDX_REWRITE_GENERAL_CASE, VM_IDX_REWRITE_GENERAL_CASE);
  serializer_dump_op_meta (create_op_meta (instr, op.lit_id, NOT_A_LITERAL, NOT_A_LITERAL));

  return oc;
} /* dump_with_for_rewrite */

/**
 * Write position of 'with' block's end to specified 'with' instruction template,
 * dumped earlier (see also: dump_with_for_rewrite).
 */
void
rewrite_with (vm_instr_counter_t oc) /**< instruction counter of the instruction template */
{
  op_meta with_op_meta = serializer_get_op_meta (oc);

  vm_idx_t id1, id2;
  split_instr_counter (get_diff_from (oc), &id1, &id2);
  with_op_meta.op.data.with.oc_idx_1 = id1;
  with_op_meta.op.data.with.oc_idx_2 = id2;
  serializer_rewrite_op_meta (oc, with_op_meta);
} /* rewrite_with */

/**
 * Dump 'meta' instruction of 'end with' type
 */
void
dump_with_end (void)
{
  const vm_instr_t instr = getop_meta (OPCODE_META_TYPE_END_WITH, VM_IDX_EMPTY, VM_IDX_EMPTY);
  serializer_dump_op_meta (create_op_meta (instr, NOT_A_LITERAL, NOT_A_LITERAL, NOT_A_LITERAL));
} /* dump_with_end */

/**
 * Dump template of 'for_in' instruction.
 *
 * Note:
 *      the instruction's flags field is written later (see also: rewrite_for_in).
 *
 * @return position of dumped instruction
 */
vm_instr_counter_t
dump_for_in_for_rewrite (operand op) /**< operand - result of evaluating Expression
                                      *   in for-in statement */
{
  op = dump_variable_assignment_res (op);

  vm_instr_counter_t oc = serializer_get_current_instruction_counter ();

  const vm_instr_t instr = getop_for_in (op.uid, VM_IDX_REWRITE_GENERAL_CASE, VM_IDX_REWRITE_GENERAL_CASE);
  serializer_dump_op_meta (create_op_meta (instr, op.lit_id, NOT_A_LITERAL, NOT_A_LITERAL));

  return oc;
} /* dump_for_in_for_rewrite */

/**
 * Write position of 'for_in' block's end to specified 'for_in' instruction template,
 * dumped earlier (see also: dump_for_in_for_rewrite).
 */
void
rewrite_for_in (vm_instr_counter_t oc) /**< instruction counter of the instruction template */
{
  op_meta for_in_op_meta = serializer_get_op_meta (oc);

  vm_idx_t id1, id2;
  split_instr_counter (get_diff_from (oc), &id1, &id2);
  for_in_op_meta.op.data.for_in.oc_idx_1 = id1;
  for_in_op_meta.op.data.for_in.oc_idx_2 = id2;
  serializer_rewrite_op_meta (oc, for_in_op_meta);
} /* rewrite_for_in */

/**
 * Dump 'meta' instruction of 'end for_in' type
 */
void
dump_for_in_end (void)
{
  const vm_instr_t instr = getop_meta (OPCODE_META_TYPE_END_FOR_IN, VM_IDX_EMPTY, VM_IDX_EMPTY);
  serializer_dump_op_meta (create_op_meta (instr, NOT_A_LITERAL, NOT_A_LITERAL, NOT_A_LITERAL));
} /* dump_for_in_end */

void
dump_try_for_rewrite (void)
{
  STACK_PUSH (tries, serializer_get_current_instruction_counter ());
  const vm_instr_t instr = getop_try_block (VM_IDX_REWRITE_GENERAL_CASE, VM_IDX_REWRITE_GENERAL_CASE);
  serializer_dump_op_meta (create_op_meta (instr, NOT_A_LITERAL, NOT_A_LITERAL, NOT_A_LITERAL));
}

void
rewrite_try (void)
{
  op_meta try_op_meta = serializer_get_op_meta (STACK_TOP (tries));
  JERRY_ASSERT (try_op_meta.op.op_idx == VM_OP_TRY_BLOCK);
  vm_idx_t id1, id2;
  split_instr_counter (get_diff_from (STACK_TOP (tries)), &id1, &id2);
  try_op_meta.op.data.try_block.oc_idx_1 = id1;
  try_op_meta.op.data.try_block.oc_idx_2 = id2;
  serializer_rewrite_op_meta (STACK_TOP (tries), try_op_meta);
  STACK_DROP (tries, 1);
}

void
dump_catch_for_rewrite (operand op)
{
  JERRY_ASSERT (op.type == OPERAND_IDENTIFIER);
  STACK_PUSH (catches, serializer_get_current_instruction_counter ());
  vm_instr_t instr = getop_meta (OPCODE_META_TYPE_CATCH, VM_IDX_REWRITE_GENERAL_CASE, VM_IDX_REWRITE_GENERAL_CASE);
  serializer_dump_op_meta (create_op_meta (instr, NOT_A_LITERAL, NOT_A_LITERAL, NOT_A_LITERAL));
  dump_triple_address (VM_OP_META, int_const_operand (OPCODE_META_TYPE_CATCH_EXCEPTION_IDENTIFIER), op, empty_operand ());
}

void
rewrite_catch (void)
{
  op_meta catch_op_meta = serializer_get_op_meta (STACK_TOP (catches));
  JERRY_ASSERT (catch_op_meta.op.op_idx == VM_OP_META
                && catch_op_meta.op.data.meta.type == OPCODE_META_TYPE_CATCH);
  vm_idx_t id1, id2;
  split_instr_counter (get_diff_from (STACK_TOP (catches)), &id1, &id2);
  catch_op_meta.op.data.meta.data_1 = id1;
  catch_op_meta.op.data.meta.data_2 = id2;
  serializer_rewrite_op_meta (STACK_TOP (catches), catch_op_meta);
  STACK_DROP (catches, 1);
}

void
dump_finally_for_rewrite (void)
{
  STACK_PUSH (finallies, serializer_get_current_instruction_counter ());
  const vm_instr_t instr = getop_meta (OPCODE_META_TYPE_FINALLY, VM_IDX_REWRITE_GENERAL_CASE, VM_IDX_REWRITE_GENERAL_CASE);
  serializer_dump_op_meta (create_op_meta (instr, NOT_A_LITERAL, NOT_A_LITERAL, NOT_A_LITERAL));
}

void
rewrite_finally (void)
{
  op_meta finally_op_meta = serializer_get_op_meta (STACK_TOP (finallies));
  JERRY_ASSERT (finally_op_meta.op.op_idx == VM_OP_META
                && finally_op_meta.op.data.meta.type == OPCODE_META_TYPE_FINALLY);
  vm_idx_t id1, id2;
  split_instr_counter (get_diff_from (STACK_TOP (finallies)), &id1, &id2);
  finally_op_meta.op.data.meta.data_1 = id1;
  finally_op_meta.op.data.meta.data_2 = id2;
  serializer_rewrite_op_meta (STACK_TOP (finallies), finally_op_meta);
  STACK_DROP (finallies, 1);
}

void
dump_end_try_catch_finally (void)
{
  const vm_instr_t instr = getop_meta (OPCODE_META_TYPE_END_TRY_CATCH_FINALLY,
                                        VM_IDX_EMPTY, VM_IDX_EMPTY);
  serializer_dump_op_meta (create_op_meta (instr, NOT_A_LITERAL, NOT_A_LITERAL, NOT_A_LITERAL));
}

void
dump_throw (operand op)
{
  dump_single_address (VM_OP_THROW_VALUE, op);
}

bool
dumper_variable_declaration_exists (lit_cpointer_t lit_id)
{
  for (vm_instr_counter_t oc = (vm_instr_counter_t) (serializer_get_current_instruction_counter () - 1);
       oc > 0; oc--)
  {
    const op_meta var_decl_op_meta = serializer_get_op_meta (oc);
    if (var_decl_op_meta.op.op_idx != VM_OP_VAR_DECL)
    {
      break;
    }
    if (var_decl_op_meta.lit_id[0].packed_value == lit_id.packed_value)
    {
      return true;
    }
  }
  return false;
}

void
dump_variable_declaration (lit_cpointer_t lit_id)
{
  dump_single_address (VM_OP_VAR_DECL, string_operand (lit_id));
}

/**
 * Dump template of 'meta' instruction for scope's code flags.
 *
 * Note:
 *      the instruction's flags field is written later (see also: rewrite_scope_code_flags).
 *
 * @return position of dumped instruction
 */
vm_instr_counter_t
dump_scope_code_flags_for_rewrite (void)
{
  vm_instr_counter_t oc = serializer_get_current_instruction_counter ();

  const vm_instr_t instr = getop_meta (OPCODE_META_TYPE_SCOPE_CODE_FLAGS, VM_IDX_REWRITE_GENERAL_CASE, VM_IDX_REWRITE_GENERAL_CASE);
  serializer_dump_op_meta (create_op_meta (instr, NOT_A_LITERAL, NOT_A_LITERAL, NOT_A_LITERAL));

  return oc;
} /* dump_scope_code_flags_for_rewrite */

/**
 * Write scope's code flags to specified 'meta' instruction template,
 * dumped earlier (see also: dump_scope_code_flags_for_rewrite).
 */
void
rewrite_scope_code_flags (vm_instr_counter_t scope_code_flags_oc, /**< position of instruction to rewrite */
                          opcode_scope_code_flags_t scope_flags) /**< scope's code properties flags set */
{
  JERRY_ASSERT ((vm_idx_t) scope_flags == scope_flags);

  op_meta opm = serializer_get_op_meta (scope_code_flags_oc);
  JERRY_ASSERT (opm.op.op_idx == VM_OP_META);
  JERRY_ASSERT (opm.op.data.meta.type == OPCODE_META_TYPE_SCOPE_CODE_FLAGS);
  JERRY_ASSERT (opm.op.data.meta.data_1 == VM_IDX_REWRITE_GENERAL_CASE);
  JERRY_ASSERT (opm.op.data.meta.data_2 == VM_IDX_REWRITE_GENERAL_CASE);

  opm.op.data.meta.data_1 = (vm_idx_t) scope_flags;
  serializer_rewrite_op_meta (scope_code_flags_oc, opm);
} /* rewrite_scope_code_flags */

void
dump_ret (void)
{
  dump_no_args (VM_OP_RET);
}

void
dump_reg_var_decl_for_rewrite (void)
{
  STACK_PUSH (reg_var_decls, serializer_get_current_instruction_counter ());
  serializer_dump_op_meta (create_op_meta (getop_reg_var_decl (VM_REG_FIRST, VM_IDX_REWRITE_GENERAL_CASE),
                                           NOT_A_LITERAL, NOT_A_LITERAL, NOT_A_LITERAL));
}

void
rewrite_reg_var_decl (void)
{
  vm_instr_counter_t reg_var_decl_oc = STACK_TOP (reg_var_decls);
  op_meta opm = serializer_get_op_meta (reg_var_decl_oc);
  JERRY_ASSERT (opm.op.op_idx == VM_OP_REG_VAR_DECL);
  opm.op.data.reg_var_decl.max = max_temp_name;
  serializer_rewrite_op_meta (reg_var_decl_oc, opm);
  STACK_DROP (reg_var_decls, 1);
}

void
dump_retval (operand op)
{
  dump_single_address (VM_OP_RETVAL, op);
}

void
dumper_init (void)
{
  max_temp_name = 0;
  reset_temp_name ();
  STACK_INIT (U8);
  STACK_INIT (varg_headers);
  STACK_INIT (function_ends);
  STACK_INIT (logical_and_checks);
  STACK_INIT (logical_or_checks);
  STACK_INIT (conditional_checks);
  STACK_INIT (jumps_to_end);
  STACK_INIT (prop_getters);
  STACK_INIT (next_iterations);
  STACK_INIT (case_clauses);
  STACK_INIT (catches);
  STACK_INIT (finallies);
  STACK_INIT (tries);
  STACK_INIT (temp_names);
  STACK_INIT (reg_var_decls);
}

void
dumper_free (void)
{
  STACK_FREE (U8);
  STACK_FREE (varg_headers);
  STACK_FREE (function_ends);
  STACK_FREE (logical_and_checks);
  STACK_FREE (logical_or_checks);
  STACK_FREE (conditional_checks);
  STACK_FREE (jumps_to_end);
  STACK_FREE (prop_getters);
  STACK_FREE (next_iterations);
  STACK_FREE (case_clauses);
  STACK_FREE (catches);
  STACK_FREE (finallies);
  STACK_FREE (tries);
  STACK_FREE (temp_names);
  STACK_FREE (reg_var_decls);
}
