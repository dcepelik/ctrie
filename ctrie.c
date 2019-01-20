#include "ctrie.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX(a, b)       ((a) >= (b) ? (a) : (b))
#define MIN(a, b)       ((a) <= (b) ? (a) : (b))

#define FLAGS_MASK	0x03
#define NODE_INIT_SIZE	0

typedef unsigned char	byte_t;

/*
 * Node flags.
 */
enum
{
	F_WORD = 1 << 0,	/* it's a word */
	F_WILD = 1 << 1,	/* it's a prefix wild-card */
	F_SEPL = 1 << 2,	/* label allocated separately, use `lptr` */
};

/*
 * Size of the `label` field in the `tnode` structure. This must be enough to
 * hold a `char *`. If the label is shorter than this value, it will be
 * embedded in the field directly, thus saving the overhead of having to create
 * a heap copy of the string.
 */
#define LABEL_SIZE	13

/*
 * A trie node, or more precisely its header. In memory, two arrays (possibly
 * of size 0) always follow: an array of child pointers and a sorted array of
 * characters. The latter array is used to find child index for given input
 * character.
 *
 * The node itself is capable of storing `size` children at any given moment.
 * It is resized to fulfil storage requirements as they change. This ensures
 * that the trie is reasonably compact at all times.
 */
struct tnode
{
	char label[LABEL_SIZE];	/* short-enough label or a pointer to a label */
	byte_t flags;		/* various flags */
	byte_t size;		/* capacity of the `child` array */
	byte_t nchild;		/* number of children */
	struct tnode *child[1];	/* child pointers start here */
};

_Static_assert(sizeof(char *) <= LABEL_SIZE, "Label pointer is longer than LABEL_SIZE");

/*
 * Return pointer to the label of node `n`. This is either pointer to the
 * `label` content if the label was embedded in the `tnode` directly, or the
 * pointer stored in `label` if the label was allocated externally.
 */
static char *get_label(struct tnode *n)
{
	return (n->flags & F_SEPL) ? *(char **)&n->label : n->label;
}

/*
 * Set label of node `n` to label `label`. If the label is short enough, `label`
 * will be copied into the `label` field of the node. If it's longer, create
 * a copy of the string given using `strdup`.
 */
static void set_label(struct tnode *n, char *label)
{
	char *old_label = get_label(n);
	bool need_free = (n->flags & F_SEPL);
	size_t len = strlen(label);
	if (len < sizeof(n->label)) {
		n->flags &= ~F_SEPL;
		memmove(n->label, label, len);
		n->label[len] = '\0';
	} else {
		n->flags |= F_SEPL;
		*(char **)&n->label = strdup(label);
	}
	if (need_free) /* freeing here because label can point into old_label */
		free(old_label);
}

/*
 * Return the number of bytes needed to allocate a node with size `size`.
 */
static size_t alloc_size(size_t size)
{
	return sizeof(struct tnode) - sizeof(void *) + (sizeof(void *) + 1) * size;
}

/*
 * Return a pointer to the character array of the node `n`.
 */
static char *char_array(struct tnode *n)
{
	return (char *)((byte_t *)n + alloc_size(n->size) - n->size);
}

/*
 * Resize the node `n` to size `new_size`.
 */
static struct tnode *resize(struct tnode *n, size_t new_size)
{
	assert(n != NULL);
	n = realloc(n, alloc_size(new_size));
	assert(n != NULL);
	size_t old_size = n->size;
	char *old = char_array(n);
	n->size = new_size;
	if (old_size)
		memmove(char_array(n), old, old_size);
	return n;
}

/*
 * Allocate a new node, making sure that its initial size will be
 * at least `min_size`, i.e. that `min_size` children can subsequently
 * be inserted without the need to reallocate the node.
 */
static struct tnode *new_node(size_t min_size)
{
	size_t size = MAX(min_size, NODE_INIT_SIZE);
	struct tnode *n = malloc(alloc_size(size));
	n->nchild = 0;
	n->size = size;
	n->flags = 0;
	n->label[0] = '\0';
	return n;
}

static size_t find_child_idx(struct tnode *n, char k)
{
	char *a = char_array(n);
	size_t l = 0, r = n->nchild;
	while (l < r) { /* won't run if no child */
		size_t m = (l + r) / 2;
		if (k <= a[m])
			r = m;
		else
			l = m + 1;
	}
	return l; /* 0 if no child */
}

static struct tnode *find_child(struct tnode *n, char k)
{
	size_t idx = find_child_idx(n, k);
	return (idx < n->nchild && char_array(n)[idx] == k) ? n->child[idx] : NULL;
}

#define ARRAY_SHR(a, idx, size) \
	memmove((a) + (idx) + 1, (a) + (idx), ((size) - (idx)) * sizeof(*(a)))

static struct tnode *insert_child(struct tnode *n, char k, struct tnode *child)
{
	if (n->size == n->nchild)
		n = resize(n, MAX(1, MIN(2 * n->size, 256)));
	size_t idx = find_child_idx(n, k);
	assert(idx < n->size);
	char *a = char_array(n);
	ARRAY_SHR(a, idx, n->nchild);
	ARRAY_SHR(n->child, idx, n->nchild);
	a[idx] = k;
	n->child[idx] = child;
	n->nchild++;
	return n;
}

void ctrie_init(struct trie *t)
{
	t->fake_root = insert_child(new_node(1), '\0', new_node(0));
}

static void delete_node(struct tnode *n)
{
	for (size_t i = 0; i < n->nchild; i++)
		delete_node(n->child[i]);
	if (n->flags & F_SEPL)
		free(get_label(n));
	free(n);
}

void ctrie_free(struct trie *t)
{
	delete_node(t->fake_root);
}

struct tnode *ctrie_find(struct trie *t, char *key)
{
	struct tnode *n = t->fake_root->child[0];
	struct tnode *wild = NULL;
	while (n) {
		char *l;
		for (l = get_label(n); *key && *key == *l; l++, key++);
		if (*l) /* label mismatch */
			return wild;
		if (!*key) /* key matched current node */
			return n->flags & F_WORD ? n : wild;
		if (n->flags & F_WILD)
			wild = n;
		n = find_child(n, *key++);
	}
	return wild;
}

bool ctrie_contains(struct trie *t, char *key)
{
	return ctrie_find(t, key) != NULL;
}

static void ctrie_print_node(struct trie *t, struct tnode *n, size_t level)
{
	char *a = char_array(n);
	for (size_t i = 0; i < n->nchild; i++) {
		struct tnode *c = n->child[i];
		for (size_t j = 0; j < 4 * level; j++)
			putchar(' ');
		printf("[%c]->'%s' size=%i alloc=%zuB <", a[i], get_label(c), c->size, alloc_size(c->size));
		if (c->flags & F_WORD)
			putchar('W');
		if (!(c->flags & F_SEPL))
			putchar('E');
		if (c->flags & F_WILD)
			putchar('*');
		printf(">:\n");
		ctrie_print_node(t, c, level + 1);
	}
}

void ctrie_dump(struct trie *t)
{
	ctrie_print_node(t, t->fake_root->child[0], 0);
}

struct tnode *ctrie_insert(struct trie *t, char *key, bool wildcard)
{
	/* TODO assert key not empty */
	struct tnode *n = t->fake_root->child[0], *parent = t->fake_root;
	size_t idx = 0;
	char *l;
	while (1) { /* find longest prefix of key in the trie */
		for (l = get_label(n); *key && *key == *l; l++, key++);
		if (*l || !*key) /* false for root label and non-empty key */
			break;
		size_t next_idx = find_child_idx(n, *key);
		if (next_idx >= n->nchild || char_array(n)[next_idx] != *key)
			break;
		key++;
		parent = n;
		n = n->child[next_idx];
		idx = next_idx;
	}
	if (*l) { /* create new node between `parent` and `n`, split label */
		struct tnode *s = new_node(1);
		s = insert_child(s, *l, n); /* won't trigger resize */
		*l++ = '\0';
		set_label(s, get_label(n));
		parent->child[idx] = s;
		set_label(n, l);
		n = s;
	}
	if (*key) { /* `n` is a prefix for `key`, prolong the path */
		struct tnode *new = new_node(0);
		n = insert_child(n, *key++, new);
		set_label(new, key); /* without the first char */
		parent->child[idx] = n;
		n = new;
	}
	n->flags |= F_WORD;
	if (wildcard)
		n->flags |= F_WILD;
	return n;
}
