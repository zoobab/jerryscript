/* Copyright 2015 University of Szeged.
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

#include "js-parser-defines.h"

/**
 * Compute real literal indicies.
 *
 * @return literal encoding mode.
 */
static cbc_literal_encoding
parser_compute_offsets (parser_context *context_p) /**< context */
{
  parser_list_iterator literal_iterator;
  lexer_literal *literal_p;

  uint16_t var_index = 0;
  uint16_t ident_index = context_p->var_count;
  uint16_t other_index = ident_index + context_p->ident_count;

  parser_list_iterator_init (&context_p->literal_pool, &literal_iterator);
  while ((literal_p = (lexer_literal *) parser_list_iterator_next (&literal_iterator)))
  {
    if (literal_p->type == LEXER_VAR_LITERAL)
    {
      literal_p->index = var_index;
      var_index++;
    }
    else if (literal_p->type == LEXER_IDENT_LITERAL)
    {
      literal_p->index = ident_index;
      ident_index++;
    }
    else
    {
      literal_p->index = other_index;
      other_index++;
    }
  }

  PARSER_ASSERT (var_index == context_p->var_count);
  PARSER_ASSERT (ident_index == context_p->var_count + context_p->ident_count);
  PARSER_ASSERT (other_index == context_p->var_count + context_p->ident_count + context_p->other_count);
  PARSER_ASSERT (other_index <= PARSER_MAXIMUM_NUMBER_OF_LITERALS);

  if (other_index < 256)
  {
    return cbc_literal_encoding_byte;
  }

  if (other_index < 511)
  {
    return cbc_literal_encoding_small;
  }

  return cbc_literal_encoding_full;
}

/*
 * During byte code post processing certain bytes are not
 * copied into the final byte code buffer. For example, if
 * one byte is enough for encoding a literal index, the
 * second byte is not copied. However, when a byte is skipped,
 * the offsets of those branches which crosses (jumps over)
 * that byte code should also be decreased by one. Instead
 * of finding these jumps every time when a byte is skipped,
 * all branch offset updates are computed in one step.
 *
 * Branch offset mapping example:
 *
 * Let's assume that each parser_mem_page of the byte_code
 * buffer is 8 bytes long and only 4 bytes are kept for a
 * given page:
 *
 * +---+---+---+---+---+---+---+---+
 * | X | 1 | 2 | 3 | X | 4 | X | X |
 * +---+---+---+---+---+---+---+---+
 *
 * X marks those bytes which are removed. The resulting
 * offset mapping is the following:
 *
 * +---+---+---+---+---+---+---+---+
 * | 0 | 1 | 2 | 3 | 3 | 4 | 4 | 4 |
 * +---+---+---+---+---+---+---+---+
 *
 * Each X is simply replaced by the index of the previous
 * index starting from zero. This shows the number of
 * copied bytes before a given byte including the byte
 * itself. The last byte always shows the number of bytes
 * copied from this page.
 *
 * This mapping allows recomputing all branch targets,
 * since mapping[to] - mapping[from] is the new argument
 * for forward branches. As for backward branches, the
 * equation is reversed to mapping[from] - mapping[to].
 *
 * The mapping is relative to one page, so distance
 * computation affecting multiple pages requires a loop.
 * We should also note that only argument bytes can
 * be skipped, so removed bytes cannot be targeted by
 * branches. Valid branches always target instruction
 * starts only.
 */

/**
 * Recompute the argument of a forward branch.
 *
 * @return the new distance
 */
static size_t
parser_update_forward_branch (parser_mem_page *page_p, /**< current page */
                              size_t full_distance, /**< full distance */
                              uint8_t bytes_copied_before_jump) /**< bytes copied before jump */
{
  size_t new_distance = 0;

  while (full_distance > PARSER_CBC_STREAM_PAGE_SIZE)
  {
    new_distance += page_p->bytes[PARSER_CBC_STREAM_PAGE_SIZE - 1] & CBC_LOWER_SEVEN_BIT_MASK;
    full_distance -= PARSER_CBC_STREAM_PAGE_SIZE;
    page_p = page_p->next_p;
  }

  new_distance += page_p->bytes[full_distance - 1] & CBC_LOWER_SEVEN_BIT_MASK;
  return new_distance - bytes_copied_before_jump;
} /* parser_update_forward_branch */

/**
 * Recompute the argument of a backward branch.
 *
 * @return the new distance
 */
static size_t
parser_update_backward_branch (parser_mem_page *page_p, /**< current page */
                               size_t full_distance, /**< full distance */
                               uint8_t bytes_copied_before_jump) /**< bytes copied before jump */
{
  size_t new_distance = bytes_copied_before_jump;

  while (full_distance >= PARSER_CBC_STREAM_PAGE_SIZE)
  {
    PARSER_ASSERT (page_p != NULL);
    new_distance += page_p->bytes[PARSER_CBC_STREAM_PAGE_SIZE - 1] & CBC_LOWER_SEVEN_BIT_MASK;
    full_distance -= PARSER_CBC_STREAM_PAGE_SIZE;
    page_p = page_p->next_p;
  }

  if (full_distance > 0)
  {
    size_t offset = PARSER_CBC_STREAM_PAGE_SIZE - full_distance;

    PARSER_ASSERT (page_p != NULL);

    new_distance += page_p->bytes[PARSER_CBC_STREAM_PAGE_SIZE - 1] & CBC_LOWER_SEVEN_BIT_MASK;
    new_distance -= page_p->bytes[offset - 1] & CBC_LOWER_SEVEN_BIT_MASK;
  }

  return new_distance;
} /* parser_update_backward_branch */

/**
 * Update targets of all branches in one step.
 */
static void
parse_update_branches (parser_context *context_p, /**< context */
                       uint8_t *byte_code_p, /**< byte code */
                       size_t length) /**< length of byte code */
{
  parser_mem_page *page_p = context_p->byte_code.first_p;
  parser_mem_page *prev_page_p = NULL;
  parser_mem_page *last_page_p = context_p->byte_code.last_p;
  size_t last_position = context_p->byte_code.last_position;
  size_t offset = 0;
  size_t bytes_copied = 0;

  if (last_position >= PARSER_CBC_STREAM_PAGE_SIZE)
  {
    last_page_p = NULL;
    last_position = 0;
  }

  while (page_p != last_page_p || offset < last_position)
  {
    /* Branch instructions are marked to improve search speed. */
    if (page_p->bytes[offset] & CBC_HIGHEST_BIT_MASK)
    {
      uint8_t *bytes_p = byte_code_p + bytes_copied;
      cbc_opcode opcode;
      uint8_t bytes_copied_before_jump = 0;
      size_t branch_argument_length;
      size_t target_distance;
      size_t length;

      if (offset > 0)
      {
        bytes_copied_before_jump = page_p->bytes[offset - 1] & CBC_LOWER_SEVEN_BIT_MASK;
      }
      bytes_p += bytes_copied_before_jump;

      opcode = *bytes_p++;

      if (opcode == CBC_EXT_OPCODE)
      {
        PARSER_ASSERT (cbc_ext_flags[*bytes_p] & CBC_HAS_BRANCH_ARG);
        branch_argument_length = CBC_BRANCH_OFFSET_LENGTH (*bytes_p);
        bytes_p++;
      }
      else
      {
        PARSER_ASSERT (cbc_flags[opcode] & CBC_HAS_BRANCH_ARG);
        branch_argument_length = CBC_BRANCH_OFFSET_LENGTH (opcode);
      }

      /* Decoding target. */
      length = branch_argument_length;
      target_distance = 0;
      do
      {
        target_distance = (target_distance << 8) | *bytes_p;
        bytes_p++;
      }
      while (--length > 0);

      if (opcode == CBC_EXT_OPCODE || CBC_BRANCH_IS_FORWARD (opcode))
      {
        /* Branch target was not set. */
        PARSER_ASSERT (target_distance > 0);

        target_distance = parser_update_forward_branch (page_p,
                                                        offset + target_distance,
                                                        bytes_copied_before_jump);
      }
      else
      {
        if (target_distance < offset)
        {
          uint8_t bytes_copied_before_target = page_p->bytes[offset - target_distance - 1];
          target_distance = bytes_copied_before_jump - (bytes_copied_before_target & CBC_LOWER_SEVEN_BIT_MASK);
        }
        else if (target_distance == offset)
        {
          target_distance = bytes_copied_before_jump;
        }
        else
        {
          target_distance = parser_update_backward_branch (prev_page_p,
                                                           target_distance - offset,
                                                           bytes_copied_before_jump);
        }
      }

      /* Encoding target again. */
      do
      {
        bytes_p--;
        *bytes_p = (uint8_t) (target_distance & 0xff);
        target_distance >>= 8;
      }
      while (--branch_argument_length > 0);
    }

    offset++;
    if (offset >= PARSER_CBC_STREAM_PAGE_SIZE)
    {
      parser_mem_page *next_p = page_p->next_p;

      /* We reverse the pages before the current page. */
      page_p->next_p = prev_page_p;
      prev_page_p = page_p;

      bytes_copied += page_p->bytes[PARSER_CBC_STREAM_PAGE_SIZE - 1] & CBC_LOWER_SEVEN_BIT_MASK;
      page_p = next_p;
      offset = 0;
    }
  }

  /* After this point the pages of the byte code stream are
   * not used anymore. However, they needs to be freed during
   * cleanup, so the first and last pointers of the stream
   * descriptor are reversed as well. */
  if (last_page_p != NULL)
  {
    PARSER_ASSERT (last_page_p == context_p->byte_code.last_p);
    last_page_p->next_p = prev_page_p;
  }
  else
  {
    last_page_p = context_p->byte_code.last_p;
  }

  context_p->byte_code.last_p = context_p->byte_code.first_p;
  context_p->byte_code.first_p = last_page_p;
} /* parse_update_branches */

#ifdef PARSER_DEBUG

static void
parse_print_final_cbc (parser_context *context_p, /**< context */
                       uint8_t *byte_code_p, /**< byte code */
                       size_t length, /**< length of byte code */
                       cbc_literal_encoding encoding) /**< literal encoding mode */
{
  cbc_opcode opcode;
  cbc_opcode ext_opcode;
  uint8_t flags;
  uint8_t *byte_code_start_p = byte_code_p;
  uint8_t *byte_code_end_p = byte_code_p + length;
  size_t cbc_offset;

  printf ("\nFinal byte code:\n  Maximum stack depth: %d\n", (int) context_p->stack_max_depth);
  printf ("  Literal encoding: ");
  switch (encoding)
  {
    case cbc_literal_encoding_byte:
    {
      printf ("byte\n");
      break;
    }
    case cbc_literal_encoding_small:
    {
      printf ("small\n");
      break;
    }
    case cbc_literal_encoding_full:
    {
      printf ("full\n");
      break;
    }
  }

  printf ("  Number of var literals: %d\n", (int) context_p->var_count);
  printf ("  Number of identifiers: %d\n", (int) context_p->ident_count);
  printf ("  Number of other literals: %d\n\n", (int) context_p->other_count);

  while (byte_code_p < byte_code_end_p)
  {
    opcode = *byte_code_p;
    ext_opcode = CBC_EXT_NOP;
    cbc_offset = byte_code_p - byte_code_start_p;

    if (opcode != CBC_EXT_OPCODE)
    {
      flags = cbc_flags[opcode];
      printf (" %3d : %s", (int) cbc_offset, cbc_names[opcode]);
      byte_code_p++;
    }
    else
    {
      ext_opcode = byte_code_p[1];
      flags = cbc_ext_flags[ext_opcode];
      printf (" %3d : %s", (int) cbc_offset, cbc_ext_names[ext_opcode]);
      byte_code_p += 2;
    }

    if (flags & CBC_HAS_BYTE_ARG)
    {
      printf (" byte_arg:%d", *byte_code_p);
      byte_code_p++;
    }

    while (flags & (CBC_HAS_LITERAL_ARG | CBC_HAS_LITERAL_ARG2))
    {
      uint16_t literal_index;
      parser_list_iterator literal_iterator;
      lexer_literal *literal_p;

#if PARSER_MAXIMUM_NUMBER_OF_LITERALS <= CBC_MAXIMUM_BYTE_VALUE
      literal_index = *byte_code_p;
#else
      switch (encoding)
      {
        case cbc_literal_encoding_byte:
        {
          literal_index = *byte_code_p;
          break;
        }
        case cbc_literal_encoding_small:
        {
          literal_index = *byte_code_p;
          if (literal_index == CBC_MAXIMUM_BYTE_VALUE)
          {
            byte_code_p++;
            literal_index = CBC_MAXIMUM_BYTE_VALUE + ((uint16_t) *byte_code_p);
          }
          break;
        }
        case cbc_literal_encoding_full:
        {
          literal_index = *byte_code_p;
          if (literal_index & 0x80)
          {
            byte_code_p++;
            literal_index = ((literal_index & 0x7f) << 8) | ((uint16_t) *byte_code_p);
          }
          break;
        }
      }
#endif /* PARSER_MAXIMUM_NUMBER_OF_LITERALS <= CBC_MAXIMUM_BYTE_VALUE */
      byte_code_p++;

      parser_list_iterator_init (&context_p->literal_pool, &literal_iterator);
      while (1)
      {
        literal_p = (lexer_literal *) parser_list_iterator_next (&literal_iterator);
        PARSER_ASSERT (literal_p != NULL);

        if (literal_p->index == literal_index)
        {
          printf (" ");
          util_print_literal (literal_p);
          printf ("-id:%d", literal_index);
          break;
        }
      }

      if (flags & CBC_HAS_LITERAL_ARG)
      {
        flags &= ~CBC_HAS_LITERAL_ARG;
      }
      else
      {
        break;
      }
    }

    if (flags & CBC_HAS_BRANCH_ARG)
    {
      size_t branch_offset_length = CBC_BRANCH_OFFSET_LENGTH (opcode);
      size_t offset = 0;

      if (opcode == CBC_EXT_OPCODE)
      {
        branch_offset_length = CBC_BRANCH_OFFSET_LENGTH (ext_opcode);
      }

      do
      {
        offset = (offset << 8) | *byte_code_p++;
      }
      while (--branch_offset_length > 0);

      if (CBC_BRANCH_IS_FORWARD (opcode))
      {
        printf (" offset:%d(->%d)", (int) offset, (int) (cbc_offset + offset));
      }
      else
      {
        printf (" offset:%d(->%d)", (int) offset, (int) (cbc_offset - offset));
      }
    }
    printf ("\n");
  }
} /* parse_print_final_cbc */

#endif /* PARSER_DEBUG */

#define PARSER_NEXT_BYTE(page_p, offset) \
  do { \
    if (++(offset) >= PARSER_CBC_STREAM_PAGE_SIZE) \
    { \
      offset = 0; \
      page_p = page_p->next_p; \
    } \
  } while (0)

#define PARSER_NEXT_BYTE_UPDATE(page_p, offset, real_offset) \
  do { \
    page_p->bytes[offset] = real_offset; \
    if (++(offset) >= PARSER_CBC_STREAM_PAGE_SIZE) \
    { \
      offset = 0; \
      real_offset = 0; \
      page_p = page_p->next_p; \
    } \
  } while (0)

/**
 * Post processing main function.
 */
static void
parser_post_processing (parser_context *context_p) /**< context */
{
  cbc_literal_encoding encoding;
  parser_mem_page *page_p;
  parser_mem_page *last_page_p = context_p->byte_code.last_p;
  size_t last_position = context_p->byte_code.last_position;
  size_t offset;
  size_t length;
  uint8_t real_offset;
  uint8_t *byte_code_p;
  uint8_t *dst_p;

  encoding = parser_compute_offsets (context_p);

  if (last_position >= PARSER_CBC_STREAM_PAGE_SIZE)
  {
    last_page_p = NULL;
    last_position = 0;
  }

  page_p = context_p->byte_code.first_p;
  offset = 0;
  length = 0;

  while (page_p != last_page_p || offset < last_position)
  {
    uint8_t *opcode_p;
    cbc_opcode opcode;
    uint8_t flags;
    size_t branch_offset_length;

    opcode_p = page_p->bytes + offset;
    opcode = *opcode_p;
    PARSER_NEXT_BYTE (page_p, offset);
    branch_offset_length = CBC_BRANCH_OFFSET_LENGTH (opcode);
    flags = cbc_flags[opcode];
    length++;

    if (opcode == CBC_EXT_OPCODE)
    {
      cbc_ext_opcode ext_opcode;

      ext_opcode = page_p->bytes[offset];
      branch_offset_length = CBC_BRANCH_OFFSET_LENGTH (ext_opcode);
      flags = cbc_ext_flags[ext_opcode];
      PARSER_NEXT_BYTE (page_p, offset);
      length++;
    }

    if (flags & CBC_HAS_BYTE_ARG)
    {
      /* This argument will be copied without modification. */
      PARSER_NEXT_BYTE (page_p, offset);
      length++;
    }

    while (flags & (CBC_HAS_LITERAL_ARG | CBC_HAS_LITERAL_ARG2))
    {
#if PARSER_MAXIMUM_NUMBER_OF_LITERALS <= CBC_MAXIMUM_BYTE_VALUE
      size_t literal_index = page_p->bytes[offset];

      PARSER_ASSERT (encoding == cbc_literal_encoding_byte);
      lexer_literal *literal_p = parser_list_get (&context_p->literal_pool, literal_index);
      page_p->bytes[offset] = (uint8_t) literal_p->index;

      PARSER_NEXT_BYTE (page_p, offset);
      length++;
#else
      uint8_t *first_byte = page_p->bytes + offset;

      PARSER_NEXT_BYTE (page_p, offset);
      length++;

      if (encoding == cbc_literal_encoding_byte)
      {
        size_t literal_index = *first_byte;

        PARSER_ASSERT (page_p->bytes[offset] == 0);
        lexer_literal *literal_p = parser_list_get (&context_p->literal_pool, literal_index);
        *first_byte = (uint8_t) literal_p->index;
      }
      else
      {
        size_t literal_index = ((size_t) *first_byte) | (((size_t) page_p->bytes[offset]) << 8);
        lexer_literal *literal_p = parser_list_get (&context_p->literal_pool, literal_index);

        if (encoding == cbc_literal_encoding_small)
        {
          if (literal_index < CBC_MAXIMUM_BYTE_VALUE)
          {
            *first_byte = (uint8_t) literal_p->index;
            page_p->bytes[offset] = 0;
          }
          else
          {
            PARSER_ASSERT (literal_index <= 511);
            *first_byte = CBC_MAXIMUM_BYTE_VALUE;
            page_p->bytes[offset] = (uint8_t) (literal_p->index - CBC_MAXIMUM_BYTE_VALUE);
            length++;
          }
        }
        else
        {
          if (literal_index < 128)
          {
            *first_byte = (uint8_t) literal_p->index;
            page_p->bytes[offset] = 0;
          }
          else
          {
            PARSER_ASSERT (literal_index <= 32767);
            *first_byte = (uint8_t) (literal_p->index >> 8) | 0x80;
            page_p->bytes[offset] = (uint8_t) (literal_p->index & 0xff);
            length++;
          }
        }
      }
      PARSER_NEXT_BYTE (page_p, offset);
#endif /* PARSER_MAXIMUM_NUMBER_OF_LITERALS <= CBC_MAXIMUM_BYTE_VALUE */

      if (flags & CBC_HAS_LITERAL_ARG)
      {
        flags &= ~CBC_HAS_LITERAL_ARG;
      }
      else
      {
        break;
      }
    }

    if (flags & CBC_HAS_BRANCH_ARG)
    {
      int prefix_zero = PARSER_TRUE;
#if PARSER_MAXIMUM_CODE_SIZE <= 65535
      cbc_opcode jump_forward = CBC_JUMP_FORWARD_2;
#else /* PARSER_MAXIMUM_CODE_SIZE <= 65535 */
      cbc_opcode jump_forward = CBC_JUMP_FORWARD_3;
#endif /* PARSER_MAXIMUM_CODE_SIZE <= 65535 */

      /* The leading zeroes are dropped from the stream.
       * Although dropping these zeroes for backward
       * branches are unnecessary, we use the same
       * code path for simplicity. */
      PARSER_ASSERT (branch_offset_length > 0 && branch_offset_length <= 3);

      while (--branch_offset_length > 0)
      {
        uint8_t byte = page_p->bytes[offset];
        if (byte > 0 || !prefix_zero)
        {
          prefix_zero = PARSER_FALSE;
          length++;
        }
        else
        {
          PARSER_ASSERT (opcode == CBC_EXT_OPCODE
                         || CBC_BRANCH_IS_FORWARD (opcode));
        }
        PARSER_NEXT_BYTE (page_p, offset);
      }

      if (opcode == jump_forward
          && prefix_zero
          && page_p->bytes[offset] == CBC_BRANCH_OFFSET_LENGTH (jump_forward) + 1)
      {
        /* Uncoditional jumps which jump right after the instruction
         * are effectively NOPs. These jumps are removed from the
         * stream. The 1 byte long CBC_JUMP_FORWARD form marks these
         * instructions, since this form is constructed during post
         * processing and cannot be emitted directly. */
        *opcode_p = CBC_JUMP_FORWARD;
        length --;
      }
      else
      {
        /* Other last bytes are always copied. */
        length++;
      }

      PARSER_NEXT_BYTE (page_p, offset);
    }
  }

  length++; /* TODO auto-detection */
  byte_code_p = (uint8_t *) parser_malloc (context_p, length);
  dst_p = byte_code_p;

  page_p = context_p->byte_code.first_p;
  offset = 0;
  real_offset = 0;

  while (page_p != last_page_p || offset < last_position)
  {
    uint8_t flags;
    uint8_t *opcode_p;
    uint8_t *branch_mark_p;
    cbc_opcode opcode;
    size_t branch_offset_length;

    opcode_p = dst_p;
    branch_mark_p = page_p->bytes + offset;
    opcode = *branch_mark_p;
    branch_offset_length = CBC_BRANCH_OFFSET_LENGTH (opcode);

    if (opcode == CBC_JUMP_FORWARD)
    {
      /* These opcodes are deleted from the stream. */
#if PARSER_MAXIMUM_CODE_SIZE <= 65535
      size_t length = 3;
#else /* PARSER_MAXIMUM_CODE_SIZE <= 65535 */
      size_t length = 4;
#endif /* PARSER_MAXIMUM_CODE_SIZE <= 65535 */

      do
      {
        PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);
      }
      while (--length > 0);

      continue;
    }

    /* Storing the opcode */
    *dst_p++ = opcode;
    real_offset++;
    PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);
    flags = cbc_flags[opcode];

    if (opcode == CBC_EXT_OPCODE)
    {
      cbc_ext_opcode ext_opcode;

      ext_opcode = page_p->bytes[offset];
      flags = cbc_ext_flags[ext_opcode];
      branch_offset_length = CBC_BRANCH_OFFSET_LENGTH (ext_opcode);

      /* Storing the extended opcode */
      *dst_p++ = ext_opcode;
      opcode_p++;
      real_offset++;
      PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);
    }

    if (flags & CBC_HAS_BRANCH_ARG)
    {
      *branch_mark_p |= CBC_HIGHEST_BIT_MASK;
    }

    /* Only literal and call arguments can be combined. */
    PARSER_ASSERT (!(flags & CBC_HAS_BRANCH_ARG)
                   || !(flags & (CBC_HAS_BYTE_ARG | CBC_HAS_LITERAL_ARG)));

    if (flags & CBC_HAS_BYTE_ARG)
    {
      /* This argument will be copied without modification. */
      *dst_p++ = page_p->bytes[offset];
      real_offset++;
      PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);
    }

    while (flags & (CBC_HAS_LITERAL_ARG | CBC_HAS_LITERAL_ARG2))
    {
#if PARSER_MAXIMUM_NUMBER_OF_LITERALS <= CBC_MAXIMUM_BYTE_VALUE
      *dst_p++ = page_p->bytes[offset];
      real_offset++;
      PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);
#else
      uint8_t first_byte = page_p->bytes[offset];

      *dst_p++ = first_byte;
      real_offset++;
      PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);

      if (encoding != cbc_literal_encoding_byte)
      {
        if ((encoding == cbc_literal_encoding_small && first_byte == CBC_MAXIMUM_BYTE_VALUE)
            || (encoding == cbc_literal_encoding_full && (first_byte & 0x80)))
        {
          *dst_p++ = page_p->bytes[offset];
          real_offset++;
        }
      }
      PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);
#endif /* PARSER_MAXIMUM_NUMBER_OF_LITERALS <= CBC_MAXIMUM_BYTE_VALUE */

      if (flags & CBC_HAS_LITERAL_ARG)
      {
        flags &= ~CBC_HAS_LITERAL_ARG;
      }
      else
      {
        break;
      }
    }

    if (flags & CBC_HAS_BRANCH_ARG)
    {
      int prefix_zero = PARSER_TRUE;

      /* The leading zeroes are dropped from the stream. */
      PARSER_ASSERT (branch_offset_length > 0 && branch_offset_length <= 3);

      while (--branch_offset_length > 0)
      {
        uint8_t byte = page_p->bytes[offset];
        if (byte > 0 || !prefix_zero)
        {
          prefix_zero = PARSER_FALSE;
          *dst_p++ = page_p->bytes[offset];
          real_offset++;
        }
        else
        {
          /* When a leading zero is dropped, the branch
           * offset length must be decreased as well. */
          (*opcode_p)--;
        }
        PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);
      }

      *dst_p++ = page_p->bytes[offset];
      real_offset++;
      PARSER_NEXT_BYTE_UPDATE (page_p, offset, real_offset);
    }
  }

  *dst_p++ = CBC_RETURN_WITH_UNDEFINED;
  PARSER_ASSERT (dst_p == byte_code_p + length);

  parse_update_branches (context_p, byte_code_p, length);

#ifdef PARSER_DEBUG
  if (context_p->is_show_opcodes)
  {
    parse_print_final_cbc (context_p, byte_code_p, length, encoding);
  }
  printf ("\nParse successfully completed. Total byte code size: %d bytes\n", (int) length);
#endif
} /* parser_post_processing */

#undef PARSER_NEXT_BYTE
#undef PARSER_NEXT_BYTE_UPDATE

/**
 * Free identifiers and literals.
 */
static void
parser_free_lists (parser_context *context_p) /**< context */
{
  parser_list_iterator literal_iterator;
  lexer_literal *literal_p;

  parser_list_iterator_init (&context_p->literal_pool, &literal_iterator);
  while ((literal_p = (lexer_literal *) parser_list_iterator_next (&literal_iterator)) != NULL)
  {
    util_free_literal (literal_p);
  }

  parser_list_free (&context_p->literal_pool);
} /* parser_free_identifiers */

/**
 * Parse EcmaScript source code
 */
void
parser_parse_script (const uint8_t *source_p, size_t size)
{
  parser_context context;

  context.error_str_p = NULL;

  context.is_strict = 0;
  context.stack_depth = 0;
  context.stack_max_depth = 0;
  context.last_statement.current_p = NULL;
#ifdef PARSER_DEBUG
  context.is_show_opcodes = 1;
#endif

  context.source_p = source_p;
  context.source_end_p = source_p + size;
  context.line = 1;
  context.column = 1;

  context.last_cbc_opcode = PARSER_CBC_UNAVAILABLE;

  context.var_count = 0;
  context.ident_count = 0;
  context.other_count = 0;

  parser_cbc_stream_init (&context.byte_code);
  context.byte_code_size = 0;
  parser_list_init (&context.literal_pool, sizeof (lexer_literal), 15);
  parser_stack_init (&context);

  PARSER_TRY (context.try_buffer)
  {
    /* Pushing a dummy value ensures the stack is never empty.
     * This simplifies the stack management routines. */
    parser_stack_push_uint8 (&context, CBC_MAXIMUM_BYTE_VALUE);
    /* The next token must always be present to make decisions
     * in the parser. Therefore when a token is consumed, the
     * lexer_next_token() must be immediately called. */
    lexer_next_token (&context);

    parser_parse_statements (&context);

    /* When the parsing is successful, only the
     * dummy value can be remained on the stack. */
    PARSER_ASSERT (context.stack_top_uint8 == CBC_MAXIMUM_BYTE_VALUE
                   && context.stack.last_position == 1
                   && context.stack.first_p != NULL
                   && context.stack.first_p->next_p == NULL
                   && context.stack.last_p == NULL);
    PARSER_ASSERT (context.last_statement.current_p == NULL);

    PARSER_ASSERT (context.last_cbc_opcode == PARSER_CBC_UNAVAILABLE);
    parser_post_processing (&context);
  }
  PARSER_CATCH
  {
    if (context.last_statement.current_p != NULL)
    {
      parser_free_jumps (&context);
    }

    printf ("Parse error '%s' at line: %d col: %d\n",
            context.error_str_p,
            (int) context.token.line,
            (int) context.token.column);
  }
  PARSER_TRY_END

  parser_cbc_stream_free (&context.byte_code);
  parser_free_lists (&context);
  parser_stack_free (&context);
} /* parser_parse_script */
