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

#include "sxe-jitson-ident.h"

uint32_t SXE_JITSON_TYPE_IDENT = SXE_JITSON_TYPE_INVALID;    // Need to register the type to get a valid value

/**
 * Called back from sxe-jitson-stack when an unrecognized identifier has been found
 */
static bool
sxe_jitson_stack_push_ident_at_index(struct sxe_jitson_stack *stack, uint32_t index, const char *ident, size_t len)
{
    if (len >= SXE_JITSON_STRING_SIZE)    // More than one token is required
        if (sxe_jitson_stack_expand(stack, (len + SXE_JITSON_STRING_SIZE) / SXE_JITSON_TOKEN_SIZE) == SXE_JITSON_STACK_ERROR)
            return false;

    stack->jitsons[index].type = SXE_JITSON_TYPE_IDENT;
    stack->jitsons[index].len  = len;
    memcpy(&stack->jitsons[index].string, ident, len);
    stack->jitsons[index].string[len] = '\0';
    return true;
}

//static bool
//sxe_jitson_ident_test(const struct sxe_jitson *jitson)
//{
//    return types[sxe_jitson_get_type(jitson->jitref)].test(jitson->jitref);
//}

/* Identifiers are stored like strings: Up to 8 chars in the first jitson, then up to 16 in each subsequent jitson.
 */
static uint32_t
sxe_jitson_ident_size(const struct sxe_jitson *jitson)
{
    return 1 + (jitson->len + SXE_JITSON_STRING_SIZE) / SXE_JITSON_TOKEN_SIZE;
}

//static size_t
//sxe_jitson_ident_len(const struct sxe_jitson *jitson)
//{
//    return types[sxe_jitson_get_type(jitson->jitref)].len(jitson->jitref);
//}

//char *
//sxe_jitson_ident_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
//{
//    return types[sxe_jitson_get_type(jitson->jitref)].build_json(jitson->jitref, factory);
//}

/* Private hook into the jitson parser
 */
extern bool (*sxe_jitson_stack_push_ident)(struct sxe_jitson_stack *stack, uint32_t index, const char *ident, size_t len);

/* Call at initialization after sxe_jitson_type_init to register the identifier type
 */
uint32_t
sxe_jitson_ident_register(void)
{
    SXE_JITSON_TYPE_IDENT = sxe_jitson_type_register("identifier", sxe_jitson_free_base, NULL, sxe_jitson_ident_size, NULL,
                                                     NULL, NULL);
    sxe_jitson_stack_push_ident = sxe_jitson_stack_push_ident_at_index;
    sxe_jitson_flags           |= SXE_JITSON_FLAG_ALLOW_IDENTS;
    return SXE_JITSON_TYPE_IDENT;
}

const char *
sxe_jitson_ident_get_name(const struct sxe_jitson *ident, size_t *len_out)
{
    if (len_out)
        *len_out = ident->len;

    return ident->string;
}
