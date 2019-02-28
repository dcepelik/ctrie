#include "ctrie.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX(a, b)      ((a) >= (b) ? (a) : (b))
#define MIN(a, b)      ((a) <= (b) ? (a) : (b))

#define FLAGS_MASK     0x03
#define NODE_INIT_SIZE 0

typedef unsigned char  byte_t;

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
 * Size of the `label` field in the `ctnode` structure. This must be enough to
 * hold a `char *`. If the label is shorter than this value, it will be
 * embedded in the field directly, thus saving the overhead of having to create
 * a heap copy of the string.
 */
#define LABEL_SIZE 13

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
struct ctnode
{
	char label[LABEL_SIZE];  /* short-enough label or a pointer to a label */
	byte_t flags;            /* various flags */
	byte_t size;             /* capacity of the `child` array */
	byte_t nchild;           /* number of children */
	struct ctnode *child[];  /* child pointers start here */
};

_Static_assert(sizeof(char *) <= LABEL_SIZE,
	"LABEL_SIZE too small to hold a char *, please increase");

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
 * Set label of node `n` to label `label`. If the label is short enough, `label`
 * will be copied into the `label` field of the node. If it's longer, create
 * a copy of the string given using `strdup`.
 */
static void set_label(struct ctnode *n, char *label)
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
 * Resize the node `n` to size `new_size`.
 */
static struct ctnode *resize(struct ctrie *t, struct ctnode *n, size_t new_size)
{
	assert(n != NULL);
	n = realloc(n, alloc_size(t, new_size));
	assert(n != NULL);
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
	size_t size = MAX(min_size, NODE_INIT_SIZE);
	struct ctnode *n = malloc(alloc_size(t, size));
	n->nchild = 0;
	n->size = size;
	n->flags = 0;
	n->label[0] = '\0';
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

static struct ctnode *insert_child(struct ctrie *t, struct ctnode *n, char k, struct ctnode *child)
{
	if (n->size == n->nchild)
		n = resize(t, n, MAX(1, MIN(2 * n->size, 255)));
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
	t->fake_root = insert_child(t, new_node(t, 1), '\0', new_node(t, 0));
	t->fake_root->child[0]->flags |= F_WORD;
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
 * Find node with key `key` in trie `t`. If found, the node is returned, `*p`
 * is set to the parent of the returned node, `*pp` is set to the parent of `*p`
 * and `*idx` is set so that `(*p)->child[*idx]` is the returned node.
 *
 * If the node isn't found, NULL is returned and the value of `*p`, `*pp` and
 * `*idx` is undefined.
 */
static struct ctnode *find2(struct ctrie *t, char *key, struct ctnode **p, struct ctnode **pp, size_t *idx)
{
	struct ctnode *n = t->fake_root->child[0];
	struct ctnode *wild = NULL;
	*pp = *p = NULL;
	size_t idx2;
	while (n) {
		char *l;
		for (l = get_label(n); *key && *key == *l; l++, key++);
		if (*l) /* label mismatch */
			return wild;
		if (!*key) /* key matched current node */
			return n->flags & F_WORD ? n : wild;
		if (n->flags & F_WILD)
			wild = n;
		*pp = *p;
		*p = n;
		char k = *key++;
		idx2 = find_child_idx(t, n, k);
		if (idx2 >= n->nchild || char_array(t, n)[idx2] != k)
			break;
		n = n->child[idx2];
		*idx = idx2;
	}
	return wild;
}

static struct ctnode *find(struct ctrie *t, char *key)
{
	struct ctnode *p, *pp;
	size_t idx;
	return find2(t, key, &p, &pp, &idx);
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
 * Cut node `n` from trie `t`, where `p` is the parent of `n`.
 */
void cut(struct ctrie *t, struct ctnode *n, struct ctnode *p)
{
	assert(p->nchild == 1);
	assert(p->child[0] == n);
	assert(n->nchild == 1);
	assert(!(n->flags & F_WORD));
	struct ctnode *c = n->child[0];
	char *label_n = get_label(n);
	char *label_c = get_label(c);
	size_t label_n_len = strlen(label_n);
	size_t label_c_len = strlen(label_c);
	size_t label_len = label_n_len + 1 + label_c_len + 1;
	char label[label_len];
	/* TODO avoid double-copying the string */
	memcpy(label, label_n, label_n_len);
	label[label_n_len] = char_array(t, n)[0];
	memcpy(label + label_n_len + 1, label_c, label_c_len);
	label[label_len] = '\0';
	set_label(c, label);
	p->child[0] = c;
	if (n->flags & F_SEPL)
		free(get_label(n));
	free(n);
}

void ctrie_remove(struct ctrie *t, char *key)
{
	struct ctnode *p, *pp;
	size_t idx;
	struct ctnode *n = find2(t, key, &p, &pp, &idx);
	if (!n)
		return; /* TODO indicate error */
	assert(n->flags & F_WORD);
	n->flags &= ~F_WORD;
	if (n->nchild > 1)
		return; /* `n` is a branching point, nothing else to do */
	if (n->nchild) {
		cut(t, n, p);
		return;
	}
	/* `n` is a leaf node */
	assert(p->child[idx] == n);
	if (n->flags & F_SEPL)
		free(get_label(n));
	free(n);
	assert(p->nchild > 1 || p->flags & F_WORD);
	ARRAY_SHIFT(p->child, idx, idx + 1, p->nchild);
	p->nchild--;
	if (p->nchild == 1 && !(p->flags & F_WORD))
		cut(t, p, pp);
}
