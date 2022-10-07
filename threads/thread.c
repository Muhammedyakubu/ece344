#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdbool.h>
#include "thread.h"
#include "interrupt.h"

#define DEBUG_USE_VALGRIND

#ifdef DEBUG_USE_VALGRIND
#include <valgrind.h>
#endif

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
	Tid id;	
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
bool q_remove(Queue *q, Tid id);

// adds a new node with Tid tid to the END of Queue q
void q_add(Queue *q, Tid id);

void q_print(Queue *q);

// free space for dead/exited threads
void t_clean_dead_threads();

// checks if a thread id is valid
static inline int t_invalid(Tid id) {
	return (id < -2 || id >= THREAD_MAX_THREADS) || (id > 0 && ( THREADS[id] == NULL || THREADS[id]->state == DEAD));
}

/* IMPLEMENTING HELPERS */

Tid q_pop(Queue *q){
	if (q->head == NULL) {
		assert(q->tail == NULL);
		return THREAD_NONE;
	}

	Node *old_head = q->head;
	// doesn't matter if head is null
	q->head = q->head->next;

	Tid ret = old_head->id;
	free(old_head);
	q->size--;
	return ret;
}

bool q_remove(Queue *q, Tid id){
	if(!q->head) return 0;

	Node *prev = NULL, *cur = q->head;

	while(cur && cur->id != id) {
		prev = cur;
		cur = cur->next;
	}

	// we don't find the id in the list
	if (cur == NULL) return 0;

	if (prev == NULL) {
		// remove head, return a truthy value
		return q_pop(q) + 1;
	}

	if (cur == q->tail) {
		q->tail = prev;
	}
	prev->next = cur->next;
	free(cur);

	q->size--;
	return 1;
}

void q_add(Queue *q, Tid id){
	assert(id >= 0); // having some weird memory corruption bugs 

	q->size++;

	Node *new = (Node *)malloc(sizeof(Node));
	assert(new);
	new->id = id;
	new->next = NULL;

	if (q->head == NULL) {
		q->head = q->tail = new;
	}
	else {
		assert(q->tail && q->head); // these are either both present or absent
		q->tail->next = new;
		q->tail = new;
		return;
	}

}

void q_print(Queue *q){
	Node *cur = q->head;
	int hitcount = 0;
	while(cur){
		printf("%p:%d->", cur, cur->id);
		cur = cur->next;
		hitcount++;
	}
	printf("NULL\n");
	return;
}


/* THREAD LIBRARY FUNCTIONS */

void
thread_init(void)
{
	// allocate space for main thread (0)
	Thread *t = (Thread *)malloc(sizeof(Thread));
	t->id = 0;
	t->state = RUNNING;
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
	if (tid == THREAD_MAX_THREADS) {
		return THREAD_NOMORE;
	}

	// create the thread:
	Thread *t = (Thread *)malloc(sizeof(Thread));
	// 	allocate stack space 
	void *stack = malloc(sizeof(THREAD_MIN_STACK));

	if (!t || !stack) {
		return THREAD_NOMEMORY;	
	}

	THREADS[tid] = t;

	t->id = tid;
	t->state = READY;
	t->stack_base = stack;
	assert(getcontext(&t->context) == 0);

	// make sure sp is aligned to 16 bits
	void *top_stack = t->stack_base + THREAD_MIN_STACK;
	int offset = (long long int) top_stack % 16;
	if (offset < 8) {
		offset = 8 + offset;
	} else {
		offset = offset - 8;
	}
	void* rsp = top_stack - offset;
	
	// 	set rip, rsp, rsi & rdi GREGS
	t->context.uc_mcontext.gregs[REG_RIP] = (greg_t) &thread_stub;
	t->context.uc_mcontext.gregs[REG_RDI] = (greg_t) fn;	// run function
	t->context.uc_mcontext.gregs[REG_RSI] = (greg_t) parg;
	t->context.uc_mcontext.gregs[REG_RSP] = (greg_t) (rsp); 

#ifdef DEBUG_USE_VALGRIND
	unsigned valgrind_register_retval = VALGRIND_STACK_REGISTER(rsp - THREAD_MIN_STACK, rsp);
	assert(valgrind_register_retval);
#endif

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
		if (want_tid == THREAD_NONE) return THREAD_NONE;
	} 
	if (want_tid == THREAD_SELF || want_tid == t_running) {
		q_remove(ready_q, want_tid);
		return t_running;
	}

	// volatile forces this variable unto the caller's stack
	volatile int setcontext_called = 0;

	// change states for caller & save its context
	THREADS[t_running]->state = READY;
	q_add(ready_q, t_running);
	assert(getcontext(&THREADS[t_running]->context) == 0);

	if (setcontext_called){
		return want_tid;
	}

	// sets up the changes for the wanted thread
	q_remove(ready_q, want_tid);	
	THREADS[t_running]->state = RUNNING;
	t_running = want_tid;
	setcontext_called = 1;
	assert(setcontext(&THREADS[t_running]->context) >= 0);

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
