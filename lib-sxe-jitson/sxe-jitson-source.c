/*
 * Copyright (c) 2022 Cisco Systems, Inc. and its affiliates
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include <ctype.h>

#include "sxe-jitson.h"
#include "sxe-log.h"

/* This is a 256 bit little endian bitmask. For each unsigned char, the bit is set if the char is valid in an identifier
 */
static uint64_t identifier_chars[4] = {0x03FF400000000000, 0x07FFFFFE87FFFFFE, 0x0000000000000000, 0x00000000000000};

/**
 * Construct a source from possibly non-NUL terminated JSON
 *
 * @param source The source to construct
 * @param string JSON to be parsed
 * @param len    Length of the JSON to be parsed
 * @param flags  Flags affecting the parsing. 0 for strict JSON, or one or more of SXE_JITSON_FLAG_ALLOW_HEX,
 *               SXE_JITSON_FLAG_ALLOW_CONSTS or SXE_JITSON_FLAG_ALLOW_IDENTS
 *
 * @note SXE_JITSON_FLAG_ALLOW_IDENTS has no effect if sxe_jitson_ident_register has not been called
 */
void
sxe_jitson_source_from_buffer(struct sxe_jitson_source *source, const char *string, size_t len, uint32_t flags)
{
    source->json  = string;
    source->next  = string;
    source->end   = string + len;
    source->flags = flags;
}

/**
 * Construct a source from a JSON string
 *
 * @param source The source to construct
 * @param string A JSON string to be parsed
 * @param flags  Flags affecting the parsing. 0 for strict JSON, or one or more of SXE_JITSON_FLAG_ALLOW_HEX,
 *               SXE_JITSON_FLAG_ALLOW_CONSTS or SXE_JITSON_FLAG_ALLOW_IDENTS
 *
 * @note SXE_JITSON_FLAG_ALLOW_IDENTS has no effect if sxe_jitson_ident_register has not been called
 */
void
sxe_jitson_source_from_string(struct sxe_jitson_source *source, const char *string, uint32_t flags)
{
    source->json  = string;
    source->next  = string;
    source->end   = (const char *)~0ULL;    // Just use the NUL terminator. This allows us to not take the strlen.
    source->flags = flags;
}

/* Get the next character in the source, returning '\0' on end of data
 */
char
sxe_jitson_source_get_char(struct sxe_jitson_source *source)
{
    return source->next >= source->end ? '\0' : *source->next++;
}

/* Push the last character read back to the source
 */
void
sxe_jitson_source_push_char(struct sxe_jitson_source *source, char c)
{
    SXEA1(source->json < source->next, "Can't push back a character when no characters have been got yet");
    SXEA1(*--source->next == c, "Attempt to push back a character that is not the one just you just got");
}

/* Return the next character without consuming it, or '\0' on end of data
 */
static unsigned char
source_peek_next(struct sxe_jitson_source *source)
{
    return source->next >= source->end ? '\0' : *source->next;
}

/* Skip whitespace characters in the source, returning the first nonspace character or '\0' on end of data
 */
char
sxe_jitson_source_get_nonspace(struct sxe_jitson_source *source)
{
    while (isspace(source_peek_next(source)))
        source->next++;

    return sxe_jitson_source_get_char(source);
}

/**
 * Get identifier characters until a non-identifier character is reached.
 *
 * @param source  Source to parse
 * @param len_out Set to the length of the identifier or 0 if there is no valid identifier
 *
 * @return Pointer to the identifier or NULL if there is no valid identifier
 *
 * @note If the first identifier character has stricter limitations than subsequent characters, you must check that outside
 */
const char *
sxe_jitson_source_get_identifier(struct sxe_jitson_source *source, size_t *len_out)
{
    const char   *identifier = source->next;

    /* While the next character's bit is set in the 256 bit identiefier_char bitmask
     */
    while (identifier_chars[source_peek_next(source) >> 6] & (1UL << (source_peek_next(source) & 0x3f)))
        source->next++;

    return (*len_out = source->next - identifier) ? identifier : NULL;
}

/**
 * Parse identifier characters from a string until a non-identifier character is reached.
 *
 * @param json String to parse
 *
 * @return Pointer to first non-identifier character in json
 *
 * @note Can be called after parsing the first character.
 */
const char *
sxe_jitson_parse_identifier(const char *json)
{
    struct sxe_jitson_source source;
    size_t                   len;

    sxe_jitson_source_from_string(&source, json, sxe_jitson_flags);
    sxe_jitson_source_get_identifier(&source, &len);
    return json + len;
}

/**
 * Get characters until a non-number character is reached.
 *
 * @param source      Source to parse
 * @param len_out     Set to the length of the number as a string
 * @param is_uint_out Set to true if the value is a uint, false if not
 *
 * @return Pointer to the number as a string or NULL if there is no valid number
 */
const char *
sxe_jitson_source_get_number(struct sxe_jitson_source *source, size_t *len_out, bool *is_uint_out)
{
    const char *number = source->next;

    *is_uint_out = true;

    if (source_peek_next(source) == '-') {
        *is_uint_out = false;
        source->next++;
    }

    if (!isdigit(source_peek_next(source)))
        goto INVALID;

    source->next++;

    /* If hex is allowed and the number starts with '0x', its a hexadecimal unsigned integer
     */
    if ((source->flags & SXE_JITSON_FLAG_ALLOW_HEX) && *number == '0' && source_peek_next(source) == 'x') {
        for (source->next++; isxdigit(source_peek_next(source)); source->next++) {
        }

        if (source->next - number <= 2)    // Can't be just '0x'
            goto INVALID;

        *len_out = source->next - number;
        return number;
    }

    while (isdigit(source_peek_next(source)))
        source->next++;

    if (source_peek_next(source) == '.') {    // If there's a fraction
        *is_uint_out = false;
        source->next++;

        if (!isdigit(source_peek_next(source)))
            goto INVALID;

        while (isdigit(source_peek_next(source)))
            source->next++;
    }

    if (source_peek_next(source) == 'E' || source_peek_next(source) == 'e') {    // If there's an exponent
        *is_uint_out = false;
        source->next++;

        if (source_peek_next(source) == '-' || source_peek_next(source) == '+')
            source->next++;

        if (!isdigit(source_peek_next(source)))
            goto INVALID;

        while (isdigit(source_peek_next(source)))
            source->next++;
    }

    *len_out = source->next - number;
    return number;

INVALID:
    errno = EINVAL;
    return NULL;
}
