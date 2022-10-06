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
Queue ready_q = {NULL, NULL, 0};
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
inline int t_valid(Tid id) {return (id < 0 || id > THREAD_MAX_THREADS);}

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
        thread_main(arg); // call thread_main() function with arg
        thread_exit(0);
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
	// create the thread:
	// 	allocate stack pointer, 
	// 	set program counter
	// 	call fn with pargs 
	

	// add new thread to ready queue

	// return tid

	TBD();
	return THREAD_FAILED;
}

Tid
thread_yield(Tid want_tid)
{
	// save the state of the running thread
	// add the current running queue to the end of the ready_q
	// context switch to thread want_tid
	// return running thread tid

	TBD();
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
