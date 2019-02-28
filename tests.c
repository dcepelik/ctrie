#include "ctrie.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define KEY_MAX_LEN        11
#define LONG_KEY_TEST_SIZE 1024
#define ENGLISH_WORD_MAX   45
#define WORDS_FILE         "words.txt"

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
	struct ctrie a, b;
	char key[KEY_MAX_LEN + 1];
	int *d;
	int n = 0;

	ctrie_init(&a, sizeof(*d));
	ctrie_init(&b, sizeof(*d));

	for (size_t i = 0; i < KEY_MAX_LEN; i++)
		key[i] = 'a';
	key[KEY_MAX_LEN] = '\0';

	n = 0;
	do {
		d = ctrie_insert((rand() % 2) ? &a : &b, key, false);
		*d = n++;
	} while (inc(key));

	n = 0;
	do {
		if (ctrie_contains(&a, key)) {
			assert(!ctrie_contains(&b, key));
			d = ctrie_find(&a, key);
			assert(*d == n);
		}
		if (ctrie_contains(&b, key)) {
			assert(!ctrie_contains(&a, key));
			d = ctrie_find(&b, key);
			assert(*d == n);
		}
		n++;
	} while (inc(key));
	ctrie_free(&a);
	ctrie_free(&b);
}

static void test_long_keys(void)
{
	struct ctrie t;
	char key[KEY_MAX_LEN];
	char *d;

	ctrie_init(&t, KEY_MAX_LEN);
	for (size_t n = 0; n < LONG_KEY_TEST_SIZE; n++) {
		size_t len = rand() % KEY_MAX_LEN;
		for (size_t i = 0; i < len; i++)
			key[i] = 'a' + (rand() % ('z' - 'a'));
		key[len] = '\0';
		d = ctrie_insert(&t, key, false);
		assert(d == ctrie_find(&t, key));
		assert(d == ctrie_insert(&t, key, false));
	}
	ctrie_free(&t);
}

static void test_english_words(void)
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
		d = ctrie_insert(&t, word, false);
		strncpy(d, word, len + 1);
	}
	fseek(words, SEEK_SET, 0);
	while ((len = getline(&word, &word_size, words)) > 0) {
		word[len - 1] = '\0'; /* trim the new-line */
		assert(ctrie_contains(&t, word));
		d = ctrie_find(&t, word);
		assert(strcmp(d, word) == 0);
	}
	free(word);
	fclose(words);
	ctrie_free(&t);
}

static void test_remove_seq(void)
{
	struct ctrie a, b;
	char key[KEY_MAX_LEN + 1];

	ctrie_init(&a, 0);

	for (size_t i = 0; i < KEY_MAX_LEN; i++)
		key[i] = 'a';
	key[KEY_MAX_LEN] = '\0';

	do {
		ctrie_insert(&a, key, false);
		assert(ctrie_contains(&a, key));
		ctrie_remove(&a, key);
		assert(!ctrie_contains(&a, key));
	} while (inc(key));
}

int main(void)
{
	srand(time(NULL));
	test_seq();
	test_long_keys();
	test_english_words();
	test_remove_seq();
	return EXIT_SUCCESS;
}
