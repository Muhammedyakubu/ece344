#include "request.h"
#include "server_thread.h"
#include "common.h"

struct server {
	int nr_threads;
	int max_requests;
	int max_cache_size;
	int exiting;
	/* add any other parameters you need */
	int *conn_buf;
	pthread_t *threads;
	int request_head;
	int request_tail;
	pthread_mutex_t mutex;
	pthread_cond_t prod_cond;
	pthread_cond_t cons_cond;	
};

static void
file_data_free(struct file_data *data);

/* Cache Implementation */
typedef struct CacheNode {
	struct file_data *data;
	struct CacheNode *next;
} CacheNode;

typedef struct Cache {
	CacheNode **array; // array of CacheNodes
	int array_size;	// initialized to max_cache_size/average_file_size (12kB)
	int max_cache_size;
	int current_cache_size;
	// should probably add a mutex lock here too?
} Cache;

typedef struct Node {
	char *file_name;	// could use static array instead
	struct Node *next;
} Node;

typedef struct Queue {
	struct Node *head;
	struct Node *tail;
	int size;
} Queue;

int hash_func(const char *word, int size) {
	unsigned long hash = 5381;
	int c;
	while ((c = *word++))
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	return hash % size;
}

CacheNode *linear_search(CacheNode *head, char *word){
	while (head) {
		if (strcmp(word, head->data->file_name) == 0)
			return head;
		head = head->next;
	}
	return NULL;
}

void
cache_init(Cache *c, int max_cache_size)
{	
	// initialize hash table
	int ht_size = max_cache_size/(4096 * 3);
	c->array_size = ht_size;
	c->max_cache_size = max_cache_size;
	c->current_cache_size = 0;
	c->array = (CacheNode **)calloc(ht_size, sizeof(CacheNode *));
	assert(c);
	return;
}

struct file_data *cache_lookup(Cache *c, char *file_name) {
	// if the word is empty, ignore
	if(strlen(file_name) == 0) return NULL;

	int k = hash_func(file_name, c->array_size);
	// get the linked list at index k of the hash table
	CacheNode *head = c->array[k];
	CacheNode *element = linear_search(head, file_name);
	return (element) ? element->data : NULL;
}

void cache_insert(Cache *c, struct file_data *file){
	// get the linked list at index k of the hash table
	int k = hash_func(file->file_name, c->array_size);
	CacheNode *head = c->array[k];

	// initialize the new node
	CacheNode *entry = malloc(sizeof(CacheNode));
	entry->data = file;
	entry->next = head;

	// insert entry at the head of wc[k]
	c->array[k] = entry;
	c->current_cache_size += file->file_size;
}

// returns the number of bytes evicted from the cache
int cache_evict(Cache *c, Queue *q, int num_bytes){
	if (c->max_cache_size < num_bytes) return 0;
	int evicted = 0;

	// evict from the head of the LRU queue
	while (evicted < num_bytes) {
		// get the file name at the head of the queue
		Node *node = q->head;
		q->head = q->head->next;
		q->size--;
		if (q->size == 0) q->tail = NULL;

		// remove from the cache
		int k = hash_func(node->file_name, c->array_size);
		CacheNode *curr = c->array[k];
		CacheNode *prev = NULL;
		while (curr) {
			if (strcmp(node->file_name, curr->data->file_name) == 0) {
				if (prev) prev->next = curr->next;
				else c->array[k] = curr->next;
				evicted += curr->data->file_size;
				c->current_cache_size -= curr->data->file_size;
				assert(c->current_cache_size >= 0);
				assert(evicted <= c->max_cache_size);
				free(curr);
				break;
			}
			prev = curr;
			curr = curr->next;
		}
		free(node);
	}

	return evicted;
}

void
list_destroy(CacheNode *head) {
	if(head == NULL) return;
	if(head->next == NULL) {
		file_data_free(head->data);
		free(head);
		return;
	}
	list_destroy(head->next);
	file_data_free(head->data);
	free(head);
	return;
}

void
cache_destroy(Cache *c)
{
	// loop through the entire array
	for(int i  = 0; i < c->array_size; ++i) {
		// free each node
		list_destroy(c->array[i]);
	}

	free(c);
};


/* LRU Implementation */
/* Use a FIFO Queue (linked list) to keep track of the order of the files */

void 
q_init(Queue *q) {
	q->head = NULL;
	q->tail = NULL;
	q->size = 0;
}

/* search & delete file_name from queue */
void 
q_remove(Queue *q, char *file_name) {
	Node *prev = NULL;
	Node *curr = q->head;
	while(curr) {
		if(strcmp(curr->file_name, file_name) == 0) {
			if(prev == NULL) {
				q->head = curr->next;
				if(q->head == NULL) q->tail = NULL;
			} else {
				prev->next = curr->next;
				if(prev->next == NULL) q->tail = prev;
			}
			free(curr->file_name);
			free(curr);
			q->size--;
			return;
		}
		prev = curr;
		curr = curr->next;
	}
}

/* add to the tail of the queue */
void 
q_add(Queue *q, char *file_name) {
	Node *new_node = malloc(sizeof(Node));
	new_node->file_name = malloc(strlen(file_name) + 1);
	strcpy(new_node->file_name, file_name);
	new_node->next = NULL;
	if(q->head == NULL) {
		q->head = new_node;
		q->tail = new_node;
	} else {
		q->tail->next = new_node;
		q->tail = new_node;
	}
	q->size++;
}

char *
q_remove_head(Queue *q) {
	if(q->head == NULL) return NULL;
	Node *head = q->head;
	q->head = head->next;
	if(q->head == NULL) q->tail = NULL;
	char *file_name = head->file_name;
	free(head);
	q->size--;
	return file_name;
}

/* Globals */

Cache FileCache;
Queue LRUQueue;

/* static functions */

/* initialize file data */
static struct file_data *
file_data_init(void)
{
	struct file_data *data;

	data = Malloc(sizeof(struct file_data));
	data->file_name = NULL;
	data->file_buf = NULL;
	data->file_size = 0;
	return data;
}

/* free all file data */
static void
file_data_free(struct file_data *data)
{
	free(data->file_name);
	free(data->file_buf);
	free(data);
}

static void
do_server_request(struct server *sv, int connfd)
{
	int ret;
	struct request *rq;
	struct file_data *data;

	data = file_data_init();

	/* fill data->file_name with name of the file being requested */
	rq = request_init(connfd, data);
	if (!rq) {
		file_data_free(data);
		return;
	}
	/* read file, 
	 * fills data->file_buf with the file contents,
	 * data->file_size with file size. */
	ret = request_readfile(rq);
	if (ret == 0) { /* couldn't read file */
		goto out;
	}
	/* send file to client */
	request_sendfile(rq);
out:
	request_destroy(rq);
	file_data_free(data);
}

static void *
do_server_thread(void *arg)
{
	struct server *sv = (struct server *)arg;
	int connfd;

	while (1) {
		pthread_mutex_lock(&sv->mutex);
		while (sv->request_head == sv->request_tail) {
			/* buffer is empty */
			if (sv->exiting) {
				pthread_mutex_unlock(&sv->mutex);
				goto out;
			}
			pthread_cond_wait(&sv->cons_cond, &sv->mutex);
		}
		/* get request from tail */
		connfd = sv->conn_buf[sv->request_tail];
		/* consume request */
		sv->conn_buf[sv->request_tail] = -1;
		sv->request_tail = (sv->request_tail + 1) % sv->max_requests;
		
		pthread_cond_signal(&sv->prod_cond);
		pthread_mutex_unlock(&sv->mutex);
		/* now serve request */
		do_server_request(sv, connfd);
	}
out:
	return NULL;
}

/* entry point functions */

struct server *
server_init(int nr_threads, int max_requests, int max_cache_size)
{
	struct server *sv;
	int i;

	sv = Malloc(sizeof(struct server));
	sv->nr_threads = nr_threads;
	/* we add 1 because we queue at most max_request - 1 requests */
	sv->max_requests = max_requests + 1;
	sv->max_cache_size = max_cache_size;
	sv->exiting = 0;

	/* Lab 4: create queue of max_request size when max_requests > 0 */
	sv->conn_buf = Malloc(sizeof(*sv->conn_buf) * sv->max_requests);
	for (i = 0; i < sv->max_requests; i++) {
		sv->conn_buf[i] = -1;
	}
	sv->request_head = 0;
	sv->request_tail = 0;

	/* Lab 5: init server cache and limit its size to max_cache_size */

	/* Lab 4: create worker threads when nr_threads > 0 */
	pthread_mutex_init(&sv->mutex, NULL);
	pthread_cond_init(&sv->prod_cond, NULL);
	pthread_cond_init(&sv->cons_cond, NULL);	
	sv->threads = Malloc(sizeof(pthread_t) * nr_threads);
	for (i = 0; i < nr_threads; i++) {
		SYS(pthread_create(&(sv->threads[i]), NULL, do_server_thread,
				   (void *)sv));
	}
	return sv;
}

void
server_request(struct server *sv, int connfd)
{
	if (sv->nr_threads == 0) { /* no worker threads */
		do_server_request(sv, connfd);
	} else {
		/*  Save the relevant info in a buffer and have one of the
		 *  worker threads do the work. */

		pthread_mutex_lock(&sv->mutex);
		while (((sv->request_head - sv->request_tail + sv->max_requests)
			% sv->max_requests) == (sv->max_requests - 1)) {
			/* buffer is full */
			pthread_cond_wait(&sv->prod_cond, &sv->mutex);
		}
		/* fill conn_buf with this request */
		assert(sv->conn_buf[sv->request_head] == -1);
		sv->conn_buf[sv->request_head] = connfd;
		sv->request_head = (sv->request_head + 1) % sv->max_requests;
		pthread_cond_signal(&sv->cons_cond);
		pthread_mutex_unlock(&sv->mutex);
	}
}

void
server_exit(struct server *sv)
{
	int i;
	/* when using one or more worker threads, use sv->exiting to indicate to
	 * these threads that the server is exiting. make sure to call
	 * pthread_join in this function so that the main server thread waits
	 * for all the worker threads to exit before exiting. */
	pthread_mutex_lock(&sv->mutex);
	sv->exiting = 1;
	pthread_cond_broadcast(&sv->cons_cond);
	pthread_mutex_unlock(&sv->mutex);
	for (i = 0; i < sv->nr_threads; i++) {
		pthread_join(sv->threads[i], NULL);
	}

	/* make sure to free any allocated resources */
	free(sv->conn_buf);
	free(sv->threads);
	free(sv);
}
