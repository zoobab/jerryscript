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

#ifndef JS_PARSER_DEFINES_H
#define JS_PARSER_DEFINES_H

#include "common.h"

#include "js-parser-limits.h"
#include "byte-code.h"
#include "js-lexer.h"

/* Expression parsing flags */
#define PARSE_EXPR                          0x00
#define PARSE_EXPR_STATEMENT                0x01
#define PARSE_EXPR_NO_COMMA                 0x02
#define PARSE_EXPR_HAS_IDENT                0x04

/* The maximum of PARSER_CBC_STREAM_PAGE_SIZE is 127. */
#define PARSER_CBC_STREAM_PAGE_SIZE (64 - sizeof (void*))
#define PARSER_STACK_PAGE_SIZE (((sizeof (void*) > 4) ? 128 : 64) - sizeof (void*))

/**
 * Parser boolean type.
 */
typedef enum
{
  PARSER_FALSE = 0,
  PARSER_TRUE = 1
} parser_boolean;

/**
 * Argument for a compact-byte code.
 */
typedef struct
{
  uint16_t literal_index;        /**< literal index argument */
  uint16_t value;                /**< other argument, literal type, etc. */
} cbc_argument;

#define PARSER_CBC_UNAVAILABLE CBC_EXT_OPCODE

#define PARSER_TO_EXT_OPCODE(opcode) ((opcode) + 256)
#define PARSER_GET_EXT_OPCODE(opcode) ((opcode) - 256)
#define PARSER_IS_BASIC_OPCODE(opcode) ((opcode) < 256)

#define PARSER_TO_BINARY_OPERATION_WITH_RESULT(opcode) \
  (PARSER_TO_EXT_OPCODE(opcode) - CBC_ASSIGN + CBC_EXT_ASSIGN_PUSH_RESULT)


/**
 * All data allocated by the parser is
 * stored in parser_data_pages in the memory.
 */
typedef struct parser_mem_page
{
  struct parser_mem_page *next_p;       /**< next page */
  uint8_t bytes[1];                     /**< memory bytes */
} parser_mem_page;

/**
 * Structure for managing parser memory.
 */
typedef struct
{
  parser_mem_page *first_p;             /**< first allocated page */
  parser_mem_page *last_p;              /**< last allocated page */
  size_t last_position;                 /**< position of the last allocated byte */
} parser_mem_data;

/**
 * Parser memory list.
 */
typedef struct
{
  parser_mem_data data;                 /**< storage space */
  size_t page_size;                     /**< size of each page */
  size_t item_size;                     /**< size of each item */
  size_t item_count;                    /**< number of items on each page */
} parser_list;

/**
 * Iterator for parser memory list.
 */
typedef struct
{
  parser_list *list_p;                  /**< parser list */
  parser_mem_page *current_p;           /**< currently processed page */
  size_t current_position;              /**< current position on the page */
} parser_list_iterator;

/**
 * Parser memory stack.
 */
typedef struct
{
  parser_mem_data data;                 /**< storage space */
  parser_mem_page *free_page_p;         /**< space for fast allocation */
} parser_stack;

/**
 * Iterator for parser memory stack.
 */
typedef struct
{
  parser_mem_page *current_p;           /**< currently processed page */
  size_t current_position;              /**< current position on the page */
} parser_stack_iterator;

/**
 * Branch type.
 */
typedef struct
{
  parser_mem_page *page_p;
  uint32_t offset;
} parser_branch;

/**
 * Branch chain type.
 */
typedef struct parser_branch_item
{
  struct parser_branch_item *next_p;
  parser_branch branch;
} parser_branch_item;

/**
 * Shared parser context.
 */
typedef struct
{
  PARSER_TRY_CONTEXT (try_buffer);      /**< try_buffer */
  char *error_str_p;                    /**< error string */

  /* Parser members. */
  int is_strict;                        /**< strict mode */
  int16_t stack_depth;                  /**< current stack depth */
  int16_t stack_max_depth;              /**< maximum stack depth */
  parser_stack_iterator last_statement; /**< last statement position */
#ifdef PARSER_DEBUG
  int is_show_opcodes;                  /**< show opcodes */
#endif

  /* Lexer members. */
  lexer_token token;                    /**< current token */
  const uint8_t *source_p;              /**< next source byte */
  const uint8_t *source_end_p;          /**< last source byte */
  lexer_line_counter line;              /**< current line */
  lexer_line_counter column;            /**< current column */

  /* Compact byte code members. */
  cbc_argument last_cbc;                /**< argument of the last cbc */
  uint16_t last_cbc_opcode;             /**< opcode of the last cbc */

  /* Literal types */
  uint16_t var_count;                   /**< number of var identifier references */
  uint16_t ident_count;                 /**< number of identifier references */
  uint16_t other_count;                 /**< number of other literals */

  /* Memory storage members. */
  parser_mem_data byte_code;            /**< byte code buffer */
  uint32_t byte_code_size;              /**< byte code size for branches */
  parser_list literal_pool;             /**< literal list */
  parser_mem_data stack;                /**< storage space */
  parser_mem_page *free_page_p;         /**< space for fast allocation */
  uint8_t stack_top_uint8;              /**< top byte stored on the stack */
} parser_context;

/* Memory management.
 * Note: throws an error if unsuccessful. */
void * parser_malloc (parser_context *, size_t);
void parser_free (void *);
void * parser_malloc_local (parser_context *, size_t);
void parser_free_local (void *);

/* Parser byte stream. */

void parser_cbc_stream_init (parser_mem_data *);
void parser_cbc_stream_free (parser_mem_data *);
void parser_cbc_stream_alloc_page (parser_context *, parser_mem_data *);

/* Parser list. Ensures pointer alignment. */

void parser_list_init (parser_list *, size_t, size_t);
void parser_list_free (parser_list *);
void * parser_list_append (parser_context *, parser_list *);
void * parser_list_get (parser_list *, size_t);
void parser_list_iterator_init (parser_list *, parser_list_iterator *);
void * parser_list_iterator_next (parser_list_iterator *);

/* Parser stack. Optimized for pushing bytes.
 * Pop functions never throws error. */

void parser_stack_init (parser_context *);
void parser_stack_free (parser_context *);
void parser_stack_push_uint8 (parser_context *, uint8_t);
void parser_stack_pop_uint8 (parser_context *);
void parser_stack_push_uint16 (parser_context *, uint16_t);
uint16_t parser_stack_pop_uint16 (parser_context *);
void parser_stack_push (parser_context *, const void *, size_t);
void parser_stack_pop (parser_context *, void *, size_t);
void parser_stack_iterator_skip (parser_stack_iterator *, size_t);
void parser_stack_iterator_read (parser_stack_iterator *, void *, size_t);
void parser_stack_iterator_write (parser_stack_iterator *, const void *, size_t);

/* Compact byte code emitting functions. */

void parser_flush_cbc (parser_context *);
void parser_emit_cbc (parser_context *, cbc_opcode);
void parser_emit_cbc_ext (parser_context *, cbc_ext_opcode);
void parser_emit_cbc_literal (parser_context *, cbc_opcode, uint16_t);
void parser_emit_cbc_literal_from_token (parser_context *, cbc_opcode);
void parser_emit_cbc_call (parser_context *, cbc_opcode, size_t);
void parser_emit_cbc_forward_branch (parser_context *, cbc_opcode, parser_branch *);
void parser_emit_cbc_ext_forward_branch (parser_context *, cbc_ext_opcode, parser_branch *);
parser_branch_item *parser_emit_cbc_forward_branch_item (parser_context *, cbc_opcode, parser_branch_item *);
void parser_emit_cbc_backward_branch (parser_context *, cbc_opcode, uint32_t);
void parser_set_branch_to_current_position (parser_context *, parser_branch *);
void parser_set_breaks_to_current_position (parser_context *, parser_branch_item *);
void parser_set_continues_to_current_position (parser_context *, parser_branch_item *);

/* Lexer functions */

void lexer_next_token (parser_context *);
void lexer_expect_identifier (parser_context *, uint8_t);
void lexer_construct_literal_object (parser_context *, lexer_char_literal *, uint8_t);
int lexer_same_identifiers (lexer_char_literal *, lexer_char_literal *);

/* Parser functions. */

void parser_parse_expression (parser_context *, int);
void parser_parse_statements (parser_context *);
void parser_free_jumps (parser_context *);

/* Other functions. */

void parser_raise_error (parser_context *, char *);

/* Note: source must be a valid UTF-8 string. */
void parser_parse_script (const uint8_t *, size_t);

#endif /* JS_PARSER_DEFINES_H */
