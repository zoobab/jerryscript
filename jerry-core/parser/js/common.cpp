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

#include "common.h"
#include "ecma-helpers.h"

/**
 * Checks whether the next UTF8 character is a valid identifier start.
 *
 * @return non-zero if it is.
 */
int
util_is_identifier_start (const uint8_t *src_p) /* pointer to a vaild UTF8 character */
{
  if (*src_p > 127)
  {
    return 0;
  }

  return util_is_identifier_start_character (*src_p);
} /* util_is_identifier_start */

/**
 * Checks whether the next UTF8 character is a valid identifier part.
 *
 * @return non-zero if it is.
 */
int
util_is_identifier_part (const uint8_t *src_p) /* pointer to a vaild UTF8 character */
{
  if (*src_p > 127)
  {
    return 0;
  }

  return util_is_identifier_part_character (*src_p);
} /* util_is_identifier_part */

/**
 * Checks whether the character is a valid identifier start.
 *
 * @return non-zero if it is.
 */
int
util_is_identifier_start_character (uint16_t chr) /**< EcmaScript character */
{
  return (((chr | 0x20) >= 'a' && (chr | 0x20) <= 'z')
          || chr == '$' || chr == '_');
} /* util_is_identifier_start_character */

/**
 * Checks whether the character is a valid identifier part.
 *
 * @return non-zero if it is.
 */
int
util_is_identifier_part_character (uint16_t chr) /**< EcmaScript character */
{
  return (((chr | 0x20) >= 'a' && (chr | 0x20) <= 'z')
          || (chr >= '0' && chr <= '9')
          || chr == '$' || chr == '_');
} /* util_is_identifier_part_character */

/**
 * Converts a character to UTF8 bytes.
 *
 * @return length of the UTF8 representation.
 */
size_t
util_to_utf8_bytes (uint8_t *dst_p, /**< destination buffer */
                    uint16_t chr) /**< EcmaScript character */
{
  if (!(chr & ~0x007f))
  {
    /* 00000000 0xxxxxxx -> 0xxxxxxx */
    *dst_p = (uint8_t) chr;
    return 1;
  }

  if (!(chr & ~0x07ff))
  {
    /* 00000yyy yyxxxxxx -> 110yyyyy 10xxxxxx */
    *(dst_p++) = (uint8_t) (0xc0 | ((chr >> 6) & 0x1f));
    *dst_p = (uint8_t) (0x80 | (chr & 0x3f));
    return 2;
  }

  /* zzzzyyyy yyxxxxxx -> 1110zzzz 10yyyyyy 10xxxxxx */
  *(dst_p++) = (uint8_t) (0xe0 | ((chr >> 12) & 0x0f));
  *(dst_p++) = (uint8_t) (0x80 | ((chr >> 6) & 0x3f));
  *dst_p = (uint8_t) (0x80 | (chr & 0x3f));
  return 3;
} /* util_to_utf8_bytes */

/**
 * Returns the length of the UTF8 representation of a character.
 *
 * @return length of the UTF8 representation.
 */
size_t
util_get_utf8_length (uint16_t chr) /**< EcmaScript character */
{
  if (!(chr & ~0x007f))
  {
    /* 00000000 0xxxxxxx */
    return 1;
  }

  if (!(chr & ~0x07ff))
  {
    /* 00000yyy yyxxxxxx */
    return 2;
  }

  /* zzzzyyyy yyxxxxxx */
  return 3;
} /* util_get_utf8_length */

/**
 * Initializes a string literal from a character array.
 *
 * @return non-zero if an error is occured.
 */
int
util_set_function_literal (lexer_literal_t *literal_p, /* literal */
                           void *function_p) /* function */
{
  literal_p->value = function_p;
  return 0;
} /* util_set_function_literal */

/**
 * Free literal.
 */
void
util_free_literal (lexer_literal_t *literal_p) /* literal */
{
  if (literal_p->type == LEXER_IDENT_LITERAL
      || literal_p->type == LEXER_STRING_LITERAL)
  {
    if (literal_p->length > 0)
    {
      ecma_deref_ecma_string (ecma_get_string_from_value (literal_p->value));
    }
  }
  else if (literal_p->type == LEXER_FUNCTION_LITERAL)
  {
    PARSER_FREE (literal_p->value);
  }
} /* util_free_literal */

#ifdef PARSER_DEBUG
/**
 * Print literal.
 */
void
util_print_literal (lexer_literal_t *literal_p) /* literal */
{
  if (literal_p->type == LEXER_IDENT_LITERAL)
  {
    if (literal_p->status_flags & LEXER_FLAG_VAR)
    {
      printf ("var_ident(");
    }
    else
    {
      printf ("ident(");
    }
    util_print_string (ecma_get_string_from_value (literal_p->value));
  }
  else if (literal_p->type == LEXER_FUNCTION_LITERAL)
  {
    printf ("function");
    return;
  }
  else if (literal_p->type == LEXER_STRING_LITERAL)
  {
    printf ("string(");
    util_print_string (ecma_get_string_from_value (literal_p->value));
  }
  else if (literal_p->type == LEXER_NUMBER_LITERAL)
  {
    printf ("number(");
    util_print_number (ecma_get_number_from_value (literal_p->value));
  }
  else if (literal_p->type == LEXER_REGEXP_LITERAL)
  {
    printf ("regexp(");
    util_print_string (ecma_get_string_from_value (literal_p->value));
  }
  else
  {
    printf ("unknown");
    return;
  }

  printf (")");
} /* util_print_literal */
#endif

#ifdef PARSER_DEBUG
/**
 * Debug utility to print a character sequence.
 */
void
util_print_string (ecma_string_t *str_p) /**< String pointer */
{
  lit_utf8_size_t str_size = ecma_string_get_size (str_p);
  MEM_DEFINE_LOCAL_ARRAY (utf8_str_p, str_size + 1, lit_utf8_byte_t);

  ssize_t sz = ecma_string_to_utf8_string (str_p, utf8_str_p, (ssize_t) str_size);
  JERRY_ASSERT (sz >= 0);
  utf8_str_p[sz] = 0;
  printf ("%s", utf8_str_p);

  MEM_FINALIZE_LOCAL_ARRAY (utf8_str_p);
}

void
util_print_number (ecma_number_t *num_p)
{
  lit_utf8_byte_t str_buf[ECMA_MAX_CHARS_IN_STRINGIFIED_NUMBER];
  lit_utf8_size_t str_size = ecma_number_to_utf8_string (*num_p, str_buf, sizeof (str_buf));
  str_buf[str_size] = 0;
  printf ("%s", str_buf);
}
#endif
