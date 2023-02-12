/* Test the sxe-jitson identifier extension
 */

#include <tap.h>

#include "mockfail.h"
#include "sxe-jitson-ident.h"
#include "sxe-thread.h"

int
main(void)
{
    struct sxe_jitson_source source;
    struct sxe_jitson_stack  *stack;
    struct sxe_jitson        *jitson;     // Constructed jitson values are returned as non-const
    const struct sxe_jitson  *element;    // Accessed jitson values are returned as const
    size_t                    len;
    uint64_t                  start_allocations;

    plan_tests(20);
    start_allocations     = sxe_allocations;
    sxe_alloc_diagnostics = true;

    ok(!sxe_jitson_is_init(), "Not yet initialized");
    sxe_jitson_type_init(0, 0);    // Initialize the JSON types, and don't enable hexadecimal
    sxe_jitson_ident_register();
    stack = sxe_jitson_stack_get_thread();

    ok(jitson = sxe_jitson_new("[NONE,length_8,identifier]"), "Parsed an array containing unknown identifiers");
    ok(element = sxe_jitson_array_get_element(jitson, 0),     "Got the first element");
    is(SXE_JITSON_TYPE_IDENT, sxe_jitson_get_type(element),   "It's an identifier");
    is_eq(sxe_jitson_ident_get_name(element, &len), "NONE",   "It's 'NONE'");
    is(len, 4,                                                "It's length is 4");
    sxe_jitson_free(jitson);

    sxe_jitson_source_from_string(&source, "[NONE,length_8,identifier]", SXE_JITSON_FLAG_STRICT);
    is(sxe_jitson_stack_load_json(stack, &source), NULL, "Failed to parse identifiers due to strict mode");

    ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT), "Opened an object on the stack");
    ok(sxe_jitson_stack_add_member_uint(stack, "NONE", 0),              "Added value for NONE to the object");
    sxe_jitson_stack_close_collection(stack);
    ok(jitson = sxe_jitson_stack_get_jitson(stack), "Got the object from the stack");
    sxe_jitson_stack_init(jitson);    // Set the constants

    /* Test the failure case where an identifier is > 7 characters and jitson allocation fails.
     */
    stack->maximum = 1;    // Reset the thread stack's initial stack size to 1
    MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND);
    MOCKFAIL_SET_FREQ(3);
    is(sxe_jitson_new("[NONE,BIT0,identifier]"), NULL, "Failed to parse constants on alloc failure");
    MOCKFAIL_END_TESTS();

    ok(jitson = sxe_jitson_new("[NONE,BIT0,identifier]"),         "Parsed array of a constant and an unknown identifier");
    ok(element = sxe_jitson_array_get_element(jitson, 0),         "Got the first element");
    is(sxe_jitson_get_type(element), SXE_JITSON_TYPE_NUMBER,      "It's a number");
    is(sxe_jitson_get_uint(element), 0,                           "It's 0");
    ok(element = sxe_jitson_array_get_element(jitson, 2),         "Got the third element");
    is(sxe_jitson_get_type(element), SXE_JITSON_TYPE_IDENT,       "It's an %s (expect identifier)",
       sxe_jitson_get_type_as_str(element));
    is_eq(sxe_jitson_ident_get_name(element, &len), "identifier", "It's 'identifier'");
    is(len, 10,                                                   "Its length is 10");
    sxe_jitson_free(jitson);

    sxe_jitson_stack_fini();
    sxe_jitson_type_fini();
    sxe_thread_memory_free(SXE_THREAD_MEMORY_ALL);
    is(sxe_allocations, start_allocations, "No memory was leaked");

    return exit_status();
}
