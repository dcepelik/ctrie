/*
 * A compressed trie data structure implementation aimed for high performance
 * (i.e., fast insertion and searching) while maintaining reasonably low memory
 * consumption.
 *
 * David Čepelík <david.cepelik@showmax.com> (c) 2018-2020
 * Ondřej Hrubý <o@hrubon.cz> (c) 2018
 */

#ifndef CTRIE_H
#define CTRIE_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Compressed trie.
 */
struct ctrie
{
	struct ctnode *fake_root; /* fake root node to simplify code */
	size_t data_size;         /* number of bytes to allocate for data */
};

/*
 * Init `t`. The trie will allocate `data_size` bytes for data in each node.
 *
 * TODO: Allocating only makes sense for word nodes, not for e.g. branching
 *       nodes. Currently we allocate everywhere, which sucks.
 */
void ctrie_init(struct ctrie *t, size_t data_size);

/*
 * Find node with by `key` and return it. If `key` is not present in `t`,
 * return `NULL`.
 */
void *ctrie_find(struct ctrie *t, char *key);

/*
 * Does `t` contain `key`?
 */
bool ctrie_contains(struct ctrie *t, char *key);

/*
 * Insert `key` into `t` and return a pointer to the memory allocated for data,
 * which is at least `t->data_size` bytes long and aligned at `sizeof(void*)`.
 *
 * The `wildcard` argument denotes whether the key should be treated as a prefix
 * wildcard.
 */
void *ctrie_insert(struct ctrie *t, char *key, bool wildcard);

/*
 * Remove `key` from `t`. If `key` is not found in `t`, do nothing.
 */
void ctrie_remove(struct ctrie *t, char *key);

/*
 * Print a textual representation of the trie. Useful for debugging only.
 */
void ctrie_dump(struct ctrie *t);

/*
 * Free `t`.
 */
void ctrie_free(struct ctrie *t);

/*
 * A trie node iterator.
 */
struct ctrie_iter
{
	struct ctrie *t;                 /* the trie which we're walking */
	struct ctrie_iter_stkent *stack; /* iteration stack entries */
	size_t stack_size;               /* size of `stack` array */
	size_t nstack;                   /* number of items on the stack */
};

/*
 * Initialize the iterator `it` to walk the trie `t`. Subsequent calls to
 * `ctrie_iter_next` will return the nodes corresponding to words of `t` 
 * in infix (NLR) order.
 */
void ctrie_iter_init(struct ctrie *t, struct ctrie_iter *it);

/*
 * Retrieve next node from `it`. After the operation, `*np` points to the next
 * node (or NULL if there is none),`*key` contains the key of the node and
 * `*key_size` is the size, in bytes, of the memory that `*key` points to.
 *
 * If `*key` is `NULL` before the call and `*key_size` is set to `0`, memory
 * will be allocated to hold the `*key` using `realloc(3)`. If `*key` is not
 * `NULL` prior to the call, then `*key` may be reallocated to some larger size
 * using `realloc(3)` and updating `*key_size` accordingly, to accommodate the
 * entire key.
 *
 * After the iterator is disposed by a call to `iter_free`, it's the caller's
 * responsibility to `free(3)` the `*key`.
 *
 * The memory allocation scheme was modeled after `getline(3)`.
 */
struct ctnode *ctrie_iter_next(struct ctrie_iter *it,
                               char **key,
                               size_t *key_size);

/*
 * Dispose `it`.
 */
void ctrie_iter_free(struct ctrie_iter *it);

#endif
