# sxe-dict

This module is based on hashdict.c, an MIT licensed dictionary downloaded from https://github.com/exebook/hashdict.c.git

# Original README

This is my REALLY FAST implementation of a hash table in C, in under 200 lines of code.

This is in fact a port of my [hashdic][cppversion] previously written in C++ for [jslike][jslike] project (which is a `var` class making programming in C++ as easy as in JavaScript).

MIT licensed.

[cppversion]: https://github.com/exebook/hashdic
[jslike]: https://github.com/exebook/jslike

For some reason it is more than twice as fast on my benchmarks as the [hash table][redisdictc] used in Redis. But unlike Redis version of a hash table there is no incremental resize.

The hash function used is my adaptation of [Meiyan][cmp2]/7zCRC, it is [better than MurMur3][cmp1].

[cmp1]: https://www.strchr.com/hash_functions
[cmp2]: http://www.sanmayce.com/Fastest_Hash/
[redisdictc]: https://github.com/antirez/redis/blob/unstable/src/dict.c

Hash slot duplicates are stored as linked list `node = node->next`.

## example

```c
#include <inttypes.h>
#include <stdio.h>

#include "sxe-dict.h"

int
main(void) {
    struct sxe_dict *dic;
    const void      **value_ptr;
    const void       *value;

    dic = sxe_dict_new(0);

    value_ptr = sxe_dict_add(dic, "ABC", 3);
    is(*value_ptr, NULL, "New entry should not have a value");
    *value_ptr = (const void *)100;

    sxe_dict_add(dic, "DE", 2);
    *value_ptr = (const void *)200;

    sxe_dict_add(dic, "HJKL", 4);
    *value_ptr = (const void *)300;

    if ((value = sxe_dict_find(dic, "ABC",  3)))
        printf("ABC found: %"PRIiPTR"\n",  (intptr_t)value);
    else
        printf("error\n");

    if ((value = sxe_dict_find(dic, "DE",   2)))
        printf("DE found: %"PRIiPTR"\n",   (intptr_t)value);
    else
        printf("error\n");

    if ((value = sxe_dict_find(dic, "HJKL", 4)))
        printf("HJKL found: %"PRIiPTR"\n", (intptr_t)value);
    else
        printf("error\n");

    sxe_dict_delete(dic);
}

```
## sxe_dict_add()

Add a new key to the hash table.

Unlike most of implementations, you do NOT supply the value as the argument for the `add()` function. Instead after
`sxe_dict_add()` returns value_ptr, set the value like this: `*value_ptr = <VALUE>`.

`const void ** sxe_dict_add(struct sxe_dict *dic, void *key, int keyn);`

Returns a pointer to the value, which is itself a void pointer. If the key was already in the table, the value will likely have
been set non-NULL; otherwise, it will be NULL. In both cases you can change the associated value.

## sxe_dict_find()

Lookup the key in the hash table. Return `true`(`1`) if found, the you can get the value like this: `myvalue = *dic->value`.

`const void * sxe_dict_find(struct sxe_dict *dic, void *key, int keyn);`

## sxe_dict_new()

Create the hash table.

`struct sxe_dict * sxe_dict_new(int initial_size);`

Set `initial_size` to the initial size of the table. Useful when you know how much keys you will store and want to preallocate,
in which case use N/growth_treshold as the initial_size. `growth_threshold` is 2.0 by default.

## sxe_dict_delete()

Delete the hash table and frees all occupied memory.

`void sxe_dict_delete(struct sxe_dict *dic);`

## sxe_dict_forEach()

Iterates over all keys in the table and calls the specified callback for each of them.

`void sxe_dict_forEach(struct sxe_dict *dic, enumFunc f, void *user);`

`typedef int (*enumFunc)(void *key, int count, const *value, void *user);`


## tuning


#### growth (resize)
`struct sxe__dict` has tuning fields:

`growth_threshold`: when to resize, for example `0.5` means "if number of inserted keys is half of the table length then resize". Default: `2.0`;

The original authors experiments on English dictionary shows balanced performance/memory savings with 1.0 to 2.0.

`growth_factor`: grow the size of hash table by N. Suggested number is between 2 (conserve memory) and 10 (faster insertions).

The key is a combination of a pointer to bytes and a count of bytes.



