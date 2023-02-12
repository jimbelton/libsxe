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

#ifndef SXE_JITSON_OPER_H
#define SXE_JITSON_OPER_H

#include "sxe-jitson.h"

#define SXE_JITSON_OPER_UNARY      0
#define SXE_JITSON_OPER_BINARY     1    // Set this bit in flags if the operator is binary
#define SXE_JITSON_OPER_TYPE_RIGHT 2    // Set this bit in flags if the operator should apply the type of the right argument

union sxe_jitson_oper_func {
    const struct sxe_jitson *(*unary)( const struct sxe_jitson *);
    const struct sxe_jitson *(*binary)(const struct sxe_jitson *, const struct sxe_jitson *);
};

#include "sxe-jitson-oper-proto.h"

#endif
