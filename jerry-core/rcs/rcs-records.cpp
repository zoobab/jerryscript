/* Copyright 2015 Samsung Electronics Co., Ltd.
 * Copyright 2015 University of Szeged
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
#include "rcs-records.h"

#include "rcs-allocator.h"
#include "rcs-cpointer.h"
#include "rcs-iterator.h"
#include "jrt-bit-fields.h"

/**
 * Set value of the record's field with specified offset and width.
 */
static void
rcs_record_set_field (record_t *rec_p, /**< record */
                      uint32_t field_pos, /**< offset, in bits */
                      uint32_t field_width, /**< width, in bits */
                      size_t value) /**< 32-bit unsigned integer value */
{
  rcs_check_record_alignment (rec_p);

  JERRY_ASSERT (sizeof (uint32_t) <= RCS_DYN_STORAGE_LENGTH_UNIT);
  JERRY_ASSERT (field_pos + field_width <= RCS_DYN_STORAGE_LENGTH_UNIT * JERRY_BITSINBYTE);

  uint32_t prev_value = *(uint32_t *) rec_p;
  *(uint32_t *) rec_p = (uint32_t) jrt_set_bit_field_value (prev_value, value, field_pos, field_width);
} /* rcs_record_set_field */

/**
 * Set the record's type identifier.
 */
void
rcs_record_set_type (record_t *rec_p, /**< record */
                     record_type_t type) /**< record type */
{
  JERRY_ASSERT (RCS_RECORD_TYPE_IS_VALID (type));

  rcs_record_set_field (rec_p, RCS_HEADER_TYPE_POS, RCS_HEADER_TYPE_WIDTH, type);
} /* rcs_record_set_type */

/**
 * Set previous record for the records.
 */
void
rcs_record_set_prev (record_set_t *rec_sec_p, /**< recordset */
                     record_t *rec_p, /**< record */
                     record_t *prev_p) /**< prev record */
{
  uint8_t begin_pos;

  switch (rcs_record_get_type (rec_p))
  {
    case RCS_RECORD_TYPE_CHARSET:
    {
      cpointer_t prev_cpointer = rcs_cpointer_compress (prev_p);
      rcs_iterator_ctx_t it_ctx = rcs_iterator_create (rec_sec_p, rec_p);

      rcs_iterator_skip (&it_ctx, RCS_DYN_STORAGE_LENGTH_UNIT);
      rcs_iterator_write (&it_ctx, &prev_cpointer.packed_value, sizeof (uint16_t));

      return;
    }
    case RCS_RECORD_TYPE_FREE:
    {
      begin_pos = RCS_FREE_HEADER_PREV_POS;
      break;
    }
    case RCS_RECORD_TYPE_MAGIC_STR:
    case RCS_RECORD_TYPE_MAGIC_STR_EX:
    {
      begin_pos = RCS_MAGIC_STR_HEADER_PREV_POS;
      break;
    }
    case RCS_RECORD_TYPE_NUMBER:
    {
      begin_pos = RCS_NUMBER_HEADER_PREV_POS;
      break;
    }
    default:
    {
      JERRY_UNREACHABLE ();
    }
  }

  cpointer_t cpointer = rcs_cpointer_compress (prev_p);
  rcs_record_set_field (rec_p, begin_pos, RCS_CPOINTER_WIDTH, cpointer.packed_value);
} /* rcs_record_set_prev */

/**
 * Set size of the records.
 */
void
rcs_record_set_size (record_t *rec_p, /**< recordset */
                     size_t size)  /**< record size */
{
  record_type_t type = rcs_record_get_type (rec_p);

  if (RCS_RECORD_TYPE_IS_CHARSET (type))
  {
    JERRY_ASSERT (JERRY_ALIGNUP (size, RCS_DYN_STORAGE_LENGTH_UNIT) == size);

    rcs_record_set_field (rec_p,
                          RCS_CHARSET_HEADER_LENGTH_POS,
                          RCS_CHARSET_HEADER_LENGTH_WIDTH,
                          size >> RCS_DYN_STORAGE_LENGTH_UNIT_LOG);
    return;
  }

  if (RCS_RECORD_TYPE_IS_FREE (type))
  {
    JERRY_ASSERT (JERRY_ALIGNUP (size, RCS_DYN_STORAGE_LENGTH_UNIT) == size);

    rcs_record_set_field (rec_p,
                          RCS_FREE_HEADER_LENGTH_POS,
                          RCS_FREE_HEADER_LENGTH_WIDTH,
                          size >> RCS_DYN_STORAGE_LENGTH_UNIT_LOG);
    return;
  }

  JERRY_ASSERT (rcs_record_get_size (rec_p) == size);
} /* rcs_record_set_size */

/**
 * Set the count of the alignment bytes at the end of record.
 */
void
rcs_record_set_alignment_bytes_count (record_t *rec_p, /**< record */
                                      size_t count)  /**< align bytes */
{
  JERRY_ASSERT (RCS_RECORD_IS_CHARSET (rec_p));

  rcs_record_set_field (rec_p, RCS_CHARSET_HEADER_ALIGN_POS, RCS_CHARSET_HEADER_ALIGN_WIDTH, count);
} /* rcs_record_set_align */

/**
 * Set the hash value of the record.
 */
void
rcs_record_set_hash (record_t *rec_p,  /**< record */
                     lit_string_hash_t hash)  /**< hash value */
{
  JERRY_ASSERT (RCS_RECORD_IS_CHARSET (rec_p));

  rcs_record_set_field (rec_p, RCS_CHARSET_HEADER_HASH_POS, RCS_CHARSET_HEADER_HASH_WIDTH, hash);
} /* rcs_record_set_hash */

/**
 * Set the charset of the record.
 */
void
rcs_record_set_charset (record_set_t *rec_set_p, /**< recordset containing the records */
                        record_t *rec_p, /**< record */
                        const lit_utf8_byte_t *str_p, /**< buffer containing characters to set */
                        lit_utf8_size_t size) /**< size of the buffer in bytes */
{
  JERRY_ASSERT (RCS_RECORD_IS_CHARSET (rec_p));
  JERRY_ASSERT (RCS_CHARSET_HEADER_SIZE + size
                == rcs_record_get_size (rec_p) - rcs_record_get_alignment_bytes_count (rec_p));

  rcs_iterator_ctx_t it_ctx = rcs_iterator_create (rec_set_p, rec_p);
  rcs_iterator_skip (&it_ctx, RCS_CHARSET_HEADER_SIZE);

  lit_utf8_size_t str_len = rcs_record_get_length (rec_p);
  lit_utf8_size_t i;

  for (i = 0; i < str_len; ++i)
  {
    rcs_iterator_write (&it_ctx, (void *)(str_p + i), sizeof (lit_utf8_byte_t));
    rcs_iterator_skip (&it_ctx, sizeof (lit_utf8_byte_t));
  }
} /* rcs_record_set_charset */

/**
 * Set the magic string id of the record.
 */
void
rcs_record_set_magic_str_id (record_t *rec_p, /**< record */
                             lit_magic_string_id_t id) /**< magic string id */
{
  JERRY_ASSERT (RCS_RECORD_IS_MAGIC_STR (rec_p));

  rcs_record_set_field (rec_p, RCS_MAGIC_STR_HEADER_ID_POS, RCS_MAGIC_STR_HEADER_ID_WIDTH, id);
} /* rcs_record_set_magic_str_id */

/**
 * Set the external magic string id of the record.
 */
void
rcs_record_set_magic_str_ex_id (record_t *rec_p, /**< record */
                                lit_magic_string_ex_id_t id) /**< external magic string id */
{
  JERRY_ASSERT (RCS_RECORD_IS_MAGIC_STR_EX (rec_p));

  rcs_record_set_field (rec_p, RCS_MAGIC_STR_HEADER_ID_POS, RCS_MAGIC_STR_HEADER_ID_WIDTH, id);
} /* rcs_record_set_magic_str_id */

/**
 * Get value of the record's field with specified offset and width.
 *
 * @return field's 32-bit unsigned integer value
 */
static uint32_t
rcs_record_get_field (record_t *rec_p, /**< record */
                      uint32_t field_pos, /**< offset, in bits */
                      uint32_t field_width) /**< width, in bits */
{
  rcs_check_record_alignment (rec_p);

  JERRY_ASSERT (sizeof (uint32_t) <= RCS_DYN_STORAGE_LENGTH_UNIT);
  JERRY_ASSERT (field_pos + field_width <= RCS_DYN_STORAGE_LENGTH_UNIT * JERRY_BITSINBYTE);

  uint32_t value = *(uint32_t *) rec_p;
  return (uint32_t) jrt_extract_bit_field (value, field_pos, field_width);
} /* rcs_record_get_field */

/**
 * Get value of the record's pointer field with specified offset and width.
 *
 * @return pointer to record
 */
static record_t *
rcs_record_get_pointer (record_t *rec_p, /**< record */
                        uint32_t field_pos, /**< offset, in bits */
                        uint32_t field_width) /**< width, in bits */
{
  cpointer_t cpointer;

  uint16_t value = (uint16_t) rcs_record_get_field (rec_p, field_pos, field_width);

  JERRY_ASSERT (sizeof (cpointer) == sizeof (cpointer.value));
  JERRY_ASSERT (sizeof (value) == sizeof (cpointer.value));

  cpointer.packed_value = value;

  return rcs_cpointer_decompress (cpointer);
} /* rcs_record_get_pointer */

/**
 * Get the record's type identifier.
 *
 * @return record type identifier
 */
record_type_t
rcs_record_get_type (record_t *rec_p) /**< record */
{
  JERRY_STATIC_ASSERT (sizeof (record_type_t) * JERRY_BITSINBYTE >= RCS_HEADER_TYPE_WIDTH);

  return (record_type_t) rcs_record_get_field (rec_p, RCS_HEADER_TYPE_POS, RCS_HEADER_TYPE_WIDTH);
} /* rcs_record_get_type */

/**
 * Get previous record for the records.
 *
 * @return previous record
 */
record_t *
rcs_record_get_prev (record_set_t *rec_sec_p,  /**< recordset */
                     record_t *rec_p)  /**< record */
{
  uint8_t begin_pos;

  switch (rcs_record_get_type (rec_p))
  {
    case RCS_RECORD_TYPE_CHARSET:
    {
      cpointer_t cpointer;
      rcs_iterator_ctx_t it_ctx = rcs_iterator_create (rec_sec_p, rec_p);

      rcs_iterator_skip (&it_ctx, RCS_DYN_STORAGE_LENGTH_UNIT);
      rcs_iterator_read (&it_ctx, &cpointer.packed_value, sizeof (uint16_t));

      return rcs_cpointer_decompress (cpointer);
    }
    case RCS_RECORD_TYPE_FREE:
    {
      begin_pos = RCS_FREE_HEADER_PREV_POS;
      break;
    }
    case RCS_RECORD_TYPE_MAGIC_STR:
    case RCS_RECORD_TYPE_MAGIC_STR_EX:
    {
      begin_pos = RCS_MAGIC_STR_HEADER_PREV_POS;
      break;
    }
    case RCS_RECORD_TYPE_NUMBER:
    {
      begin_pos = RCS_NUMBER_HEADER_PREV_POS;
      break;
    }
    default:
    {
      JERRY_UNREACHABLE ();
    }
  }

  return rcs_record_get_pointer (rec_p, begin_pos, RCS_CPOINTER_WIDTH);
} /* rcs_record_get_prev */

/**
 * Get the count of the alignment bytes at the end of record.
 * These bytes are needed to align the record to RCS_DYN_STORAGE_ALIGNMENT.
 *
 * @return alignment bytes count
 */
size_t
rcs_record_get_alignment_bytes_count (record_t *rec_p) /**< record */
{
  JERRY_ASSERT (RCS_RECORD_IS_CHARSET (rec_p));

  return rcs_record_get_field (rec_p, RCS_CHARSET_HEADER_ALIGN_POS, RCS_CHARSET_HEADER_ALIGN_WIDTH);
} /* rcs_record_get_alignment_bytes_count */

/**
 * Get hash value of the record's charset.
 *
 * @return hash value of the string
 */
lit_string_hash_t
rcs_record_get_hash (record_t *rec_p) /**< record */
{
  JERRY_ASSERT (RCS_RECORD_IS_CHARSET (rec_p));

  return (lit_string_hash_t) rcs_record_get_field (rec_p, RCS_CHARSET_HEADER_HASH_POS, RCS_CHARSET_HEADER_HASH_WIDTH);
} /* rcs_record_get_hash */

/**
 * Get header size of the records.
 *
 * @return size of the header in bytes
 */
size_t
rcs_header_get_size (record_t *rec_p) /**< record */
{
  if (RCS_RECORD_IS_CHARSET (rec_p))
  {
    return RCS_CHARSET_HEADER_SIZE;
  }

  return RCS_DYN_STORAGE_LENGTH_UNIT;
} /* rcs_header_get_size */

/**
 * Get size of the records.
 *
 * @return size of the record in bytes
 */
size_t
rcs_record_get_size (record_t *rec_p) /**< record */
{
  switch (rcs_record_get_type (rec_p))
  {
    case RCS_RECORD_TYPE_CHARSET:
    {
      size_t size = rcs_record_get_field (rec_p, RCS_CHARSET_HEADER_LENGTH_POS, RCS_CHARSET_HEADER_LENGTH_WIDTH);
      return (size * RCS_DYN_STORAGE_LENGTH_UNIT);
    }
    case RCS_RECORD_TYPE_FREE:
    {
      size_t size = rcs_record_get_field (rec_p, RCS_FREE_HEADER_LENGTH_POS, RCS_FREE_HEADER_LENGTH_WIDTH);
      return (size * RCS_DYN_STORAGE_LENGTH_UNIT);
    }
    case RCS_RECORD_TYPE_NUMBER:
    {
      return (RCS_DYN_STORAGE_LENGTH_UNIT + sizeof (ecma_number_t));
    }
    case RCS_RECORD_TYPE_MAGIC_STR:
    case RCS_RECORD_TYPE_MAGIC_STR_EX:
    {
      return RCS_DYN_STORAGE_LENGTH_UNIT;
    }
    default:
    {
      JERRY_UNREACHABLE ();
      return 0;
    }
  }
} /* rcs_record_get_size */

/**
 * Get the length of the string, which is contained inside the record.
 *
 * @return length of the string (bytes count)
 */
lit_utf8_size_t
rcs_record_get_length (record_t *rec_p) /**< record */
{
  JERRY_ASSERT (RCS_RECORD_IS_CHARSET (rec_p));

  size_t record_size = rcs_record_get_size (rec_p);
  size_t align_count = rcs_record_get_alignment_bytes_count (rec_p);

  return (lit_utf8_size_t) (record_size - RCS_CHARSET_HEADER_SIZE - align_count);
} /* rcs_record_get_length */

/**
 * Get magic string id which is held by the record.
 *
 * @return magic string id
 */
lit_magic_string_id_t
rcs_record_get_magic_str_id (record_t *rec_p) /**< record */
{
  JERRY_ASSERT (RCS_RECORD_IS_MAGIC_STR (rec_p));

  return (lit_magic_string_id_t) rcs_record_get_field (rec_p,
                                                       RCS_MAGIC_STR_HEADER_ID_POS,
                                                       RCS_MAGIC_STR_HEADER_ID_WIDTH);
} /* rcs_record_get_magic_str_id */

/**
 * Get external magic string id which is held by the record.
 *
 * @return external magic string id
 */
lit_magic_string_ex_id_t
rcs_record_get_magic_str_ex_id (record_t *rec_p) /**< record */
{
  JERRY_ASSERT (RCS_RECORD_IS_MAGIC_STR_EX (rec_p));

  return (lit_magic_string_ex_id_t) rcs_record_get_field (rec_p,
                                                          RCS_MAGIC_STR_HEADER_ID_POS,
                                                          RCS_MAGIC_STR_HEADER_ID_WIDTH);
} /* rcs_record_get_magic_str_id */

/**
 * Get the number which is held by the record.
 *
 * @return number
 */
ecma_number_t
rcs_record_get_number (record_set_t *rec_set_p, /**< recordset */
                       record_t *rec_p)  /**< record */
{
  JERRY_ASSERT (RCS_RECORD_IS_NUMBER (rec_p));

  rcs_iterator_ctx_t it_ctx = rcs_iterator_create (rec_set_p, rec_p);
  rcs_iterator_skip (&it_ctx, RCS_NUMBER_HEADER_SIZE);

  ecma_number_t value;
  rcs_iterator_read (&it_ctx, &value, sizeof (ecma_number_t));

  return value;
} /* rcs_record_get_number */

/**
 * Get the characters which are stored to the record.
 *
 * @return number of code units written to the buffer
 */
lit_utf8_size_t
rcs_record_get_charset (record_set_t *rec_set_p, /**< recordset */
                        record_t *rec_p, /**< record */
                        const lit_utf8_byte_t *buff_p, /**< output buffer */
                        size_t buff_size) /**< size of the output buffer in bytes */
{
  JERRY_ASSERT (RCS_RECORD_IS_CHARSET (rec_p));
  JERRY_ASSERT (buff_p && buff_size >= sizeof (lit_utf8_byte_t));

  rcs_iterator_ctx_t it_ctx = rcs_iterator_create (rec_set_p, rec_p);
  rcs_iterator_skip (&it_ctx, RCS_CHARSET_HEADER_SIZE);

  lit_utf8_size_t str_len = rcs_record_get_length (rec_p);
  lit_utf8_size_t i;

  for (i = 0; i < str_len && buff_size > 0; ++i)
  {
    rcs_iterator_read (&it_ctx, (void *)(buff_p + i), sizeof (lit_utf8_byte_t));
    rcs_iterator_skip (&it_ctx, sizeof (lit_utf8_byte_t));
    buff_size -= sizeof (lit_utf8_byte_t);
  }

  return i;
} /* rcs_record_get_charset */

/**
 * Compares two charset type records for equality.
 *
 * @return  true if strings inside records are equal
 *          false otherwise
 */
bool
rcs_record_is_equal (record_set_t *rec_set_p, /**< recordset */
                     record_t *l_rec_p, /**< left record */
                     record_t *r_rec_p) /**< rigth record */
{
  size_t l_rec_length = rcs_record_get_length (l_rec_p);
  size_t r_rec_length = rcs_record_get_length (r_rec_p);

  if (l_rec_length != r_rec_length)
  {
    return false;
  }

  rcs_iterator_ctx_t l_rec_it_ctx = rcs_iterator_create (rec_set_p, l_rec_p);
  rcs_iterator_ctx_t r_rec_it_ctx = rcs_iterator_create (rec_set_p, r_rec_p);

  rcs_iterator_skip (&l_rec_it_ctx, RCS_CHARSET_HEADER_SIZE);
  rcs_iterator_skip (&r_rec_it_ctx, RCS_CHARSET_HEADER_SIZE);

  lit_utf8_size_t i;
  for (i = 0; i < l_rec_length; ++i)
  {
    lit_utf8_byte_t l_chr;
    lit_utf8_byte_t r_chr;

    rcs_iterator_read (&l_rec_it_ctx, &l_chr, sizeof (lit_utf8_byte_t));
    rcs_iterator_read (&r_rec_it_ctx, &r_chr, sizeof (lit_utf8_byte_t));

    if (l_chr != r_chr)
    {
      return false;
    }

    rcs_iterator_skip (&l_rec_it_ctx, sizeof (lit_utf8_byte_t));
    rcs_iterator_skip (&r_rec_it_ctx, sizeof (lit_utf8_byte_t));
  }

  return true;
} /* rcs_record_is_equal */

/**
 * Compare a record with a string (which could contain '\0' characters) for equality.
 *
 * @return  true if compared instances are equal
 *          false otherwise
 */
bool
rcs_record_is_equal_utf8 (record_set_t *rec_set_p, /**< recordset */
                          record_t *rec_p, /**< record */
                          const lit_utf8_byte_t *str_p, /**< string to compare with */
                          lit_utf8_size_t str_size) /**< length of the string */
{
  JERRY_ASSERT (str_p != NULL);

  size_t rec_length = rcs_record_get_length (rec_p);

  if (rec_length != str_size)
  {
    return false;
  }

  rcs_iterator_ctx_t it_ctx = rcs_iterator_create (rec_set_p, rec_p);
  rcs_iterator_skip (&it_ctx, RCS_CHARSET_HEADER_SIZE);

  lit_utf8_size_t i;
  for (i = 0; i < rec_length; ++i)
  {
    lit_utf8_byte_t chr;
    rcs_iterator_read (&it_ctx, &chr, sizeof (lit_utf8_byte_t));

    if (chr != str_p[i])
    {
      return false;
    }

    rcs_iterator_skip (&it_ctx, sizeof (lit_utf8_byte_t));
  }

  return true;
} /* rcs_record_is_equal_utf8 */
