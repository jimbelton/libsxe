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

#include "sxe-alloc.h"
#include "sxe-jitson-oper.h"
#include "sxe-log.h"

/*
 * This module implements operations on sxe-jitson values.  All operations take and return constant values.
 *
 * Memory allocation philosophy: All values returned must either be allocated by the operation or be non-allocated values. This
 * allows any value returned by an operation to be freed. If an allocated value that was not allocated by the operation (e.g.
 * one of the arguments) needs to be returned, a reference to the value should be created and returned. This will prevent
 * accidental deallocation of the value.
 */

struct sxe_jitson_oper {
    const char                *name;
    unsigned                   flags;
    union sxe_jitson_oper_func def_func;
};

struct sxe_jitson_oper_per_type {
    unsigned                    num_opers;    // Number of additonal operations supported by this type
    union sxe_jitson_oper_func *opers;        // Pointers to operation implementations
};

static unsigned                         num_opers  = 0;
static struct sxe_jitson_oper          *opers      = NULL;      // Note that operation 0 is invalid
static unsigned                         num_types  = 0;         // Number of types with operations registered
static struct sxe_jitson_oper_per_type *type_opers = NULL;      // Array of oper_per_type, 1:1 with sxe-jitson-types
static union sxe_jitson_oper_func       null_func  = {NULL};

/**
 * Register an operation on sxe_jitson values
 *
 * @param name    Name of the operation (which may be a symbol, like '==')
 * @param flags   SXE_JITSON_OPER_UNARY if the operation is takes 1 argument (i.e. is unary)
 *                SXE_JITSON_OPER_BINARY if the operation takes 2 arguments and uses the function from the type of the left arg
 *                SXE_JITSON_OPER_BINARY|SXE_JITSON_OPER_TYPE_RIGHT if binary and uses function from the type of the right arg
 * @param default The default implementation of the operation, or NULL if there is no default
 *
 * @return An unsigned identifier for the operation
 */
unsigned
sxe_jitson_oper_register(const char *name, unsigned flags, union sxe_jitson_oper_func def_func)
{
    unsigned i;

    if (num_opers == 0) {
        SXEA1(opers = sxe_calloc(2, sizeof(*opers)), "Failed to allocate first operation");
        num_opers = 1;
    } else {
        for (i = 1; i <= num_opers; i++)
            SXEA1(strcmp(opers[i].name, name) != 0, "Operation '%s' is already registered", name);

        SXEA1(opers = sxe_realloc(opers, (++num_opers + 1) * sizeof(*opers)), "Failed to reallocate operations");
    }

    opers[num_opers].name     = name;
    opers[num_opers].flags    = flags;
    opers[num_opers].def_func = def_func;
    return num_opers;
}

void
sxe_jitson_oper_add_to_type(unsigned op, unsigned type, union sxe_jitson_oper_func func)
{
    unsigned i;

    SXEA1(op <= num_opers, "Operator %u is invalid with only %u operators registered", op, num_opers);

    if (type >= num_types) {
        SXEA1(type_opers = sxe_realloc(type_opers, (type + 1) * sizeof(*type_opers)),
              "Failed to reallocate operations per type");

        for (i = num_types; i <= type; i++) {    // All new types need to be initialized
            type_opers[i].num_opers = 0;
            type_opers[i].opers     = NULL;
        }

        num_types = type + 1;
    }

    if (op >= type_opers[type].num_opers) {
        SXEA1(type_opers[type].opers = sxe_realloc(type_opers[type].opers, (op + 1) * sizeof(*type_opers[type].opers)),
              "Failed to reallocate operations for type %s", sxe_jitson_type_to_str(type));

        for (i = type_opers[type].num_opers; i < op; i++)    // Functions for intervening operations need to be initialized
            type_opers[type].opers[i] = null_func;

        type_opers[type].num_opers = op + 1;
    }

    type_opers[type].opers[op] = func;
}

const char *
sxe_jitson_oper_get_name(unsigned op)
{
    SXEA1(op <= num_opers, "Operator %u is invalid with only %u operators registered", op, num_opers);
    return opers[op].name;
}

const struct sxe_jitson *
sxe_jitson_oper_apply_unary(unsigned op, const struct sxe_jitson *arg)
{
    SXEA1(op <= num_opers, "Operator %u is invalid with only %u operators registered", op, num_opers);
    SXEA1(!(opers[op].flags & SXE_JITSON_OPER_BINARY), "Operator '%s' is binary", opers[op].name);

    unsigned type = sxe_jitson_get_type(arg);

    if (type < num_types && op < type_opers[type].num_opers && type_opers[type].opers[op].unary)
        return type_opers[type].opers[op].unary(arg);

    if (!opers[op].def_func.unary) {
        SXEL2(": No default function for operator '%s'", opers[op].name);
        errno = EOPNOTSUPP;
        return NULL;
    }

    return opers[op].def_func.unary(arg);
}

const struct sxe_jitson *
sxe_jitson_oper_apply_binary(const struct sxe_jitson *left, unsigned op, const struct sxe_jitson *right)
{
    SXEA1(op <= num_opers, "Operator %u is invalid with only %u operators registered", op, num_opers);
    SXEA1(opers[op].flags & SXE_JITSON_OPER_BINARY, "Operator '%s' is unary", opers[op].name);

    unsigned type = opers[op].flags & SXE_JITSON_OPER_TYPE_RIGHT ? sxe_jitson_get_type(right) : sxe_jitson_get_type(left);

    if (type < num_types && op < type_opers[type].num_opers && type_opers[type].opers[op].binary)
        return type_opers[type].opers[op].binary(left, right);

    if (!opers[op].def_func.binary) {
        SXEL2(": No default function for operator '%s'", opers[op].name);
        errno = EOPNOTSUPP;
        return NULL;
    }

    return opers[op].def_func.binary(left, right);
}

void
sxe_jitson_oper_fini(void)
{
    unsigned i;

    for (i = 0; i < num_types; i++)
        sxe_free(type_opers[i].opers);

    sxe_free(type_opers);
    sxe_free(opers);
    opers     = NULL;
    num_opers = 0;
}
