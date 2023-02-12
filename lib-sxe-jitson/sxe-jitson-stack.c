/* Copyright (c) 2021 Jim Belton
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* SXE jitson stacks are factories for building sxe-jitson.
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mockfail.h"
#include "sxe-alloc.h"
#include "sxe-hash.h"
#include "sxe-jitson.h"
#include "sxe-log.h"
#include "sxe-thread.h"
#include "sxe-unicode.h"

#define JITSON_STACK_INIT_SIZE 1       // The initial numer of tokens in a per thread stack
#define JITSON_STACK_MAX_INCR  4096    // The maximum the stack will grow by

/* A per thread stack is kept for parsing. It's per thread for lockless thread safety, and automatically grows as needed.
 */
static unsigned                            jitson_stack_init_size = JITSON_STACK_INIT_SIZE;
static struct sxe_jitson                  *jitson_constants       = NULL;
static __thread struct sxe_jitson_stack   *jitson_stack           = NULL;

/* Hook to allow parser access to unmatched identifiers. This is a non-standard extension
 */
bool (*sxe_jitson_stack_push_ident)(struct sxe_jitson_stack *stack, uint32_t index, const char *ident, size_t len) = NULL;

/**
 * Initialize the stack/parser module for non-standard JSON extensions (not required for standard JSON)
 *
 * @param constants A set of identifiers to be replaced with constant jitson values when parsing.
 *
 * @note Values are duplicated in the parsed jitson. If you want to include a large object or array, consider making the value
 *       a reference to it if it may appear more than once in the parsed JSON, but then beware the lifetime of the referenced
 *       object or array.
 */
void
sxe_jitson_stack_init(struct sxe_jitson *constants)
{
    jitson_constants = constants;
    sxe_jitson_flags |= SXE_JITSON_FLAG_ALLOW_CONSTS;
}

void
sxe_jitson_stack_fini(void)
{
    sxe_jitson_free(jitson_constants);
    jitson_constants = NULL;
}

static bool
sxe_jitson_stack_make(struct sxe_jitson_stack *stack, unsigned init_size)
{
    SXEA1(SXE_JITSON_TOKEN_SIZE == 16, "Expected token size 16, got %zu", SXE_JITSON_TOKEN_SIZE);

    if (!stack)
        return false;

    memset(stack, 0, sizeof(*stack));
    stack->maximum = init_size;
    stack->jitsons = MOCKFAIL(MOCK_FAIL_STACK_NEW_JITSONS, NULL, sxe_malloc((size_t)init_size * sizeof(*stack->jitsons)));
    return stack->jitsons ? true : false;
}

struct sxe_jitson_stack *
sxe_jitson_stack_new(unsigned init_size)
{
    struct sxe_jitson_stack *stack = MOCKFAIL(MOCK_FAIL_STACK_NEW_OBJECT, NULL, sxe_malloc(sizeof(*stack)));

    if (!sxe_jitson_stack_make(stack, init_size)) {
        sxe_free(stack);
        return NULL;
    }

    return stack;
}

/**
 * Extract the jitson parsed or constructed on a stack
 *
 * @param The stack
 *
 * @note Aborts if there is no jitson on the stack or if there is a partially constructed one
 */
struct sxe_jitson *
sxe_jitson_stack_get_jitson(struct sxe_jitson_stack *stack)
{
    struct sxe_jitson *ret = stack->jitsons;

    SXEA1(stack->jitsons, "Can't get a jitson from an empty stack");
    SXEA1(!stack->open,   "Can't get a jitson there's an open collection");
    SXEE6("(stack=%p)", stack);

    if (stack->maximum > stack->count)
        ret = sxe_realloc(ret, stack->count * sizeof(*stack->jitsons)) ?: stack->jitsons;

    stack->jitsons = NULL;
    stack->count   = 0;
    ret->type     |= SXE_JITSON_TYPE_ALLOCED;    // The token at the base of the stack is marked as allocated

    SXER6("return %p; // type=%s", ret, ret ? sxe_jitson_type_to_str(sxe_jitson_get_type(ret)) : "NONE");
    return ret;
}

/**
 * Clear the content of a parse stack
 */
void
sxe_jitson_stack_clear(struct sxe_jitson_stack *stack)
{
    stack->count = 0;
    stack->open  = 0;
}

/**
 * Return a per thread stack, constructing it on first call.
 *
 * @note The stack can be freed after the thread exits by calling sxe_thread_memory_free
 */
struct sxe_jitson_stack *
sxe_jitson_stack_get_thread(void)
{
    /* Allocate the per thread stack; once allocated, we can't free it because its being tracked
     */
    if (!jitson_stack) {
        jitson_stack = sxe_thread_malloc(sizeof(*jitson_stack), (void (*)(void *))sxe_jitson_stack_free, NULL);

        if (!sxe_jitson_stack_make(jitson_stack, jitson_stack_init_size)) {
            SXEL2(": failed to create a sxe-jitson per thread stack");
            return NULL;
        }
    }

    SXEL6(": return %p; // count=%u, open=%u", jitson_stack, jitson_stack ? jitson_stack->count : 0,
          jitson_stack ? jitson_stack->open : 0);
    return jitson_stack;
}

void
sxe_jitson_stack_free(struct sxe_jitson_stack *stack)
{
    sxe_free(stack->jitsons);
    sxe_free(stack);
}

/* Reserve space on stack, expanding it if needed to make room for at least 'more' new values
 *
 * @return The index of the first new slot on the stack, or SXE_JITSON_STACK_ERROR on error (ENOMEM)
 */
unsigned
sxe_jitson_stack_expand(struct sxe_jitson_stack *stack, unsigned more)
{
    unsigned expanded = stack->count + more;

    if (expanded > stack->maximum) {
        unsigned new_maximum;

        if (expanded < JITSON_STACK_MAX_INCR)
            new_maximum = ((expanded - 1) / stack->maximum + 1) * stack->maximum;
        else
            new_maximum = ((expanded - 1) / JITSON_STACK_MAX_INCR + 1) * JITSON_STACK_MAX_INCR;

        struct sxe_jitson *new_jitsons = MOCKFAIL(MOCK_FAIL_STACK_EXPAND, NULL,
                                                  sxe_realloc(stack->jitsons, (size_t)new_maximum * sizeof(*stack->jitsons)));

        if (!new_jitsons) {
            SXEL2(": Failed to expand the stack to %u jitsons from %u", new_maximum, stack->maximum);
            return SXE_JITSON_STACK_ERROR;
        }

        stack->maximum = new_maximum;
        stack->jitsons = new_jitsons;    // If the array moved, point current into the new one.
    }
    else if (!stack->jitsons && !(stack->jitsons = MOCKFAIL(MOCK_FAIL_STACK_EXPAND_AFTER_GET, NULL,
                                                            sxe_malloc(((size_t)stack->maximum * sizeof(*stack->jitsons)))))) {
        SXEL2(": Failed to allocate %u jitsons for the stack", stack->maximum);
        return SXE_JITSON_STACK_ERROR;
    }

    stack->count = expanded;
    return expanded - more;
}

/**
 * Load a JSON string from a source, returning true on success, false on error
 *
 * @note The initial '"' character must already have been consumed from the source
 */
static bool
sxe_jitson_stack_load_string(struct sxe_jitson_stack *stack, struct sxe_jitson_source *source, bool is_member_name)
{
    unsigned index = sxe_jitson_stack_expand(stack, 1);

    if (index == SXE_JITSON_STACK_ERROR)
        return false;

    char               c, utf8[4];
    unsigned           i;
    unsigned           unicode;
    struct sxe_jitson *jitson  = &stack->jitsons[index];
    jitson->type               = SXE_JITSON_TYPE_STRING | (is_member_name ? SXE_JITSON_TYPE_IS_KEY : 0);
    jitson->len                = 0;

    while ((c = sxe_jitson_source_get_char(source)) != '"') {
        if (c == '\0') {   // No terminating "
            errno = EINVAL;
            goto ERROR;
        }

        i = 1;

        if (c == '\\')
            switch (c = sxe_jitson_source_get_char(source)) {
            case '"':
            case '\\':
            case '/':
                jitson->string[jitson->len++] = c;
                break;

            case 'b':
                jitson->string[jitson->len++] = '\b';
                break;

            case 'f':
                jitson->string[jitson->len++] = '\f';
                break;

            case 'n':
                jitson->string[jitson->len++] = '\n';
                break;

            case 'r':
                jitson->string[jitson->len++] = '\r';
                break;

            case 't':
                jitson->string[jitson->len++] = '\t';
                break;

            case 'u':
                for (i = 0, unicode = 0; i < 4; i++)
                    switch (c = sxe_jitson_source_get_char(source)) {
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                        unicode = (unicode << 4) + c - '0';
                        break;

                    case 'a':
                    case 'b':
                    case 'c':
                    case 'd':
                    case 'e':
                    case 'f':
                        unicode = (unicode << 4) + c - 'a' + 10;
                        break;

                    case 'A':
                    case 'B':
                    case 'C':
                    case 'D':
                    case 'E':
                    case 'F':
                        unicode = (unicode << 4) + c - 'A' + 10;
                        break;

                    default:
                        errno = EILSEQ;
                        goto ERROR;
                    }

                i = sxe_unicode_to_utf8(unicode, utf8);
                jitson->string[jitson->len++] = utf8[0];
                break;

            default:
                errno = EILSEQ;
                goto ERROR;
            }
        else
            jitson->string[jitson->len++] = c;

        unsigned edge = jitson->len % SXE_JITSON_TOKEN_SIZE;

        if (edge >= SXE_JITSON_TOKEN_SIZE / 2 && edge < SXE_JITSON_TOKEN_SIZE / 2 + i) {
            if (sxe_jitson_stack_expand(stack, 1) == SXE_JITSON_STACK_ERROR)
                goto ERROR;

            jitson = &stack->jitsons[index];    // In case the jitsons were moved by realloc
        }

        if (i > 1) {    // For UTF8 strings > 1 character, copy the rest of the string
            memcpy(&jitson->string[jitson->len], &utf8[1], i - 1);
            jitson->len += i - 1;
        }
    }

    jitson->string[jitson->len] = '\0';
    return true;

ERROR:
    stack->count = index;    // Discard any data that this function added to the stack
    return false;
}

/* Internal function to add a duplicate to the stack without first making room for it. Be careful.
 */
static bool
sxe_jitson_stack_dup_at_index(struct sxe_jitson_stack *stack, unsigned index, const struct sxe_jitson *value, unsigned size)
{
    memcpy(&stack->jitsons[index], value, size * sizeof(*value));
    bool ret = sxe_jitson_clone(value, &stack->jitsons[index]);      // If the type requires a deep clone, do it
    stack->jitsons[index].type &= ~SXE_JITSON_TYPE_ALLOCED;          // Clear the allocation flag if any
    return ret;
}

/* Character classes used by the parser
 */
#define INV 0    // Invalid (control character, non-ASCII character, etc.)
#define SYM 1    // A standalone symbol not used by JSON
#define QOT 2    // A quote-like character
#define DIG 4    // A decimal digit
#define ALP 5    // An alphabetic character, including _ and upper and lower case letters

/* Map from char to character class. Characters special to JSON are often a class of their own.
 */
static char jitson_class[256] = { INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                                  INV, SYM, '"', SYM, SYM, SYM, SYM, QOT, '(', ')', SYM, SYM, SYM, '-', SYM, SYM,
                                  DIG, DIG, DIG, DIG, DIG, DIG, DIG, DIG, DIG, DIG, SYM, SYM, SYM, SYM, SYM, SYM,
                                  SYM, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP,
                                  ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, '[', SYM, ']', SYM, ALP,
                                  QOT, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP,
                                  ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, '{', SYM, '}', SYM, INV,
                                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV };

/**
 * Load a JSON onto a sxe-jitson stack.
 *
 * @param stack  The stack to load onto
 * @param source A sxe_jitson_source object
 *
 * @return true if the JSON was successfully parsed or false on error
 *
 * @note On error, any jitson values partially parsed onto the stack will be cleared.
 */
bool
sxe_jitson_stack_load_json(struct sxe_jitson_stack *stack, struct sxe_jitson_source *source)
{
    const struct sxe_jitson *jitson;
    const char              *token;
    char                    *endptr;
    size_t                   len;
    unsigned                 index, size;
    bool                     is_uint;
    char                     c;

    if ((c = sxe_jitson_source_get_nonspace(source)) == '\0') {    // Nothing but whitespace
        errno = ENODATA;
        return false;
    }

    if ((index = sxe_jitson_stack_expand(stack, 1)) == SXE_JITSON_STACK_ERROR)    // Get an empty jitson
        return false;

    switch (jitson_class[(unsigned char)c]) {
    case '"':               // It's a string
        stack->count--;    // Return the jitson just allocated. The load_string function will get it back.
        return sxe_jitson_stack_load_string(stack, source, false);

    case '{':    // It's an object
        stack->jitsons[index].type = SXE_JITSON_TYPE_OBJECT;
        stack->jitsons[index].len  = 0;

        if ((c = sxe_jitson_source_get_nonspace(source)) == '}') {    // If it's an empty object, return it
            stack->jitsons[index].integer = 1;                        // Save the size in jitsons
            return true;
        }

        do {
            if (c != '"')
                goto INVALID;

            if (!sxe_jitson_stack_load_string(stack, source, true))    // Member name must be a string
                goto ERROR;

            if ((c = sxe_jitson_source_get_nonspace(source)) != ':')
                goto INVALID;

            if (!sxe_jitson_stack_load_json(stack, source))    // Value can be any JSON value
                goto ERROR;

            stack->jitsons[index].len++;
            c = sxe_jitson_source_get_nonspace(source);
        } while (c == ',' && (c = sxe_jitson_source_get_nonspace(source)));

        if (c == '}') {
            stack->jitsons[index].integer = stack->count - index;    // Store the size = offset past the object
            return true;
        }

        goto INVALID;

    case '[':    // It's an array
        stack->jitsons[index].type = SXE_JITSON_TYPE_ARRAY;
        stack->jitsons[index].len  = 0;

        if ((c = sxe_jitson_source_get_nonspace(source)) == ']') {   // If it's an empty array, return it
            stack->jitsons[index].integer = 1;    // Offset past the empty array
            return true;
        }

        sxe_jitson_source_push_char(source, c);

        do {
            if (!sxe_jitson_stack_load_json(stack, source))    // Value can be any JSON value
                goto ERROR;

            stack->jitsons[index].len++;
        } while ((c = sxe_jitson_source_get_nonspace(source)) == ',');

        if (c == ']') {
            stack->jitsons[index].integer = stack->count - index;    // Store the offset past the object
            return true;
        }

        goto INVALID;

    case '-':
    case DIG:
        sxe_jitson_source_push_char(source, c);

        if ((token = sxe_jitson_source_get_number(source, &len, &is_uint)) == NULL)
            goto ERROR;

        if (is_uint) {
            stack->jitsons[index].type    = SXE_JITSON_TYPE_NUMBER | SXE_JITSON_TYPE_IS_UINT;
            stack->jitsons[index].integer = strtoul(token, &endptr,
                                                    sxe_jitson_source_get_flags(source) & SXE_JITSON_FLAG_ALLOW_HEX ? 0 : 10);
            SXEA6(endptr - token == (ptrdiff_t)len, "strtoul failed to parse '%.*s'", (int)len, token);
        } else {
            stack->jitsons[index].type   = SXE_JITSON_TYPE_NUMBER;
            stack->jitsons[index].number = strtod(token, &endptr);
            SXEA6(endptr  - token == (ptrdiff_t)len, "strtod failed to parse '%.*s'", (int)len, token);
        }

        return true;

    case ALP:
        sxe_jitson_source_push_char(source, c);
        token = sxe_jitson_source_get_identifier(source, &len);    // Can't fail if all alphas are valid identifier characters

        if (len == sizeof("false") - 1 && memcmp(token, "false", sizeof("false") - 1) == 0) {
            stack->jitsons[index].type    = SXE_JITSON_TYPE_BOOL;
            stack->jitsons[index].boolean = false;
            return true;
        }

        if (len == sizeof("null") - 1 && memcmp(token, "null", sizeof("null") - 1) == 0) {
            stack->jitsons[index].type = SXE_JITSON_TYPE_NULL;
            return true;
        }

        if (len == sizeof("true") - 1 && memcmp(token, "true", sizeof("true") - 1) == 0) {
            stack->jitsons[index].type    = SXE_JITSON_TYPE_BOOL;
            stack->jitsons[index].boolean = true;
            return true;
        }

        if (jitson_constants && (sxe_jitson_source_get_flags(source) & SXE_JITSON_FLAG_ALLOW_CONSTS)
         && (jitson = sxe_jitson_object_get_member(jitson_constants, token, len))) {
            if (((size = sxe_jitson_size(jitson)) > 1) && sxe_jitson_stack_expand(stack, size - 1) == SXE_JITSON_STACK_ERROR)
                goto ERROR;

            sxe_jitson_stack_dup_at_index(stack, index, jitson, size);    // Duplicate the constant's value
            return true;
        }

        if (sxe_jitson_stack_push_ident && (sxe_jitson_source_get_flags(source) & SXE_JITSON_FLAG_ALLOW_IDENTS)) {
            if (!sxe_jitson_stack_push_ident(stack, index, token, len))
                goto ERROR;

            return true;
        }

        if (jitson_constants && (sxe_jitson_source_get_flags(source) & SXE_JITSON_FLAG_ALLOW_CONSTS))
            SXEL6(": Identifier '%.*s' is neither a JSON keyword nor a registered constant", (int)len, token);
        else
            SXEL6(": Identifier '%.*s' is not a JSON keyword", (int)len, token);

        /* FALL THRU */

    default:
        break;
    }

INVALID:
    errno = EINVAL;

ERROR:
    stack->count = index;    // Discard any data that this function added to the stack
    return false;
}

/**
 * Parse a JSON from a string onto a sxe-jitson stack.
 *
 * @return Pointer into the json string to the character after the JSON parsed or NULL on error
 *
 * @note On error, any jitson values partially parsed onto the stack will be cleared.
 */
const char *
sxe_jitson_stack_parse_json(struct sxe_jitson_stack *stack, const char *json)
{
    struct sxe_jitson_source source;

    sxe_jitson_source_from_string(&source, json, sxe_jitson_flags);

    if (!sxe_jitson_stack_load_json(stack, &source))
        return NULL;

    return json + sxe_jitson_source_get_consumed(&source);
}

static unsigned
sxe_jitson_stack_add_value(struct sxe_jitson_stack *stack, unsigned size)
{
    unsigned collection = stack->open - 1;
    unsigned index;

    SXEA1(stack->open, "Can't add a value when there is no array or object under construction");
    SXEA1(stack->jitsons[collection].type == SXE_JITSON_TYPE_ARRAY || stack->jitsons[collection].partial.no_value,
          "Member name must be added added to an object before adding a value");
    SXEA1(stack->jitsons[collection].type == SXE_JITSON_TYPE_OBJECT || stack->jitsons[collection].type == SXE_JITSON_TYPE_ARRAY,
          "Values can only be added to arrays or objects");

    if ((index = sxe_jitson_stack_expand(stack, size)) == SXE_JITSON_STACK_ERROR)
        return SXE_JITSON_STACK_ERROR;

    stack->jitsons[collection].len++;
    stack->jitsons[collection].partial.no_value = 0;    // (uint8_t)false
    return index;
}

/**
 * Begin construction of an object on a stack
 *
 * @param type SXE_JITSON_TYPE_OBJECT or SXE_JITSON_TYPE_ARRAY
 *
 * @return true on success, false on allocation failure
 */
bool
sxe_jitson_stack_open_collection(struct sxe_jitson_stack *stack, uint32_t type)
{
    unsigned index;

    SXEA6(type == SXE_JITSON_TYPE_ARRAY || type == SXE_JITSON_TYPE_OBJECT, "Only arrays and objects can be constructed");

    /* Add the collection jitson; if there's already an open collection, the collection being opened is a value
     */
    if ((index = (stack->open ? sxe_jitson_stack_add_value(stack, 1) : sxe_jitson_stack_expand(stack, 1))) == SXE_JITSON_STACK_ERROR)
        return false;    /* COVERAGE EXCLUSION: Out of memory condition */

    stack->jitsons[index].type               = type;
    stack->jitsons[index].len                = 0;
    stack->jitsons[index].partial.no_value   = 0;              // (uint8_t)false
    stack->jitsons[index].partial.nested     = 0;              // (uint8_t)false
    stack->jitsons[index].partial.collection = stack->open;
    stack->open                              = index + 1;
    return true;
}

/**
 * Add a string to the stack.
 *
 * @param stack  The jitson stack
 * @param string The string
 * @param type   SXE_JITSON_TYPE_IS_COPY, SXE_JITSON_TYPE_IS_REF, or SXE_JITSON_TYPE_IS_OWN
 *
 * @return true on success, false on out of memory (ENOMEM) or copied string too long (ENAMETOOLONG)
 *
 * @note Called internally, type may include SXE_JITSON_TYPE_IS_KEY
 */
bool
sxe_jitson_stack_push_string(struct sxe_jitson_stack *stack, const char *string, uint32_t type)
{
    unsigned index;

    if ((index = sxe_jitson_stack_expand(stack, 1)) == SXE_JITSON_STACK_ERROR)
        return false;    /* COVERAGE EXCLUSION: Out of memory condition */

    type                       = type & SXE_JITSON_TYPE_IS_OWN ? SXE_JITSON_TYPE_IS_REF | type : type;
    stack->jitsons[index].type = SXE_JITSON_TYPE_STRING | type;

    if (type & SXE_JITSON_TYPE_IS_REF) {    // Not a copy (a reference, possibly giving ownership to the object)
        stack->jitsons[index].reference = string;
        stack->jitsons[index].len       = 0;
        return true;
    }

    size_t len = strlen(string);

    if ((uint32_t)len != len) {
        errno = ENAMETOOLONG;    /* COVERAGE EXCLUSION: Copied string > 4294967295 characters */
        return false;            /* COVERAGE EXCLUSION: Copied string > 4294967295 characters */
    }

    stack->jitsons[index].len = len;

    if (len < SXE_JITSON_STRING_SIZE) {
        memcpy(stack->jitsons[index].string, string, len + 1);
        return true;
    }

    memcpy(stack->jitsons[index].string, string, SXE_JITSON_STRING_SIZE);

    for (string += SXE_JITSON_STRING_SIZE, len -= SXE_JITSON_STRING_SIZE; ; len -= SXE_JITSON_TOKEN_SIZE) {
        if ((index = sxe_jitson_stack_expand(stack, 1)) == SXE_JITSON_STACK_ERROR)
            return false;    /* COVERAGE EXCLUSION: Out of memory condition */

        if (len < SXE_JITSON_TOKEN_SIZE) {
            memcpy(&stack->jitsons[index], string, len + 1);
            return true;
        }

        memcpy(&stack->jitsons[index], string, SXE_JITSON_TOKEN_SIZE);
        string += SXE_JITSON_TOKEN_SIZE;
    }

    return true;
}

/**
 * Add a member name to the object being constructed on the stack
 *
 * @param stack The jitson stack
 * @param name  The member name
 * @param type  SXE_JITSON_TYPE_IS_COPY, SXE_JITSON_TYPE_IS_REF, or SXE_JITSON_TYPE_IS_OWN
 *
 * @return true on success, false on out of memory (ENOMEM) or copied string too long (ENAMETOOLONG)
 */
bool
sxe_jitson_stack_add_member_name(struct sxe_jitson_stack *stack, const char *name, uint32_t type)
{
    unsigned object;

    SXEA1(stack->open,                                    "Can't add a member name when there is no object under construction");
    SXEA1(stack->jitsons[object = stack->open - 1].type == SXE_JITSON_TYPE_OBJECT, "Member names can only be added to objects");
    SXEA1(!stack->jitsons[object].partial.no_value,                                "Member name already added without a value");
    SXEA1(!(type & ~(SXE_JITSON_TYPE_IS_REF | SXE_JITSON_TYPE_IS_OWN)),            "Unexected type flags 0x%x", (unsigned)type);

    stack->jitsons[object].partial.no_value = 1;    // (uint8_t)true
    return sxe_jitson_stack_push_string(stack, name, type | SXE_JITSON_TYPE_IS_KEY);
}

/**
 * Add a string to the object or array being constructed on the stack
 *
 * @param stack The jitson stack
 * @param name  The member name
 * @param type  SXE_JITSON_TYPE_IS_COPY, SXE_JITSON_TYPE_IS_REF, or SXE_JITSON_TYPE_IS_OWN
 *
 * @return true on success, false on out of memory (ENOMEM) or copied string too long (ENAMETOOLONG)
 */
bool
sxe_jitson_stack_add_string(struct sxe_jitson_stack *stack, const char *name, uint32_t type)
{
    unsigned collection = stack->open - 1;
    bool     ret;

    SXEA1(stack->open, "Can't add a value when there is no array or object under construction");
    SXEA1(stack->jitsons[collection].type == SXE_JITSON_TYPE_ARRAY || stack->jitsons[collection].partial.no_value,
          "Member name must be added added to an object before adding a string value");
    SXEA1(stack->jitsons[collection].type == SXE_JITSON_TYPE_OBJECT || stack->jitsons[collection].type == SXE_JITSON_TYPE_ARRAY,
          "Strings can only be added to arrays or objects");
    SXEA1(!(type & ~(SXE_JITSON_TYPE_IS_REF | SXE_JITSON_TYPE_IS_OWN)), "Unexected type flags 0x%x", (unsigned)type);
    stack->jitsons[collection].partial.no_value = 0;    // (uint8_t)false

    if ((ret = sxe_jitson_stack_push_string(stack, name, type)))
        stack->jitsons[collection].len++;

    return ret;
}

/**
 * Add a null value to the array or object being constructed on the stack
 *
 * @param stack The jitson stack
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_null(struct sxe_jitson_stack *stack)
{
    unsigned index;

    if ((index = sxe_jitson_stack_add_value(stack, 1)) == SXE_JITSON_STACK_ERROR)
        return false;

    sxe_jitson_make_null(&stack->jitsons[index]);
    return true;
}

/**
 * Add a boolean value to the array or object being constructed on the stack
 *
 * @param stack   The jitson stack
 * @param boolean The boolean value
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_bool(struct sxe_jitson_stack *stack, bool boolean)
{
    unsigned index;

    if ((index = sxe_jitson_stack_add_value(stack, 1)) == SXE_JITSON_STACK_ERROR)
        return false;

    sxe_jitson_make_bool(&stack->jitsons[index], boolean);
    return true;
}

/**
 * Add a number to the array or object being constructed on the stack
 *
 * @param stack  The jitson stack
 * @param number The numeric value
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_number(struct sxe_jitson_stack *stack, double number)
{
    unsigned index;

    if ((index = sxe_jitson_stack_add_value(stack, 1)) == SXE_JITSON_STACK_ERROR)
        return false;

    sxe_jitson_make_number(&stack->jitsons[index], number);
    return true;
}

/**
 * Add an unsigned integer to the array or object being constructed on the stack
 *
 * @param stack The jitson stack
 * @param uint  The unsigned integer value
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_uint(struct sxe_jitson_stack *stack, uint64_t uint)
{
    unsigned index;

    if ((index = sxe_jitson_stack_add_value(stack, 1)) == SXE_JITSON_STACK_ERROR)
        return false;

    sxe_jitson_make_uint(&stack->jitsons[index], uint);
    return true;
}

/**
 * Add a reference value to the array or object being constructed on the stack
 *
 * @param stack The jitson stack
 * @param to    The jitson to add a reference to
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_reference(struct sxe_jitson_stack *stack, const struct sxe_jitson *to)
{
    unsigned index;

    if ((index = sxe_jitson_stack_add_value(stack, 1)) == SXE_JITSON_STACK_ERROR)
        return false;

    sxe_jitson_make_reference(&stack->jitsons[index], to);
    return true;
}

/**
 * Add a duplicate of a value to the array or object being constructed on the stack
 *
 * @param stack The jitson stack
 * @param to    The jitson to add a duplicate of
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_dup(struct sxe_jitson_stack *stack, const struct sxe_jitson *value)
{
    unsigned index;
    unsigned size = sxe_jitson_size(value);

    if ((index = sxe_jitson_stack_add_value(stack, size)) == SXE_JITSON_STACK_ERROR)
        return false;

    return sxe_jitson_stack_dup_at_index(stack, index, value, size);
}

/**
 * Add duplicates of all members of an object to the object being constructed on the stack
 *
 * @param stack  The jitson stack
 * @param jitson The object whose members are to be duplicated
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_dup_members(struct sxe_jitson_stack *stack, const struct sxe_jitson *jitson)
{
    size_t   size;
    unsigned len, object = stack->open - 1;
    uint32_t index;

    SXEA1(stack->open,                                           "Can't add members when no object is under construction");
    SXEA1(stack->jitsons[object].type == SXE_JITSON_TYPE_OBJECT, "Members can only be added to an object");
    SXEA1(!stack->jitsons[object].partial.no_value,              "Member name already added without a value");

    jitson = sxe_jitson_is_reference(jitson) ? jitson->jitref : jitson;
    SXEA1((jitson->type & SXE_JITSON_TYPE_MASK) == SXE_JITSON_TYPE_OBJECT, "Can't add members from JSON type %s",
                                                                           sxe_jitson_get_type_as_str(jitson));

    if ((len = jitson->len) == 0)
        return true;

    size = sxe_jitson_size(jitson) - 1;    // Don't include the object itself

    if ((index = sxe_jitson_stack_expand(stack, size)) == SXE_JITSON_STACK_ERROR)
        return false;

    memcpy(&stack->jitsons[index], jitson + 1, size * sizeof(*jitson));

    if (!sxe_jitson_object_clone_members(jitson, &stack->jitsons[index - 1], len))
        return false;

    stack->jitsons[object].len += len;
    return true;
}

/**
 * Finish construction of an object or array on a stack
 *
 * @note Aborts if the object is not a collection under construction, has an incomplete nested object, or is an object and has a
 *       member name without a matching value.
 *
 * @return true. This is returned to allow further calls (e.g. sxe_jitson_stack_get_jitson) to be chained with &&.
 */
bool
sxe_jitson_stack_close_collection(struct sxe_jitson_stack *stack)
{
    unsigned index;

    SXEA1(stack->open, "There must be an open collection on the stack");
    index = stack->open - 1;
    SXEA1(!stack->jitsons[index].partial.no_value, "Index %u is an object with a member name with no value", index);
    SXEA1(!stack->jitsons[index].partial.nested,   "Index %u is a collection with a nested open collection", index);

    stack->open                   = stack->jitsons[index].partial.collection;
    stack->jitsons[index].integer = stack->count - index;    // Store the offset past the object or array
    return true;
}
