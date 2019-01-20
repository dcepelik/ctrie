#include "ctrie.h"
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>

#define KEY_MAX_LEN		6
#define LONG_KEY_TEST_SIZE	1024

int inc(char k[KEY_MAX_LEN])
{
	int i;
	for (i = KEY_MAX_LEN - 1; i >= 0; i--) {
		if (++k[i] <= 'c')
			break;
		k[i] = 'a';
	}
	return i >= 0;
}

static void test_seq(void)
{
	struct trie yes, no;
	ctrie_init(&yes);
	ctrie_init(&no);
	char key[KEY_MAX_LEN + 1];
	for (size_t i = 0; i < KEY_MAX_LEN; i++)
		key[i] = 'a';
	key[KEY_MAX_LEN] = '\0';

	do {
		ctrie_insert((rand() % 2) ? &yes : &no, key, false);
	} while (inc(key));

	do {
		if (ctrie_contains(&yes, key))
			assert(!ctrie_contains(&no, key));
		if (ctrie_contains(&no, key))
			assert(!ctrie_contains(&yes, key));
	} while (inc(key));
	ctrie_free(&yes);
	ctrie_free(&no);
}

static void test_long_keys(void)
{
	struct trie t;
	ctrie_init(&t);
	char key[KEY_MAX_LEN];
	for (size_t n = 0; n < LONG_KEY_TEST_SIZE; n++) {
		size_t len = rand() % KEY_MAX_LEN;
		for (size_t i = 0; i < len; i++)
			key[i] = 'a' + (rand() % ('z' - 'a'));
		key[len] = '\0';
		struct tnode *n = ctrie_insert(&t, key, false);
		assert(n == ctrie_find(&t, key));
		assert(n == ctrie_insert(&t, key, false));
	}
	ctrie_free(&t);
}

#include <stdio.h>

#define ENGLISH_WORD_MAX	32

static void test_english_words(void)
{
	struct trie t;
	FILE *f = fopen("words.txt", "r");
	assert(f != NULL);
	size_t size = ENGLISH_WORD_MAX + 1;
	char *word = malloc(size);
	assert(word);
	ssize_t len;
	ctrie_init(&t);
	while ((len = getline(&word, &size, f)) > 0) {
		word[len - 1] = '\0'; /* trim the new-line */
		ctrie_insert(&t, word, false);
		assert(ctrie_contains(&t, word));
	}
	free(word);
	fclose(f);
	ctrie_free(&t);
}

int main(void)
{
	srand(time(NULL));
	test_seq();
	test_long_keys();
	test_english_words();
	return EXIT_SUCCESS;
}
