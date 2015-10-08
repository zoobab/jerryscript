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

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <setjmp.h>

/* The utilites here are just for compiling purposes, JS
 * engines should have an optimized version for them. */

#define PARSER_DEBUG

/* Malloc */

#define PARSER_MALLOC(size) malloc (size)
#define PARSER_FREE(ptr) free (ptr)

#define PARSER_MALLOC_LOCAL(size) malloc (size)
#define PARSER_FREE_LOCAL(ptr) free (ptr)

/* UTF character management. Only ASCII characters are
 * supported for simplicity. */

int util_is_identifier_start (const uint8_t *);
int util_is_identifier_part (const uint8_t *);
int util_is_identifier_start_character (uint16_t);
int util_is_identifier_part_character (uint16_t);
size_t util_to_utf8_bytes (uint8_t *, uint16_t);
size_t util_get_utf8_length (uint16_t);

/* Immediate management. */

/**
 * Literal types.
 *
 * Important rules for literal searching:
 *   (LEXER_VAR_LITERAL | 0x1) == LEXER_IDENT_LITERAL
 *   (LEXER_IDENT_LITERAL | 0x1) == LEXER_IDENT_LITERAL
 *   (LEXER_UNKNOWN_LITERAL | 0x1) == LEXER_STRING_LITERAL
 *   (LEXER_STRING_LITERAL | 0x1) == LEXER_STRING_LITERAL
 *
 * The LEXER_VAR_LITERAL type is internal, the tokenizer
 * always return with LEXER_IDENT_LITERAL, and the
 * lexer_construct_literal_object returns with its
 * literal_type argument. The LEXER_VAR_LITERAL type
 * is only matters during the post processing phase.
 */
typedef enum
{
  LEXER_VAR_LITERAL = 0,       /**< local identifier literal */
  LEXER_IDENT_LITERAL = 1,     /**< identifier literal */
  LEXER_UNKNOWN_LITERAL = 2,   /**< unknown literal, can only be
                                *   used by the byte code generator. */
  LEXER_STRING_LITERAL = 3,    /**< string literal */
  LEXER_NUMBER_LITERAL = 4,    /**< number literal */
  LEXER_REGEXP_LITERAL = 5,    /**< regexp literal */
} lexer_literal_type;

/**
 * Immediate data.
 */
typedef struct
{
  /* Private part. */
  union
  {
    uint8_t *char_p;       /**< char array */
  } u;

  /* Public part (also used by the parser). */
  uint16_t length;         /**< length of ident / string literal */
  uint16_t index;          /**< real index during post processing */
  uint8_t type;            /**< type of the literal */
} lexer_literal;

int util_compare_char_literals (lexer_literal *, const uint8_t *);
int util_set_char_literal (lexer_literal *, const uint8_t *);
void util_free_literal (lexer_literal *);

#ifdef PARSER_DEBUG
void util_print_literal (lexer_literal *);
#endif /* PARSER_DEBUG */

/* Assertions */

#ifdef PARSER_DEBUG

#define PARSER_ASSERT(x) \
  do \
  { \
    if (!(x)) \
    { \
      printf ("Assertion failure in '%s' at line %d\n", __FILE__, __LINE__); \
      abort (); \
    } \
  } \
  while (0)

#else

#define PARSER_ASSERT(x)

#endif /* PARSER_DEBUG */

/* TRY/CATCH block */

#define PARSER_TRY_CONTEXT(context_name) \
  jmp_buf context_name;

#define PARSER_THROW(context_name) \
  longjmp (context_name, 1);

#define PARSER_TRY(context_name) \
  { \
    if (!setjmp (context_name)) \
    { \

#define PARSER_CATCH \
    } \
    else \
    {

#define PARSER_TRY_END \
    } \
  }

/* Other */

#define PARSER_INLINE inline
#define PARSER_NOINLINE __attribute__ ((noinline))

#ifdef PARSER_DEBUG
void util_print_string (const uint8_t *, size_t);
#endif

#endif /* COMMON_H */
