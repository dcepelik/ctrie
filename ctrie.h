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
struct trie
{
	struct tnode *fake_root;	/* fake root node to simplify code */
};

/*
 * Init the trie `t`.
 */
void ctrie_init(struct trie *t);

/*
 * Find node with key `key` and return it. If the key is not present in `t`,
 * return `NULL`.
 */
struct tnode *ctrie_find(struct trie *t, char *key);

/*
 * Does the trie `t` contain the string `key`?
 */
bool ctrie_contains(struct trie *t, char *key);

/*
 * Insert the key `key` into the trie `t`. The `wildcard` argument denotes
 * whether the key should be treated as a prefix wildcard.
 */
struct tnode *ctrie_insert(struct trie *t, char *key, bool wildcard);

/*
 * Remove the key `key` from the trie `t`.
 */
struct tnode *ctrie_remove(struct trie *t, char *key);

/*
 * Print a textual representation of the trie. Useful for debugging only.
 */
void ctrie_dump(struct trie *t);

/*
 * Free the trie `t`.
 */
void ctrie_free(struct trie *t);

#endif
