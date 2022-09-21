#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "wc.h"
#include <string.h>

typedef struct Node {
	char *word;
	int count;
	struct Node *next;
} Node;

struct wc {
	/* you can define this struct to have whatever fields you want. */
	Node **array; // array of linkedlistNodes
	int size;
	
};

int hash_func(const char *word, int size) {
	unsigned long hash = 5381;
	int c;
	while ((c = *word++))
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	return hash % size;
}



Node * linear_search(Node *head, char *word){
	while (head) {
		if (strcmp(word, head->word))
			return head;
		head = head->next;
	}
	return NULL;
}

struct wc *
wc_init(char *word_array, long size)
{	
	// initialize hash table
	struct wc *wc;
	int ht_size = size/2;


	wc = (struct wc *)malloc(sizeof(struct wc));
	wc->array = (Node **)malloc(sizeof(Node) * ht_size);
	assert(wc);

	// parse words and insert into hash table
	char *wa_copy, *token;

	printf("before strdup");

	wa_copy = strdup(word_array);

	printf("after, boutta start parsing");
 
	while( (token = strsep(&wa_copy," \n")) != NULL ) {
		printf("token is: %s", token);
		//=>Check if word exists in hashtable

		int k = hash_func(token, ht_size);
		// get the linked list at index k of the hash table
		Node *head = wc->array[k];
		Node *element = linear_search(head, token);
		if (element) {
			element->count += 1;
		} else {

			// initialize the new node
			Node *entry = malloc(sizeof(Node));
			entry->count = 1;
			strcpy(entry->word, token);
			entry->next = head;

			// insert entry at the head of wc[k]
			wc->array[k] = entry;
		}

	}

	return wc;
}

void
wc_output(struct wc *wc)
{
	for(int i = 0; i<wc->size; i++){
		Node *ptr = wc->array[i];

		while(ptr) {
			printf("%s:%d\n", ptr->word, ptr->count);
		}
	}
	
}

void
wc_destroy(struct wc *wc)
{
	TBD();
	free(wc);
};;