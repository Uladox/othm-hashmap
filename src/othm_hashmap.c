/*    This file is part of Ruspma.
 *
 *    Ruspma is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Ruspma is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Ruspma.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "MurmurHash2.h"
#include "othm_hashmap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DEFAULT_ENTRIES_TO_HASHBINS 5
#define DEFAULT_HASH_SEED 37

static unsigned int Hashmap_Primes[24] = {
	256 + 27, 512 + 9, 1024 + 9, 2048 + 5, 4096 + 3, 8192 + 27,
	16384 + 43, 32768 + 3, 65536 + 45, 131072 + 29, 262144 + 3,
	524288 + 21, 1048576 + 7, 2097152 + 17, 4194304 + 15,
	8388608 + 9, 16777216 + 43, 33554432 + 35, 67108864 + 15,
	134217728 + 29, 268435456 + 3, 536870912 + 11, 1073741824 + 85, 0
};

static void rehash(struct othm_hashmap *);

/* Copies value of key pointer into the hashbin */
static struct othm_hashbin *new_hashbin(char *key, void *storage)
{
	struct othm_hashbin *new;

	new = malloc(sizeof(struct othm_hashbin));
	new->key = key;
	new->storage = storage;
	new->next = NULL;
}

/* If key exists saved in some place the hashmap can uses safely
  (such as a hashbin before a rehash) then use this method so
  there is no extra computation */
static struct othm_hashbin *copy_hashbin(char *key, void *storage)
{
	struct othm_hashbin *new;

	new = malloc(sizeof(struct othm_hashbin));
	new->key = key;
	new->storage = storage;
	new->next = NULL;
}

/* frees hashbin without freeing key, used in rehashing */
static void hashbin_free(struct othm_hashbin *hashbin)
{
	free(hashbin);
}

/* creates a new hashmap */
struct othm_hashmap *othm_hashmap_new(void)
{
	struct othm_hashmap *new;
	struct othm_hashbin **hashbin_ptr;
	int i;

	new = malloc(sizeof(struct othm_hashmap));
	new->hashbin_num = Hashmap_Primes[0];
	new->hashbins =
		malloc(sizeof(struct othm_hashbin *) * Hashmap_Primes[0]);
	new->entries_num = 0;
	new->Primes_pointer = Hashmap_Primes;
	hashbin_ptr = new->hashbins;
	for (i = 0; i != Hashmap_Primes[0]; ++i) {
		*hashbin_ptr = NULL;
		++hashbin_ptr;
	}
	return new;
}

/* Avoid uneeded rehashings by setting up the hashmap at creation
   with a point in the sequence */
struct othm_hashmap *othm_hashmap_new_seq(int sequence)
{
	struct othm_hashmap *new;
	struct othm_hashbin **hashbin_ptr;
	int i;

	new = malloc(sizeof(struct othm_hashmap));
	new->hashbin_num = Hashmap_Primes[sequence];
	new->hashbins =
		malloc(sizeof(struct othm_hashbin *) *
		       Hashmap_Primes[sequence]);
	new->entries_num = 0;
	new->Primes_pointer = Hashmap_Primes + sequence;
	hashbin_ptr = new->hashbins;
	for (i = 0; i != Hashmap_Primes[sequence]; ++i) {
		*hashbin_ptr = NULL;
		++hashbin_ptr;
	}
	return new;
}

/* Final freeing of hashmap */
void othm_hashmap_free(struct othm_hashmap *hashmap)
{
	struct othm_hashbin **current_top_hashbin;
	unsigned int hashbin_num;
	unsigned int i;

	current_top_hashbin = hashmap->hashbins;
	hashbin_num = hashmap->hashbin_num;
	for (i = 0; i != hashbin_num; ++i) {
		struct othm_hashbin *current_hashbin;

		current_hashbin = *current_top_hashbin;
		while (current_hashbin != NULL) {
			struct othm_hashbin *current_hashbin_holder;

			current_hashbin_holder = current_hashbin;
			current_hashbin = current_hashbin->next;
			hashbin_free(current_hashbin_holder);
		}
		++current_top_hashbin;
	}
	free(hashmap->hashbins);
	free(hashmap);
}

/* void othm_print_hashmap(struct othm_hashmap *hashmap) */
/* { */
/*   struct othm_hashbin **current_top_hashbin = hashmap->hashbins; */
/*   unsigned int hashbin_num = hashmap->hashbin_num; */
/*   unsigned int i; */
/*   for(i = 0; i != hashbin_num; ++i) { */
/*     struct othm_hashbin *current_hashbin = *current_top_hashbin; */
/*     while(current_hashbin != NULL) { */
/*       struct othm_hashbin *current_hashbin_holder = current_hashbin; */
/*       current_hashbin = current_hashbin->next; */
/*       printf("%s ", current_hashbin_holder->storage->prim_vals.string); */
/*     } */
/*     ++current_top_hashbin; */
/*     printf("\n"); */
/*   } */
/* } */

/* checks to see if hashbin uses key*/
static int check_key_hashbin(struct othm_hashbin *hashbin, char *key)
{
	return (strcmp(hashbin->key, key) == 0) ? 1 : 0;

}

/* Returns a hashbin given a key, else makes hashbin and returns that */
static void search_add_top_hashbin(struct othm_hashmap *hashmap,
				     struct othm_hashbin *top_hashbin,
				     char *key, void *storage)
{
	if (check_key_hashbin(top_hashbin, key))
		top_hashbin->storage = storage;
	else {
		struct othm_hashbin *next_hashbin;

		next_hashbin = top_hashbin;
		if (top_hashbin->next != NULL) {
			next_hashbin = top_hashbin->next;
			int loop = 1;
			while (loop) {
				if (check_key_hashbin(next_hashbin, key)) {
					next_hashbin->storage = storage;
					loop = 0;
				} else if (next_hashbin->next == NULL) {
					next_hashbin->next =
					    new_hashbin(key, storage);
					++(hashmap->entries_num);
					loop = 0;
				} else
					next_hashbin = next_hashbin->next;
			}
		} else
			top_hashbin->next = new_hashbin(key, storage);
	}
}

/* You should never need to use this so long name is okay... I hate myself for this
   Returns hashbin if it exists, else makes new hashbin and returns it */
static struct othm_hashbin *search_add_top_hashbin_old_key(struct
							       othm_hashmap
							       *hashmap,
							       struct
							       othm_hashbin
							       *top_hashbin,
							       char *key,
							       void *storage)
{
	if (check_key_hashbin(top_hashbin, key))
		top_hashbin->storage = storage;
	else {
		struct othm_hashbin *next_hashbin;

		next_hashbin = top_hashbin;
		if (top_hashbin->next != NULL) {
			next_hashbin = top_hashbin->next;
			int loop = 1;
			while (loop) {
				if (check_key_hashbin(next_hashbin, key)) {
					next_hashbin->storage = storage;
					loop = 0;
				} else if (next_hashbin->next == NULL) {
					next_hashbin->next =
					    copy_hashbin(key, storage);
					++(hashmap->entries_num);
					loop = 0;
				} else
					next_hashbin = next_hashbin->next;
			}
		} else {
			top_hashbin->next = new_hashbin(key, storage);
		}
	}
}

/* Adds an element to the hashmap */
void othm_hashmap_add(struct othm_hashmap *hashmap,
			char *key, void *storage)
{
	struct othm_hashbin *top_hashbin;
	unsigned int row;


	if (hashmap->entries_num / hashmap->hashbin_num >=
	    DEFAULT_ENTRIES_TO_HASHBINS)
		rehash(hashmap);
	row = MurmurHash2(key, strlen(key), DEFAULT_HASH_SEED)
		% hashmap->hashbin_num;
        top_hashbin = hashmap->hashbins[row];
	if (top_hashbin == NULL) {
		hashmap->hashbins[row] = new_hashbin(key, storage);
		++(hashmap->entries_num);
		return;
	}

	search_add_top_hashbin(hashmap, top_hashbin, key, storage);
}

/* Adds an element to the hashmap without allocating a new key. Used in rehashes */
static void readd_to_hashmap(struct othm_hashmap *hashmap,
			       char *key, void *storage)
{
	struct othm_hashbin *top_hashbin;
	unsigned int row;

	if (hashmap->entries_num / hashmap->hashbin_num >=
	    DEFAULT_ENTRIES_TO_HASHBINS)
		rehash(hashmap);
	row = MurmurHash2(key, strlen(key), DEFAULT_HASH_SEED)
		% hashmap->hashbin_num;
        top_hashbin = hashmap->hashbins[row];
	if (top_hashbin == NULL) {
		hashmap->hashbins[row] = copy_hashbin(key, storage);
		++(hashmap->entries_num);
		return;
	}

	search_add_top_hashbin_old_key(hashmap, top_hashbin, key,
					 storage);
}

/* Gets an element from a hashmap */
void *othm_hashmap_get(struct othm_hashmap *hashmap, char *key)
{
	struct othm_hashbin *top_hashbin;
	struct othm_hashbin *next_hashbin;
	unsigned int row;

        row = MurmurHash2(key, strlen(key), DEFAULT_HASH_SEED)
		% hashmap->hashbin_num;

	top_hashbin = hashmap->hashbins[row];
	if (top_hashbin == NULL)
		return NULL;
	if (check_key_hashbin(top_hashbin, key))
		return top_hashbin->storage;

        next_hashbin = top_hashbin->next;
	while (next_hashbin != NULL) {
		if (check_key_hashbin(next_hashbin, key))
			return next_hashbin->storage;
		next_hashbin = next_hashbin->next;
	}
	getchar();

	return NULL;
}

/* Rehashes the hashmap */
static void rehash(struct othm_hashmap *hashmap)
{
	struct othm_hashbin **old_hashbins;
	struct othm_hashbin **current_top_hashbin;
	struct othm_hashbin **rehashed_hashbins;
	unsigned int old_hashbin_num;
	unsigned int new_hashbin_num;
	unsigned int i;

	old_hashbin_num = hashmap->hashbin_num;
	old_hashbins = hashmap->hashbins;
	hashmap->entries_num = 0;
	++(hashmap->Primes_pointer);
        new_hashbin_num = *(hashmap->Primes_pointer);
	hashmap->hashbin_num = new_hashbin_num;
	rehashed_hashbins =
	    malloc(sizeof(struct othm_hashbin *) * new_hashbin_num);
	hashmap->hashbins = rehashed_hashbins;

	for (i = 0; i != new_hashbin_num; ++i) {
		*rehashed_hashbins = NULL;
		++rehashed_hashbins;
	}

        current_top_hashbin = old_hashbins;
	for (i = 0; i != old_hashbin_num; ++i) {
		struct othm_hashbin *current_hashbin;

		current_hashbin = *current_top_hashbin;
		while (current_hashbin != NULL) {
			readd_to_hashmap(hashmap,
					   current_hashbin->key,
					   current_hashbin->storage);
			struct othm_hashbin *current_hashbin_holder;

			current_hashbin_holder = current_hashbin;
			current_hashbin = current_hashbin->next;
			hashbin_free(current_hashbin_holder);
		}
		++current_top_hashbin;
	}
	free(old_hashbins);
}