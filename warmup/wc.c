#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "wc.h"

struct wc {
	/* you can define this struct to have whatever fields you want. */
};

int hash_func(char *word) {
	
}

int wc_insert(struct wc *, char *word){

}

struct wc *
wc_init(char *word_array, long size)
{	
	// initialize hash table
	struct wc *wc;

	wc = (struct wc *)malloc(sizeof(struct wc));
	assert(wc);

	// parse words and insert into hash table
    char* token;
    char* rest = word_array;
 
    while ((token = strtok_r(rest, " ", &rest)))
        wc_insert(token);

	// TBD();

	return wc;
}

void
wc_output(struct wc *wc)
{
	TBD();
}

void
wc_destroy(struct wc *wc)
{
	TBD();
	free(wc);
}
