/* Test the sxe-jitson operator extension
 */

#include <tap.h>

#include "sxe-jitson-oper.h"
#include "sxe-log.h"

static unsigned len_op = ~0U;

static struct sxe_jitson jitson_true = {
    .type    = SXE_JITSON_TYPE_BOOL,
    .boolean = true
};

static struct sxe_jitson jitson_false = {
    .type    = SXE_JITSON_TYPE_BOOL,
    .boolean = false
};

static struct sxe_jitson jitson_null = {
    .type = SXE_JITSON_TYPE_NULL
};

static const struct sxe_jitson *
and_op_default(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    if (!sxe_jitson_test(left))
        return &jitson_false;

    return sxe_jitson_test(right) ? &jitson_true : &jitson_false;
}

static const struct sxe_jitson *
len_op_default(const struct sxe_jitson *arg)
{
    if (!sxe_jitson_supports_len(arg)) {
        SXEL2(": Type %s doesn't support operator '%s'", sxe_jitson_get_type_as_str(arg), sxe_jitson_oper_get_name(len_op));
        errno = EOPNOTSUPP;
        return NULL;
    }

    return sxe_jitson_create_uint(sxe_jitson_len(arg));
}

static const struct sxe_jitson *
in_op_string(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    if (sxe_jitson_get_type(left) != SXE_JITSON_TYPE_STRING) {
        SXEL2(": Can't look for a %s in a string", sxe_jitson_get_type_as_str(left));
        errno = EOPNOTSUPP;
        return NULL;
    }

    char *found = strstr(sxe_jitson_get_string(right, NULL), sxe_jitson_get_string(left, NULL));

    if (!found)
        return &jitson_null;

    return sxe_jitson_create_string_ref(found);
}

/* Wonky override to take the length of a number.
 */
static const struct sxe_jitson *
len_op_number(const struct sxe_jitson *arg)
{
    char buf[16];

    return sxe_jitson_create_number(snprintf(buf, sizeof(buf), "%g", sxe_jitson_get_number(arg)));
}

static const char *string = "this string is 33 characters long";

int
main(void)
{
    struct sxe_jitson          arg, element;
    union sxe_jitson_oper_func func;
    const struct sxe_jitson   *result;
    const char                *substring;
    uint64_t                   start_allocations;
    unsigned                   in_op, and_op;

    plan_tests(18);
    start_allocations     = sxe_allocations;
    sxe_alloc_diagnostics = true;
    sxe_jitson_type_init(8, 0);

    func.binary = and_op_default;
    is(and_op   = sxe_jitson_oper_register("&&", SXE_JITSON_OPER_BINARY, func), 1, "First operator is 1");
    is(sxe_jitson_oper_apply_binary(&jitson_true, and_op, &jitson_true),  &jitson_true,  "true && true is true");
    is(sxe_jitson_oper_apply_binary(&jitson_true, and_op, &jitson_false), &jitson_false, "true && false is false");

    sxe_jitson_make_string_ref(&arg, string);
    func.unary = len_op_default;
    is(len_op  = sxe_jitson_oper_register("len", SXE_JITSON_OPER_UNARY, func), 2, "Second operator is 2");
    is(sxe_jitson_oper_apply_unary(len_op, &jitson_true), NULL,                   "len true is invalid");
    ok(result = sxe_jitson_oper_apply_unary(len_op, &arg),                        "len string is implemented");
    is(sxe_jitson_get_number(result), 33,                                         "len string is 33");
    sxe_jitson_free(result);

    sxe_jitson_make_string_ref(&element, "33");
    func.binary = NULL;    // No default
    is(in_op  = sxe_jitson_oper_register("in", SXE_JITSON_OPER_BINARY | SXE_JITSON_OPER_TYPE_RIGHT, func), 3,
       "Third operator is 3");
    func.binary = in_op_string;
    sxe_jitson_oper_add_to_type(in_op, SXE_JITSON_TYPE_STRING, func);
    ok(result = sxe_jitson_oper_apply_binary(&element, in_op, &arg), "substring in string is implemented");
    is_strncmp(substring = sxe_jitson_get_string(result, NULL), "33", 2,   "Returned string begins with the substring");
    is(substring, string + strlen("this string is "),                      "And points into the containing string");
    sxe_jitson_free(result);

    sxe_jitson_make_number(&arg, 33);
    ok(!sxe_jitson_oper_apply_binary(&element, in_op, &arg), "Substring in a number is not implemented");
    is(errno, EOPNOTSUPP,                                    "Got the expected error (%s)", strerror(errno));

    sxe_jitson_make_number(&arg, 666);
    func.unary = len_op_number;
    sxe_jitson_oper_add_to_type(len_op, SXE_JITSON_TYPE_NUMBER, func);
    ok(result = sxe_jitson_oper_apply_unary(len_op, &arg), "length of number is now implemented");
    is(sxe_jitson_get_uint(result), 3,                     "Returned length of 666 is 3");
    sxe_jitson_free(result);

    func.unary = NULL;    // No default
    is(sxe_jitson_oper_register("~", SXE_JITSON_OPER_UNARY, func), 4, "Fourth operator is 4");
    ok(!sxe_jitson_oper_apply_unary(4, &arg),                         "~ of number is not implemented");

    sxe_jitson_oper_fini();
    sxe_jitson_type_fini();
    is(sxe_allocations, start_allocations, "No memory was leaked");
    return exit_status();
}
