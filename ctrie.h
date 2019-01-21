/*
 * A compressed trie data structure implementation aimed for high performance
 * (i.e., fast insertion and searching) while maintaining reasonably low memory
 * consumption.
 *
 * David Čepelík <david.cepelik@showmax.com> (c) 2018
 * Ondřej Hrubý <o@hrubon.cz> (c) 2018
 */

#ifndef TRIE_H
#define TRIE_H

#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>

/*
 * Compressed trie.
 */
struct ctrie
{
	struct ctnode *fake_root; /* fake root node to simplify code */
	size_t data_size;         /* number of bytes to allocate for data */
};

/*
 * Init the trie `t`. The trie will allocate `data_size` bytes for the data.
 */
void ctrie_init(struct ctrie *t, size_t data_size);

/*
 * Find node with key `key` and return it. If the key is not present in `t`,
 * return `NULL`.
 */
void *ctrie_find(struct ctrie *t, char *key);

/*
 * Does the trie `t` contain the string `key`?
 */
bool ctrie_contains(struct ctrie *t, char *key);

/*
 * Insert the key `key` into the trie `t` and return a pointer to the memory
 * allocated for data, which is at least `t->data-size` bytes long and aligned
 * at a `sizeof(void *)`-byte boundary.
 *
 * The `wildcard` argument denotes whether the key should be treated as a prefix
 * wildcard.
 */
void *ctrie_insert(struct ctrie *t, char *key, bool wildcard);

/*
 * Remove the key `key` from the trie `t`.
 */
void ctrie_remove(struct ctrie *t, char *key);

/*
 * Print a textual representation of the trie. Useful for debugging only.
 */
void ctrie_dump(struct ctrie *t);

/*
 * Free the trie `t`.
 */
void ctrie_free(struct ctrie *t);

#endif
