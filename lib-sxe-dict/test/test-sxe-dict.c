#include <inttypes.h>
#include <tap.h>

#include "sxe-alloc.h"
#include "sxe-dict.h"

int
main(void) {
    struct sxe_dict *dic;
    const void      **value_ptr;
    const void       *value;
    uint64_t          start_allocations;

    plan_tests(13);
    start_allocations     = sxe_allocations;
    sxe_alloc_diagnostics = true;

    dic = sxe_dict_new(0);
    is(dic->table, NULL,                                "Empty dictionary has no table");
    ok((value = sxe_dict_find(dic, "ABC",  3)) == NULL, "Before adding ABC, expected NULL, found: %"PRIiPTR"\n", (intptr_t)value);

    value_ptr = sxe_dict_add(dic, "ABC", 3);
    is(*value_ptr, NULL, "New entry should not have a value");
    *value_ptr = (const void *)100;
    is(dic->size, 1, "Size after 1 insert is 1");

    value_ptr = sxe_dict_add(dic, "DE", 2);
    *value_ptr = (const void *)200;
    is(dic->size, 2, "Size after 2 inserts is 2");

    value_ptr = sxe_dict_add(dic, "HJKL", 4);
    *value_ptr = (const void *)300;

    /* The following is because after doubling to 2, 1 and 2 ended up in bucket 0, but 3 ends up in bucket 1.
     */
    is(dic->size, 2, "Size after 3 inserts is 2");

    ok((value = sxe_dict_find(dic, "ABC",  3)), "ABC found");
    is(value, 100,                              "It's value is 100");
    ok((value = sxe_dict_find(dic, "DE",   2)), "DE found");
    is(value, 200,                              "It's value is 200");
    ok((value = sxe_dict_find(dic, "HJKL", 4)), "HJKL found");
    is(value, 300,                              "It's value is 300");

    sxe_dict_delete(dic);

    is(sxe_allocations, start_allocations, "No memory was leaked");
    return exit_status();
}
