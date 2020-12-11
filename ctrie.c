#include "ctrie.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX(a, b)      ((a) >= (b) ? (a) : (b))
#define MIN(a, b)      ((a) <= (b) ? (a) : (b))

#define FLAGS_MASK     0x03
#define NODE_INIT_SIZE 0
#define NODE_MAX_SIZE  255
#define LABEL_BUF_SIZE 128

_Static_assert(NODE_INIT_SIZE <= NODE_MAX_SIZE,
	"initial node size may not exceed maximum node size");

typedef unsigned char  byte_t;

static void *xrealloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (!ptr) {
		perror("realloc");
		abort();
	}
	return ptr;
}

static void *xmalloc(size_t size)
{
	return xrealloc(NULL, size);
}

static void *xcalloc(size_t nmemb, size_t size)
{
	void *ptr = calloc(nmemb, size);
	if (!ptr) {
		perror("calloc");
		abort();
	}
	return ptr;
}

static char *xstrdup(char *s)
{
	// To not introduce unnecessary POSIX dependency, we copied the
	// implementation of strdup(3p) from musl. Please refer to:
	// http://git.musl-libc.org/cgit/musl/tree/src/string/strdup.c
	size_t l = strlen(s);
	char *d = malloc(l+1);
	if (!d) {
		perror("strdup");
		abort();
	}
	return memcpy(d, s, l+1);
}

/*
 * Size of the `label` field in `struct ctnode`. This must be enough to hold a
 * `char *`. If the label is shorter than this value, it will be embedded in
 * the field directly, thus saving the overhead of having to create a heap copy
 * of the string.
 *
 * LABEL_SIZE is set to 13 since that will result in in plausible alignment
 * on 64-bit platforms: sizeof(struct ctnode) == 16 and no padding is required.
 */
#define LABEL_SIZE 13

_Static_assert(sizeof(char *) <= LABEL_SIZE,
	"LABEL_SIZE too small to hold a char *, please increase");

/*
 * Node flags.
 */
enum
{
	F_USER = 1 << 0, /* user-defined boolean flag */
	F_WORD = 1 << 1, /* it's a word */
	F_WILD = 1 << 2, /* it's a prefix wild-card */
	F_SEPL = 1 << 3, /* label allocated separately, use `lptr` */
	F_SEPD = 1 << 4, /* data allocated separately */
};

/*
 * A trie node, or more precisely its header. In memory, two arrays (possibly
 * of size 0) always follow: an array of child pointers and a sorted array of
 * characters. The latter array is used to find child index for given input
 * character.
 *
 * The node itself is capable of storing `size` children at any given moment.
 * It is resized to fulfil storage requirements as they change. This ensures
 * that the trie is reasonably compact at all times.
 *
 * Please note that node size is limited to 255 children. This is fine since
 * the NUL byte ('\0') cannot appear inside of a string, so we cannot have
 * a branching node which would intersect paths at 256 different bytes.
 *
 * FIXME Enforce proper alignment for cases when people chose poor LABEL_SIZE.
 *       (Must be safe to reinterpret label as a char *.)
 */
struct ctnode
{
	char label[LABEL_SIZE];  /* short-enough label or pointer to a label */
	byte_t flags;            /* various flags */
	byte_t size;             /* capacity of the `child` array */
	byte_t nchild;           /* number of children */
	struct ctnode *child[];  /* child pointers start here */
};

/*
 * Return pointer to the label of node `n`. This is either pointer to the
 * `label` content if the label was embedded in the `ctnode` directly, or the
 * pointer stored in `label` if the label was allocated externally.
 */
static char *get_label(struct ctnode *n)
{
	return (n->flags & F_SEPL) ? *(char **)&n->label : n->label;
}

/*
 * Set label of `n` to `label`. If the label is short enough, `label` will be
 * copied into the `label` field of the node. If it's longer, create a copy of
 * the string given using `strdup`.
 */
static void set_label(struct ctnode *n, char *label)
{
	char *old_label = get_label(n);
	bool need_free = (n->flags & F_SEPL);
	size_t len = strlen(label);
	if (len < sizeof(n->label)) {
		n->flags &= ~F_SEPL;
		/* memmove: label may be equal to n->label if old label short */
		memmove(n->label, label, len);
		n->label[len] = '\0';
	} else {
		n->flags |= F_SEPL;
		*(char **)&n->label = xstrdup(label);
	}
	if (need_free)
		free(old_label);
}

/*
 * Return the number of bytes needed to allocate a node with size `size`.
 */
static size_t alloc_size(struct ctrie *t, size_t size)
{
	size_t ptrs = size * sizeof(struct ctnode *);
	size_t chars = size;
	return sizeof(struct ctnode) + ptrs + t->data_size + chars;
}

/*
 * Return a pointer to the character array of the node `n`.
 */
static char *char_array(struct ctrie *t, struct ctnode *n)
{
	return (char *)((byte_t *)n + alloc_size(t, n->size) - n->size);
}

/*
 * Return a pointer to the data memory of the node `n`.
 */
static void *data(struct ctrie *t, struct ctnode *n)
{
	return (void *)(char_array(t, n) - t->data_size);
}

/*
 * Resize `n` to `new_size`.
 */
static struct ctnode *resize(struct ctrie *t, struct ctnode *n, size_t new_size)
{
	assert(new_size <= NODE_MAX_SIZE);
	assert(n->size <= new_size);
	n = xrealloc(n, alloc_size(t, new_size));
	size_t old_size = n->size;
	void *old = data(t, n);
	n->size = new_size;
	/* copy both data and the char array to the new node */
	memmove(data(t, n), old, t->data_size + old_size);
	return n;
}

/*
 * Allocate a new node, making sure that its initial size will be
 * at least `min_size`, i.e. that `min_size` children can subsequently
 * be inserted without the need to reallocate the node.
 */
static struct ctnode *new_node(struct ctrie *t, size_t min_size)
{
	assert(min_size <= NODE_MAX_SIZE);
	size_t size = MAX(min_size, NODE_INIT_SIZE);
	struct ctnode *n = xcalloc(1, alloc_size(t, size));
	n->size = size;
	return n;
}

static size_t find_child_idx(struct ctrie *t, struct ctnode *n, char k)
{
	char *a = char_array(t, n);
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

#define ARRAY_SHIFT(a, j, i, size) \
	memmove((a) + (j), (a) + (i), ((size) - (i)) * sizeof(*(a)))

static struct ctnode *insert_child(struct ctrie *t,
                                   struct ctnode *n,
                                   char k,
                                   struct ctnode *child)
{
	if (n->size == n->nchild)
		n = resize(t, n, MAX(1, MIN(2 * n->size, NODE_MAX_SIZE)));
	size_t idx = find_child_idx(t, n, k);
	assert(idx < n->size);
	char *a = char_array(t, n);
	ARRAY_SHIFT(a, idx + 1, idx, n->nchild);
	ARRAY_SHIFT(n->child, idx + 1, idx, n->nchild);
	a[idx] = k;
	n->child[idx] = child;
	n->nchild++;
	return n;
}

void ctrie_init(struct ctrie *t, size_t data_size)
{
	t->data_size = data_size;
	// FIXME Alloc check
	t->fake_root = insert_child(t, new_node(t, 1), '\0', new_node(t, 0));
	//t->fake_root->child[0]->flags |= F_WORD;
}

static void delete_node(struct ctnode *n)
{
	for (size_t i = 0; i < n->nchild; i++)
		delete_node(n->child[i]);
	if (n->flags & F_SEPL)
		free(get_label(n));
	free(n);
}

void ctrie_free(struct ctrie *t)
{
	delete_node(t->fake_root);
}

/*
 * Find a node with key `key` in trie `t` and its two immediate predecessors.
 *
 * If node with the given key is found, it's returned and `*p` points to the
 * returned node's parent, `*pi` is the returned node's index in the child
 * array of `*p`, `*pp` is the parent of `*p` and `*ppi` is the index of `*p`
 * in the child array of `*pp`.
 *
 * If there's no node with the given key if `t`, but a wild-card node is
 * encountered during the search descent, last such node will be returned and
 * `*p`, `*pi`, `*pp` and `*ppi` will be set accordingly.
 *
 * If the node is not found, `NULL` is returned and the value of `*p`, `*pi`,
 * `*pp` and `*ppi` is undefined.
 *
 * The reason we need this method (with two immediate predecessors returned
 * alongside the node) is that we don't keep parent pointers in the nodes of
 * the trie to conserve space. Since all operations on `t` require at most the
 * grand-parent of modified node, this seems reasonable.
 */
static inline struct ctnode *find3(struct ctrie *t,
                                   char *key,
                                   struct ctnode **pp,
                                   size_t *ppi,
                                   struct ctnode **p,
                                   size_t *pi)
{
	*ppi = *pi = 0;
	*pp = NULL;
	*p = t->fake_root;
	struct ctnode *w = NULL, *wp = *p, *wpp = *pp;
	size_t wpi = 0, wppi = 0;
	struct ctnode *n = t->fake_root->child[0];
	while (n) {
		char *l;
		for (l = get_label(n); *key && *key == *l; l++, key++);
		if (*l) /* label mismatch */
			break;
		if (!*key) { /* key matched current node */
			if (n->flags & F_WORD)
				return n;
			break;
		}
		if (n->flags & F_WILD) {
			/* save the wild node and current search state */
			w = n;
			wpp = *pp;
			wppi = *ppi;
			wp = *p;
			wpi = *pi;
		}
		*pp = *p;
		*ppi = *pi;
		*p = n;
		char k = *key++;
		*pi = find_child_idx(t, n, k);
		if (*pi >= n->nchild || char_array(t, n)[*pi] != k)
			break;
		n = (*p)->child[*pi];
		assert((*pp)->child[*ppi] == *p);
		assert((*p)->child[*pi] == n);
	}
	/* return last wild-card node encountered during the search (if any) */
	*pp = wpp;
	*ppi = wppi;
	*p = wp;
	*pi = wpi;
	return w;
}

static struct ctnode *find(struct ctrie *t, char *key)
{
	struct ctnode *p, *pp;
	size_t pi, ppi;
	return find3(t, key, &pp, &ppi, &p, &pi);
}

void *ctrie_find(struct ctrie *t, char *key)
{
	struct ctnode *n = find(t, key);
	return n ? data(t, n) : NULL;
}

bool ctrie_contains(struct ctrie *t, char *key)
{
	return find(t, key) != NULL;
}

static void ctrie_print_node(struct ctrie *t, struct ctnode *n, size_t level)
{
	char *a = char_array(t, n);
	for (size_t i = 0; i < n->nchild; i++) {
		struct ctnode *c = n->child[i];
		for (size_t j = 0; j < 4 * level; j++)
			putchar(' ');
		printf("[%c]->'%s' size=%i alloc=%zuB <",
			a[i],
			get_label(c),
			c->size,
			alloc_size(t, c->size));
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

void ctrie_dump(struct ctrie *t)
{
	ctrie_print_node(t, t->fake_root->child[0], 0);
}

void *ctrie_insert(struct ctrie *t, char *key, bool wildcard)
{
	/* TODO assert key not empty */
	struct ctnode *n = t->fake_root->child[0], *parent = t->fake_root;
	size_t idx = 0;
	char *l;
	while (1) { /* find longest prefix of key in the trie */
		for (l = get_label(n); *key && *key == *l; l++, key++);
		if (*l || !*key) /* false for root label and non-empty key */
			break;
		size_t next_idx = find_child_idx(t, n, *key);
		if (next_idx >= n->nchild || char_array(t, n)[next_idx] != *key)
			break;
		key++;
		parent = n;
		n = n->child[next_idx];
		idx = next_idx;
	}
	if (*l) { /* create new node between `parent` and `n`, split label */
		struct ctnode *s = new_node(t, 1);
		s = insert_child(t, s, *l, n); /* won't trigger resize */
		*l++ = '\0';
		set_label(s, get_label(n));
		parent->child[idx] = s;
		set_label(n, l);
		n = s;
	}
	if (*key) { /* `n` is a prefix for `key`, prolong the path */
		struct ctnode *new = new_node(t, 0);
		n = insert_child(t, n, *key++, new);
		set_label(new, key); /* without the first char */
		parent->child[idx] = n;
		n = new;
	}
	n->flags |= F_WORD;
	if (wildcard)
		n->flags |= F_WILD;
	return data(t, n);
}

/*
 * Cut `n` from `t`, where `p` is the parent of `n`. This assumes that `n` has
 * only a single child and `n` is not a word node, i.e. it can be merged with
 * its only child.
 */
static void cut(struct ctrie *t,
                       struct ctnode *n,
                       struct ctnode *p,
                       size_t pi)
{
	assert(p->nchild >= 1);
	assert(p->child[pi] == n);
	assert(n->nchild == 1);
	assert(!(n->flags & F_WORD));

	char label_buf[LABEL_BUF_SIZE];

	struct ctnode *c = n->child[0];
	char *label_n = get_label(n);
	char *label_c = get_label(c);
	size_t label_n_len = strlen(label_n);
	size_t label_c_len = strlen(label_c);
	size_t label_len = label_n_len + 1 + label_c_len;

	char *label;
	if (label_len < sizeof(label_buf))
		label = label_buf;
	else
		label = xmalloc(label_len + 1);

	memcpy(label, label_n, label_n_len);
	label[label_n_len] = char_array(t, n)[0];
	memcpy(label + label_n_len + 1, label_c, label_c_len);
	label[label_len] = '\0';

	/* TODO we're basically double-copying the label - avoid that */
	set_label(c, label);
	p->child[pi] = c;

	if (label != label_buf)
		free(label);
	if (n->flags & F_SEPL)
		free(label_n);
	free(n);
}

void ctrie_remove(struct ctrie *t, char *key)
{
	struct ctnode *pp, *p;
	size_t ppi, pi;
	struct ctnode *n = find3(t, key, &pp, &ppi, &p, &pi);
	if (!n)
		return;

	assert(n->flags & F_WORD);
	n->flags &= ~F_WORD;

	// The node is internal branching node. Clearing F_WORD is
	// enough, the node must be kept.
	if (n->nchild > 1)
		return;

	// The node has a single child. We will cut the node and it's
	// sole child will become a child of the parent.
	if (n->nchild) {
		cut(t, n, p, pi);
		return;
	}

	assert(p->nchild > 1 || p->flags & F_WORD || pp == t->fake_root);
	assert(p->child[pi] == n);

	// Otherwise, the node is a leaf.
	ARRAY_SHIFT(p->child, pi, pi + 1, p->nchild);
	ARRAY_SHIFT(char_array(t, p), pi, pi + 1, p->nchild);
	if (n->flags & F_SEPL)
		free(get_label(n));
	free(n);
	p->nchild--;
	if (p->nchild == 1 && !(p->flags & F_WORD) && pp != t->fake_root)
		cut(t, p, pp, ppi);
}

/*
 * Array growing helper. Ensure that the array `a` of `sizeof(*a)`-sized items
 * with current capacity `cap` can hold `size + 1` items (i.e., is not full).
 * If `size` has reached `a`'s capacity limit, use `realloc(3)` to resize the
 * array to at least twice current capacity.
 */
#define AGROW(a, size, cap) do { \
	if ((size) >= (cap)) { \
		(cap) = MAX(size, MAX(1, 2 * (cap))); \
		(a) = xrealloc((a), (cap) * sizeof(*(a))); \
	} \
} while (0)

/*
 * Iterator stack entry. Together, these objects hold the entire iteration state
 * of the associated iterator.
 */
struct ctrie_iter_stkent
{
	struct ctnode *n; /* node that this entry describes */
	size_t idx;       /* current child index in `n` */
	size_t key_len;   /* length of `n`'s key */
};

/*
 * Push and return new stack entry for node `n` onto the stack of iterator `it`.
 */
static struct ctrie_iter_stkent *push(struct ctrie_iter *it, struct ctnode *n)
{
	AGROW(it->stack, it->nstack, it->stack_size);
	struct ctrie_iter_stkent *c = &it->stack[it->nstack++];
	c->n = n;
	c->idx = SIZE_MAX;
	return c;
}

void ctrie_iter_init(struct ctrie *t, struct ctrie_iter *it)
{
	it->t = t;
	it->stack = NULL;
	it->stack_size = it->nstack = 0;
	push(it, t->fake_root->child[0])->key_len = 0;
}

struct ctnode *ctrie_iter_next(struct ctrie_iter *it,
                               char **key,
                               size_t *key_size)
{
	struct ctrie_iter_stkent *se;
	struct ctnode *n;
	size_t key_len;
	size_t label_len;
	while (it->nstack) {
		se = &it->stack[it->nstack - 1];
		se->idx++; /* deliberate overflow */
		if (se->idx >= se->n->nchild) {
			it->nstack--;
			continue;
		}
		char c = char_array(it->t, se->n)[se->idx];
		n = se->n->child[se->idx];
		char *label = get_label(n);
		label_len = strlen(label);
		key_len = se->key_len + 1 + label_len;
		AGROW(*key, key_len + 1, *key_size);
		(*key)[se->key_len] = c;
		memcpy(*key + se->key_len + 1, label, label_len);
		(*key)[key_len] = '\0';
		if (n->nchild)
			push(it, n)->key_len = key_len;
		if (n->flags & F_WORD)
			return n;
	}
	return NULL;
}

void ctrie_iter_free(struct ctrie_iter *it)
{
	free(it->stack);
}
