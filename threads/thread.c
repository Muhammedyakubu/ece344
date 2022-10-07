#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"

// enum for thread states
enum { 
	READY = 0,
	RUNNING,
	DEAD
};

/* This is the wait queue structure */
struct wait_queue {
	/* ... Fill this in Lab 3 ... */
};

/* This is the thread control block */
typedef struct thread {
	/* ... Fill this in ... */
	Tid id;	
	int setcontext_called;
	int state;
	struct ucontext_t context;
	void* stack_base;
} Thread;

/* queue data structure */
typedef struct node {
	Tid id;
	struct node *next;
} Node;

typedef struct queue {
	Node *head, *tail;
	int size;
} Queue;

/* GLOBALS */

Tid t_running;	// houses the currently running thread for quick access
Queue ready_queue = {NULL, NULL, 0};
Queue *ready_q = &ready_queue;
Thread *THREADS[THREAD_MAX_THREADS] = {NULL};	// initialize thread array to NULL

/* HELPERS */

// returns the top node from a queue
Tid q_pop(Queue *q);
// remove thread with Tid tid from the queue. 
// returns 1 if successful and 0 if there were no matching threads in the queue
int q_remove(Queue *q, Tid id);
// adds a new node with Tid tid to the END of Queue q
void q_add(Queue *q, Tid id);

// free space for dead/exited threads
void t_clean_dead_threads();
// checks if a thread id is valid
static inline int t_invalid(Tid id) {return (id < -2 || id >= THREAD_MAX_THREADS) || (id > 0 && ( THREADS[id] == NULL || THREADS[id]->state == DEAD));}

/* IMPLEMENTING HELPERS */
Tid q_pop(Queue *q){
	if(!q->head) return THREAD_NONE;
	Node *old_head = q->head;
	q->head = q->head->next;
	if(q->head == NULL) {
		q->tail = NULL;
	}
	Tid ret = old_head->id;
	free(old_head);
	q->size--;
	return ret;
	// could refactor this to use q_remove
}

int q_remove(Queue *q, Tid id){
	
	if(q->head == NULL) {
		return 0;
	}
	// if only one node 
	if(q->head->next == NULL) {
		if(q->head->id != id) return 0;

		free(q->head);
		q->head = q->tail = NULL;
		q->size--;
		return 1;
	}

	Node *cur = q->head;
	Node *prev = NULL;
	while(cur){
		assert(cur->id);
		if(cur->id == id) break;
		prev = cur;
		cur = cur->next;
	}

	if(cur == NULL) return 0;	// we didn't find Tid in q

	// if the wanted node is the first node
	if(cur == q->head && prev == NULL){
		q->head = q->head->next;
		free(cur);
		q->size--;
		return 1;
	}

	// if the wanted node is the last node
	if(cur == q->tail) {
		q->tail = prev; // if we're gonna remove the tail then move it one back
		q->tail->next = NULL; 
		free(cur);
		q->size--;
		return 1;
	}

	prev->next = cur->next;
	free(cur);
	q->size--;
	return 1;
}

void q_add(Queue *q, Tid id){
	if(q->tail) {
		Node *new = (Node *)malloc(sizeof(Node));
		new->id = id;
		new->next = NULL;
		q->tail->next = new;
		q->tail = new;
	} else {	// means the q is empty
		q->head = q->tail = (Node *)malloc(sizeof(Node));
		q->head->id = id;
		q->head->next = NULL;
	}
	q->size++;
}


/* THREAD LIBRARY FUNCTIONS */

void
thread_init(void)
{
	// allocate space for main thread (0)
	Thread *t = (Thread *)malloc(sizeof(Thread));
	t->id = 0;
	t->state = RUNNING;
	t->setcontext_called = 0;
	t->stack_base = NULL;
	// initialize thread context
	assert(getcontext(&t->context) == 0);

	t_running = t->id;
	// add it to threads array
	THREADS[t->id] = t;
}

Tid
thread_id()
{
	return t_running;
}

/* New thread starts by calling thread_stub. The arguments to thread_stub are
 * the thread_main() function, and one argument to the thread_main() function. 
 */
void
thread_stub(void (*thread_main)(void *), void *arg)
{
	THREADS[t_running]->setcontext_called = 0;
	thread_main(arg); // call thread_main() function with arg
	thread_exit(0);
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
	// check that we can make more threads
	Tid tid = THREAD_MAX_THREADS;
	for(int i = 0; i < THREAD_MAX_THREADS; ++i) {
		if (THREADS[i] == NULL || THREADS[i]->state == DEAD) {
			tid = i;
			break;
		}
	}

	// if tid was not reassigned that means that we can't make more threads
	if (tid == THREAD_MAX_THREADS) return THREAD_NOMORE;

	// create the thread:
	Thread *t = (Thread *)malloc(sizeof(Thread));
	void *stack = malloc(sizeof(THREAD_MIN_STACK));

	// 	allocate stack space 
	if (!t || !stack) {
		return THREAD_NOMEMORY;	
	}

	THREADS[tid] = t;

	t->id = tid;
	t->state = READY;
	t->stack_base = stack;
	t->setcontext_called = 0;
	assert(getcontext(&t->context) == 0);
	
	// 	set rip, rsp, rsi & rdi GREGS
	t->context.uc_mcontext.gregs[REG_RIP] = (greg_t) &thread_stub;
	t->context.uc_mcontext.gregs[REG_RDI] = (greg_t) fn;	// run function
	t->context.uc_mcontext.gregs[REG_RSI] = (greg_t) parg;
	t->context.uc_mcontext.gregs[REG_RSP] = (greg_t) (
		t->stack_base + THREAD_MIN_STACK - 8 - (greg_t)stack%16
		); // make sure sp is aligned to 16 bits

	// add new thread to ready queue
	q_add(ready_q, t->id);
	return t->id;
}

Tid
thread_yield(Tid want_tid)
{
	if(t_invalid(want_tid)) {
		return THREAD_INVALID;
	}

	// get the wanted thread
	if (want_tid == THREAD_ANY) {
		want_tid = q_pop(ready_q);
		if(want_tid == THREAD_NONE) return THREAD_NONE;
	} 
	if (want_tid == THREAD_SELF || want_tid == t_running) {
		return t_running;
	}

	// save the state of the running thread
	THREADS[t_running]->state = READY;
	assert(getcontext(&THREADS[t_running]->context) == 0);

	if(THREADS[t_running]->setcontext_called){
		THREADS[t_running]->setcontext_called = 0;
		return want_tid;
	}

	// add the current running queue to the end of the ready_q
	q_add(ready_q, t_running);
	// remove the now running thead from the ready_q
	if(!q_remove(ready_q, want_tid))
		printf("DEBUG: thread %d was not in the ready_q\n", want_tid);

	t_running = want_tid;
	THREADS[t_running]->setcontext_called = 1;
	THREADS[t_running]->state = RUNNING;
	// context switch to thread want_tid
	setcontext(&THREADS[t_running]->context);

	return THREAD_FAILED;
}

void
thread_exit(int exit_code)
{
	TBD();
}

Tid
thread_kill(Tid tid)
{
	TBD();
	return THREAD_FAILED;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue *
wait_queue_create()
{
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	TBD();

	return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
	TBD();
	free(wq);
}

Tid
thread_sleep(struct wait_queue *queue)
{
	TBD();
	return THREAD_FAILED;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
	TBD();
	return 0;
}

/* suspend current thread until Thread tid exits */
Tid
thread_wait(Tid tid, int *exit_code)
{
	TBD();
	return 0;
}

struct lock {
	/* ... Fill this in ... */
};

struct lock *
lock_create()
{
	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	TBD();

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

	TBD();

	free(lock);
}

void
lock_acquire(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

void
lock_release(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

struct cv {
	/* ... Fill this in ... */
};

struct cv *
cv_create()
{
	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

	TBD();

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

	TBD();

	free(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}
