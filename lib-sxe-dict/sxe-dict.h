#ifndef SXE_DICT_H
#define SXE_DICT_H

#include <stdbool.h>
#include <stdint.h>

typedef bool (*sxe_dict_iter)(void *key, size_t key_size, const void **value, void *user);

struct sxe_dict_node;

struct sxe_dict {
    struct sxe_dict_node **table;    // Pointer to the bucket list or NULL if the dictionary is empty
    unsigned               size;     // Number of buckets
    unsigned               count;    // Number of entries
    unsigned               load;     // Maximum load factor (count/size) as a percentage. 100 -> count == size
    unsigned               growth;   // Growth factor when load exceeded. 2 is for doubling
};

#include "sxe-dict-proto.h"

#endif
