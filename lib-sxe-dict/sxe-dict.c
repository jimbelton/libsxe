#include <xmmintrin.h>

#include "sxe-alloc.h"
#include "sxe-dict.h"

#define hash_func       meiyan

struct sxe_dict_node {
    struct sxe_dict_node *next;
    char                 *key;
    size_t                len;
    const void           *value;
};

static inline uint32_t
meiyan(const char *key, size_t count)
{
    typedef const uint32_t * P;
    uint32_t h = 0x811c9dc5;

    while (count >= 8) {
        h = (h ^ ((((*(P)key) << 5) | ((*(P)key) >> 27)) ^ *(P)(key + 4))) * 0xad3e7;
        count -= 8;
        key += 8;
    }

#define tmp h = (h ^ *(const uint16_t *)key) * 0xad3e7; key += 2;
    if (count & 4) { tmp tmp }
    if (count & 2) { tmp }
    if (count & 1) { h = (h ^ *key) * 0xad3e7; }
#undef tmp

    return h ^ (h >> 16);
}

struct sxe_dict_node *
sxe_dict_node_new(const char *k, size_t l)
{
    struct sxe_dict_node *node = sxe_malloc(sizeof(struct sxe_dict_node));
    node->len = l;
    node->key = sxe_malloc(l);
    memcpy(node->key, k, l);
    node->next  = 0;
    node->value = NULL;
    return node;
}

static void
sxe_dict_node_delete(struct sxe_dict_node *node)
{
    sxe_free(node->key);

    if (node->next)
        sxe_dict_node_delete(node->next);

    sxe_free(node);
}

struct sxe_dict *
sxe_dict_new(int initial_size)
{
    struct sxe_dict* dic = sxe_malloc(sizeof(struct sxe_dict));

    dic->size   = initial_size;
    dic->count  = 0;
    dic->table  = initial_size ? sxe_calloc(sizeof(struct sxe_dict_node*), initial_size) : NULL;
    dic->load   = 100;    // As many entries as there are buckets
    dic->growth = 2;      // Double when load exceeded
    return dic;
}

void
sxe_dict_delete(struct sxe_dict *dic)
{
    for (unsigned i = 0; i < dic->size; i++) {
        if (dic->table[i])
            sxe_dict_node_delete(dic->table[i]);
    }

    sxe_free(dic->table);
    dic->table = 0;
    sxe_free(dic);
}

static void
sxe_dict_reinsert_when_resizing(struct sxe_dict *dic, struct sxe_dict_node *k2)
{
    int n = hash_func(k2->key, k2->len) % dic->size;

    if (dic->table[n] == 0) {
        dic->table[n] = k2;
        return;
    }

    struct sxe_dict_node *k = dic->table[n];
    k2->next = k;
    dic->table[n] = k2;
}

void
sxe_dict_resize(struct sxe_dict *dic, int newsize)
{
    int o = dic->size;
    struct sxe_dict_node **old = dic->table;
    dic->table = sxe_calloc(sizeof(struct sxe_dict_node*), newsize);
    dic->size = newsize;

    for (int i = 0; i < o; i++) {
        struct sxe_dict_node *k = old[i];
        while (k) {
            struct sxe_dict_node *next = k->next;
            k->next = 0;
            sxe_dict_reinsert_when_resizing(dic, k);
            k = next;
        }
    }

    sxe_free(old);
}

/**
 * Add a key to a dictionary
 *
 * @param dic  The dictionary
 * @param key  The key
 * @param keyn The size of the key, or 0 if it's a string to determine its length with strlen
 *
 * @return A pointer to a value. If there is a collision, the value it points to should be something other than NULL.
 *
 * @note The caller is expected to save a non-NULL value in the value pointed at by the return value.
 */
const void **
sxe_dict_add(struct sxe_dict *dic, const void *key, size_t keyn)
{
    struct sxe_dict_node **link;

    keyn = keyn ?: strlen(key);

    if (dic->table == NULL) {    // If this is a completely empty dictionary
        dic->table    = sxe_calloc(sizeof(struct sxe_dict_node*), 1);
        dic->size     = 1;
    }

    unsigned hash   = hash_func((const char *)key, keyn);
    unsigned bucket = hash % dic->size;

    if (dic->table[bucket] != NULL) {
        unsigned load = dic->count * 100 / dic->size;

        if (load >= dic->load)
            sxe_dict_resize(dic, dic->size * dic->growth);

        bucket = hash % dic->size;
    }

    for (link = &dic->table[bucket]; *link != NULL; link = &((*link)->next)) {
    }

    *link = sxe_dict_node_new((const char *)key, keyn);
    dic->count++;
    return &((*link)->value);
}

/**
 * Find a key in a dictionary
 *
 * @param dic  The dictionary
 * @param key  The key
 * @param keyn The size of the key, or 0 if it's a string to determine its length with strlen
 *
 * @return The value, or NULL if the key is not found.
 */
const void *
sxe_dict_find(struct sxe_dict *dic, const void *key, size_t keyn)
{
    if (dic->table == NULL)    // If the dictionary is empty and its initial_size was 0, the key is not found.
        return NULL;

    unsigned n = hash_func((const char *)key, keyn) % dic->size;
    #if defined(__MINGW32__) || defined(__MINGW64__)
    __builtin_prefetch(gc->table[n]);
    #endif

    #if defined(_WIN32) || defined(_WIN64)
    _mm_prefetch((char*)gc->table[n], _MM_HINT_T0);
    #endif
    struct sxe_dict_node *k = dic->table[n];

    if (!k)
        return NULL;

    while (k) {
        if (k->len == keyn && !memcmp(k->key, key, keyn))
            return k->value;

        k = k->next;
    }

    return NULL;
}

void
sxe_dict_forEach(struct sxe_dict *dic, sxe_dict_iter f, void *user)
{
    for (unsigned i = 0; i < dic->size; i++) {
        if (dic->table[i] != 0) {
            struct sxe_dict_node *k = dic->table[i];

            while (k) {
                if (!f(k->key, k->len, &k->value, user))
                    return;

                k = k->next;
            }
        }
    }
}
#undef hash_func
