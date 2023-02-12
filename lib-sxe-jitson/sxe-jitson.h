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

#ifndef SXE_JITSON_H
#define SXE_JITSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sxe-alloc.h"
#include "sxe-factory.h"

#define SXE_JITSON_FLAG_STRICT       0             // Disable all extensions. This is only valid for a sxe_jitson_source.
#define SXE_JITSON_FLAG_ALLOW_HEX    0x00000001    // Allow hexadecimal when parsing numbers, which isn't strictly valid JSON
#define SXE_JITSON_FLAG_ALLOW_CONSTS 0x00000002    // Replace parsed constants (default if sxe_jitson_type_init called)
#define SXE_JITSON_FLAG_ALLOW_IDENTS 0x00000004    // Return parsed identifiers (default if sxe_jitson_ident_register called)

#define SXE_JITSON_MIN_TYPES 8    // The minimum number of types for JSON

#define SXE_JITSON_TYPE_INVALID   0
#define SXE_JITSON_TYPE_NULL      1
#define SXE_JITSON_TYPE_BOOL      2
#define SXE_JITSON_TYPE_NUMBER    3
#define SXE_JITSON_TYPE_STRING    4
#define SXE_JITSON_TYPE_ARRAY     5
#define SXE_JITSON_TYPE_OBJECT    6
#define SXE_JITSON_TYPE_REFERENCE 7    // A reference acts like the type the reference points to.

#define SXE_JITSON_TYPE_MASK    0xFFFF        // Bits included in the type enumeration
#define SXE_JITSON_TYPE_IS_UINT 0x08000000    // Flag set for numbers that are unsigned integers
#define SXE_JITSON_TYPE_IS_KEY  0x10000000    // Flag set for types (in JSON, strings) when they are used as keys in an object
#define SXE_JITSON_TYPE_IS_COPY 0             // Flag passed to API to indicate that strings/member names are to be copied
#define SXE_JITSON_TYPE_IS_REF  0x20000000    // Flag set for strings that are references (size == 0 until cached or if empty)
#define SXE_JITSON_TYPE_IS_OWN  0x40000000    // Flag set for values where the reference is owned by the object (to be freed)
#define SXE_JITSON_TYPE_INDEXED 0x40000000    // Flag set for arrays and object if they have been indexed
#define SXE_JITSON_TYPE_ALLOCED 0x80000000    // Flag set for the first jitson token in an allocated jitson

#define SXE_JITSON_STACK_ERROR      (~0U)
#define SXE_JITSON_TOKEN_SIZE       sizeof(struct sxe_jitson)
#define SXE_JITSON_STRING_SIZE      sizeof(((struct sxe_jitson *)0)->string)

/* A jitson token. Copied strings of > 7 bytes length continue into the next token. Collections (arrays and objects) may
 * initially store the size in jitsons of the entire collection in integer. If so, the size is atomically replaced by the index
 * om first access.
 */
struct sxe_jitson {
    union {
        uint32_t type;             // See definitions above
        uint16_t shortype;         // The bottom 16 bits just for readable gdbing
    };
    union {
        uint32_t len;              // Length of string (if <= 4294967295) or number of elements/members in array/object
        uint32_t link;             // In an indexed object member name, this is the offset of the next member name in the bucket
    };
    union {
        uint32_t                *index;        // Points to offsets (len of them) to elements/members, or 0 for empty buckets
        uint64_t                 integer;      // JSON unsigned integer or size of array or object before indexing
        double                   number;       // JSON number, stored as a double wide floating point number
        bool                     boolean;      // True and false
        char                     string[8];    // First 8 bytes of a string, including NUL.
        const void              *reference;    // Points to a constant external value
        const struct sxe_jitson *jitref;       // Reference to another sxe_jitson. Type must be SXE_JITSON_TYPE_REFERENCE
        struct {
            uint8_t  no_value;      // Object under construction has a member name with no value
            uint8_t  nested;        // Object or array under construction contains another array/object under construction
            uint32_t collection;    // Object or array is contained in the object or array at this index - 1 or 0 if its the root
        } partial;
    };
};

struct sxe_jitson_source {
    const char *json;     // Pointer into a buffer containing JSON to be parsed
    const char *next;     // Pointer to the next character to be parsed
    const char *end;      // Pointer after the last character to be parsed (max pointer value if the buffer is NUL terminated).
    uint32_t    flags;    // Specific JSON extensions allowed while parsing this source
};

struct sxe_jitson_stack {
    unsigned           maximum;
    unsigned           count;
    struct sxe_jitson *jitsons;
    unsigned           open;       // Index + 1 of the deepest open collection that's under construction or 0 if none
};

extern uint32_t sxe_jitson_flags;    // Default JSON extensions allowed

#include "sxe-jitson-proto.h"
#include "sxe-jitson-source-proto.h"
#include "sxe-jitson-stack-proto.h"
#include "sxe-jitson-type-proto.h"

/* Inline functions to create an easier to use interface.
 */

static inline const char *
sxe_jitson_get_type_as_str(const struct sxe_jitson *jitson)
{
    return sxe_jitson_type_to_str(sxe_jitson_get_type(jitson));
}

static inline bool
sxe_jitson_stack_add_member_string(struct sxe_jitson_stack *stack, const char *name, const char * value, uint32_t type)
{
    return sxe_jitson_stack_add_member_name(stack, name,  SXE_JITSON_TYPE_IS_COPY)
        && sxe_jitson_stack_add_string(     stack, value, type);
}

static inline bool
sxe_jitson_stack_add_member_null(struct sxe_jitson_stack *stack, const char *name)
{
    return sxe_jitson_stack_add_member_name(stack, name, SXE_JITSON_TYPE_IS_COPY) && sxe_jitson_stack_add_null(stack);
}

static inline bool
sxe_jitson_stack_add_member_bool(struct sxe_jitson_stack *stack, const char *name, bool boolean)
{
    return sxe_jitson_stack_add_member_name(stack, name, SXE_JITSON_TYPE_IS_COPY) && sxe_jitson_stack_add_bool(stack, boolean);
}

static inline bool
sxe_jitson_stack_add_member_number(struct sxe_jitson_stack *stack, const char *name, double number)
{
    return sxe_jitson_stack_add_member_name(stack, name, SXE_JITSON_TYPE_IS_COPY) && sxe_jitson_stack_add_number(stack, number);
}

static inline bool
sxe_jitson_stack_add_member_uint(struct sxe_jitson_stack *stack, const char *name, uint64_t uint)
{
    return sxe_jitson_stack_add_member_name(stack, name, SXE_JITSON_TYPE_IS_COPY) && sxe_jitson_stack_add_uint(stack, uint);
}

static inline bool
sxe_jitson_stack_add_member_reference(struct sxe_jitson_stack *stack, const char *name, const struct sxe_jitson *to)
{
    return sxe_jitson_stack_add_member_name(stack, name, SXE_JITSON_TYPE_IS_COPY) && sxe_jitson_stack_add_reference(stack, to);
}

static inline bool
sxe_jitson_stack_add_member_dup(struct sxe_jitson_stack *stack, const char *name, const struct sxe_jitson *value)
{
     return sxe_jitson_stack_add_member_name(stack, name, SXE_JITSON_TYPE_IS_COPY) && sxe_jitson_stack_add_dup(stack, value);
}

static inline struct sxe_jitson *
sxe_jitson_create_null(void)
{
    struct sxe_jitson *jitson;

    if (!(jitson = sxe_malloc(sizeof(struct sxe_jitson))))
        return NULL;

    sxe_jitson_make_null(jitson);
    jitson->type |= SXE_JITSON_TYPE_ALLOCED;
    return jitson;
}

static inline struct sxe_jitson *
sxe_jitson_create_bool(bool boolean)
{
    struct sxe_jitson *jitson;

    if (!(jitson = sxe_malloc(sizeof(struct sxe_jitson))))
        return NULL;

    sxe_jitson_make_bool(jitson, boolean);
    jitson->type |= SXE_JITSON_TYPE_ALLOCED;
    return jitson;
}

static inline struct sxe_jitson *
sxe_jitson_create_number(double number)
{
    struct sxe_jitson *jitson;

    if (!(jitson = sxe_malloc(sizeof(struct sxe_jitson))))
        return NULL;

    sxe_jitson_make_number(jitson, number);
    jitson->type |= SXE_JITSON_TYPE_ALLOCED;
    return jitson;
}

static inline struct sxe_jitson *
sxe_jitson_create_uint(uint64_t integer)
{
    struct sxe_jitson *jitson;

    if (!(jitson = sxe_malloc(sizeof(struct sxe_jitson))))
        return NULL;

    sxe_jitson_make_uint(jitson, integer);
    jitson->type |= SXE_JITSON_TYPE_ALLOCED;
    return jitson;
}

/**
 * Create a jitson string value that references an immutable C string
 */
static inline struct sxe_jitson *
sxe_jitson_create_string_ref(const char *string)
{
    struct sxe_jitson *jitson;

    if (!(jitson = sxe_malloc(sizeof(struct sxe_jitson))))
        return NULL;

    sxe_jitson_make_string_ref(jitson, string);
    jitson->type |= SXE_JITSON_TYPE_ALLOCED;
    return jitson;
}

/**
 * Create a jitson string value with an owned reference to a duplication of a C string
 */
static inline struct sxe_jitson *
sxe_jitson_create_string_dup(const char *string)
{
    struct sxe_jitson *jitson;

    if (!(jitson = sxe_malloc(sizeof(struct sxe_jitson))))
        return NULL;

    if (!(string = sxe_strdup(string))) {
        sxe_free(jitson);
        return NULL;
    }

    sxe_jitson_make_string_ref(jitson, string);
    jitson->type |= SXE_JITSON_TYPE_IS_OWN | SXE_JITSON_TYPE_ALLOCED;
    return jitson;
}

/**
 * Create a reference to another jitson that will behave exactly like the original jitson
 *
 * @param jitson Pointer to jitson to create
 * @param to     jitson to refer to
 *
 * @note References are only valid during the lifetime of the jitson they refer to
 */
static inline struct sxe_jitson *
sxe_jitson_create_reference(const struct sxe_jitson *to)
{
    struct sxe_jitson *jitson;

    if (!(jitson = sxe_malloc(sizeof(struct sxe_jitson))))
        return NULL;

    sxe_jitson_make_reference(jitson, to);
    jitson->type |= SXE_JITSON_TYPE_ALLOCED;
    return jitson;
}

static inline uint32_t
sxe_jitson_source_get_flags(struct sxe_jitson_source *source)
{
    return source->flags;
}

static inline size_t
sxe_jitson_source_get_consumed(struct sxe_jitson_source *source)
{
    return source->next - source->json;
}

#define MOCK_FAIL_STACK_NEW_OBJECT       ((char *)sxe_jitson_new + 0)
#define MOCK_FAIL_STACK_NEW_JITSONS      ((char *)sxe_jitson_new + 1)
#define MOCK_FAIL_STACK_EXPAND_AFTER_GET ((char *)sxe_jitson_new + 2)
#define MOCK_FAIL_STACK_DUP              ((char *)sxe_jitson_new + 3)
#define MOCK_FAIL_STACK_EXPAND           ((char *)sxe_jitson_new + 4)
#define MOCK_FAIL_OBJECT_GET_MEMBER      ((char *)sxe_jitson_new + 5)
#define MOCK_FAIL_ARRAY_GET_ELEMENT      ((char *)sxe_jitson_new + 6)
#define MOCK_FAIL_DUP                    ((char *)sxe_jitson_dup + 0)
#define MOCK_FAIL_OBJECT_CLONE           ((char *)sxe_jitson_dup + 1)
#define MOCK_FAIL_ARRAY_CLONE            ((char *)sxe_jitson_dup + 2)
#define MOCK_FAIL_STRING_CLONE           ((char *)sxe_jitson_dup + 3)

#endif
