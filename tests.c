#include "ctrie.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SEQ_KEY_LEN        6
#define LONG_KEY_LEN       10
#define LONG_KEY_TEST_SIZE 2048
#define ENGLISH_WORD_MAX   45
#define WORDS_FILE         "words.txt"

static void rst(char k[SEQ_KEY_LEN])
{
	for (size_t i = 0; i < SEQ_KEY_LEN; i++)
		k[i] = 'a';
}

int inc(char k[SEQ_KEY_LEN])
{
	int i;
	for (i = SEQ_KEY_LEN - 1; i >= 0; i--) {
		if (++k[i] <= 'c')
			break;
		k[i] = 'a';
	}
	return i >= 0;
}

static void test_iter_seq(void)
{
	struct ctrie t;
	char key[SEQ_KEY_LEN + 1];
	ctrie_init(&t, 0);
	rst(key);
	do {
		ctrie_insert(&t, key, SEQ_KEY_LEN, false);
	} while (inc(key));

	struct ctrie_iter it;
	ctrie_iter_init(&t, &it);

	rst(key);
	struct ctnode *n;
	char *key2 = NULL;
	size_t key2_size = 0;
	size_t key2_len;
	int more;
	while ((n = ctrie_iter_next(&it, &key2, &key2_size, &key2_len))) {
		assert(key2_len == SEQ_KEY_LEN);
		assert(!strncmp(key, key2, SEQ_KEY_LEN));
		more = inc(key);
	}
	assert(!more);
}

static void test_insert_seq(void)
{
	struct ctrie a, b;
	char key[SEQ_KEY_LEN + 1];
	int *d;
	int n = 0;

	ctrie_init(&a, sizeof(*d));
	ctrie_init(&b, sizeof(*d));

	n = 0;
	rst(key);
	do {
		d = ctrie_insert((rand() % 2) ? &a : &b, key, SEQ_KEY_LEN, false);
		*d = n++;
	} while (inc(key));

	n = 0;
	do {
		if (ctrie_contains(&a, key, SEQ_KEY_LEN)) {
			assert(!ctrie_contains(&b, key, SEQ_KEY_LEN));
			d = ctrie_find(&a, key, SEQ_KEY_LEN);
			assert(*d == n);
		}
		if (ctrie_contains(&b, key, SEQ_KEY_LEN)) {
			assert(!ctrie_contains(&a, key, SEQ_KEY_LEN));
			d = ctrie_find(&b, key, SEQ_KEY_LEN);
			assert(*d == n);
		}
		n++;
	} while (inc(key));
	ctrie_free(&a);
	ctrie_free(&b);
}

static void test_insert_long_keys(void)
{
	struct ctrie t;
	char key[LONG_KEY_LEN];
	char *d;

	ctrie_init(&t, LONG_KEY_LEN);
	for (size_t n = 0; n < LONG_KEY_TEST_SIZE; n++) {
		size_t len = rand() % LONG_KEY_LEN;
		for (size_t i = 0; i < len; i++)
			key[i] = rand() % 255; /* intentionally 255 */
		d = ctrie_insert(&t, key, len, false);
		assert(d == ctrie_find(&t, key, len));
		assert(d == ctrie_insert(&t, key, len, false));
	}
	ctrie_free(&t);
}

static void test_insert_english(void)
{
	struct ctrie t;
	FILE *words;
	char *word;
	size_t word_size;
	ssize_t len;
	char *d;

	words = fopen(WORDS_FILE, "r");
	assert(words != NULL);
	word_size = ENGLISH_WORD_MAX + 1 + 1; /* new line and '\0' */
	word = malloc(word_size);
	assert(word);

	ctrie_init(&t, word_size);

	while ((len = getline(&word, &word_size, words)) > 0) {
		word[len - 1] = '\0'; /* trim the new-line */
		d = ctrie_insert(&t, word, strlen(word), false);
		strncpy(d, word, len + 1);
	}
	fseek(words, SEEK_SET, 0);
	while ((len = getline(&word, &word_size, words)) > 0) {
		word[len - 1] = '\0'; /* strip trailing newline */
		assert(ctrie_contains(&t, word, strlen(word)));
		d = ctrie_find(&t, word, strlen(word));
		assert(strcmp(d, word) == 0);
	}
	free(word);
	fclose(words);
	ctrie_free(&t);
}

/*
 * Test removal of sequential keys. Keys are removed in the order in which they
 * were inserted.
 *
 * This test constructs two sets of keys `a` and `b`, which are disjoint. The
 * union of these two sets is precisely `c`. In each step, one key is removed
 * from `a` and inserted into `b`. Then, we check that the relationships above
 * still hold, which is equivalent to: all deleted keys were removed from `a`
 * and all non-deleted keys are still present in `a`.
 */
static void test_remove_seq(void)
{
	struct ctrie a, b, c;
	char key[SEQ_KEY_LEN];

	ctrie_init(&a, 0);
	ctrie_init(&c, 0);
	ctrie_init(&b, 0);

	rst(key);
	do {
		ctrie_insert(&a, key, SEQ_KEY_LEN, false);
		ctrie_insert(&c, key, SEQ_KEY_LEN, false);
	} while (inc(key));

	struct ctrie_iter it;
	char *key2 = NULL;
	size_t key2_size = 0;
	size_t key2_len;
	rst(key);
	do {
		ctrie_remove(&a, key, SEQ_KEY_LEN);
		assert(!ctrie_contains(&a, key, SEQ_KEY_LEN));
		ctrie_insert(&b, key, SEQ_KEY_LEN, false);
		/* check that no deleted key is found */
		ctrie_iter_init(&b, &it);
		while (ctrie_iter_next(&it, &key2, &key2_size, &key2_len))
			assert(!ctrie_contains(&a, key2, key2_len));
		ctrie_iter_free(&it);
		/* check that all non-deleted keys are still found */
		ctrie_iter_init(&c, &it);
		while (ctrie_iter_next(&it, &key2, &key2_size, &key2_len))
			if (!ctrie_contains(&b, key2, key2_len))
				assert(ctrie_contains(&a, key2, key2_len));
		ctrie_iter_free(&it);
	} while (inc(key));
}

size_t ceil2(size_t n);
int main(void)
{
	srand(time(NULL));
	test_iter_seq();
	test_insert_seq();
	test_insert_long_keys();
	test_insert_english();
	test_remove_seq();
	return EXIT_SUCCESS;
}
