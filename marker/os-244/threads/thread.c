#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdbool.h>
#include "thread.h"
#include "interrupt.h"

// #define DEBUG_USE_VALGRIND
int debug = 0;

#ifdef DEBUG_USE_VALGRIND
#include <valgrind.h>
#endif

#define str(x) #x

// enum for thread states
enum { 
	READY = 0,
	RUNNING,
	KILLED,
	DEAD,
	SLEEPING,
	WAITING,
};

/* queue data structure */
typedef struct node {
	Tid id;
	struct node *next;
} Node;

/* This is the wait queue structure */
typedef struct wait_queue {
	int head, tail;
    int size;
    Tid array[THREAD_MAX_THREADS];
} Queue;

/* This is the thread control block */
typedef struct thread {
	Tid id;	
	int state;
	struct ucontext_t context;
	void* stack_base;
	Queue *cur_q;	// pointer to the queue that the thread is currently in
	Queue *wait_q;	// queue of threads waiting on this thread
} Thread;

// returns the top node from a queue
Tid q_pop(Queue *q);
// remove thread with Tid tid from the queue. 
// returns 1 if successful and 0 if there were no matching threads in the queue
bool q_remove(Queue *q, Tid id);
// adds a new node with Tid tid to the END of Queue q
void q_add(Queue *q, Tid id);
void q_print(Queue *q);


/* GLOBALS */

Tid t_running;	// houses the currently running thread for quick access
Queue ready_queue = {0, 0, 0, {THREAD_NONE}};
Queue *ready_q = &ready_queue;
Thread *THREADS[THREAD_MAX_THREADS] = {NULL};	// initialize thread array to NULL
int THREAD_EXIT_STATUS[THREAD_MAX_THREADS] = {THREAD_INVALID};	// initialize exit status array to 0

/* HELPERS */

// free space for dead/exited threads
void t_clean_dead_threads();
void thread_awaken(Tid tid);

static inline bool t_in_range(Tid tid) {
	return tid >= 0 && tid < THREAD_MAX_THREADS;
}

// checks if a thread id is valid
static inline int t_invalid(Tid id) {
	return (id < -2 || id >= THREAD_MAX_THREADS) || (id > 0 && ( THREADS[id] == NULL || THREADS[id]->state == DEAD));
}

/* IMPLEMENTING HELPERS */

void q_init(Queue *q) {
	q->size = 0;
	q->head = 0;
	q->tail = 0;
	for (int i = 0; i < THREAD_MAX_THREADS; i++) {
		q->array[i] = THREAD_NONE;
	}
}

Tid q_pop(Queue *q) {
    if (q->size == 0) {
        return THREAD_NONE;
    }
    Tid id = q->array[q->head];
	q->array[q->head] = THREAD_NONE;
    q->head = (q->head + 1) % THREAD_MAX_THREADS;
    q->size--;
	if (debug) {
		printf("%s(): ", __func__);
		q_print(q);
	}
    return id;
}

bool q_remove(Queue *q, Tid id) {
    if (q == NULL || q->size == 0) {
        return false;
    }
    int i = q->head;
    while (i != q->tail) {
        if (q->array[i] == id) {
            q->array[i] = THREAD_NONE;
            q->size--;

			// shift all elements after id to the left
			int j = i;
			while (j != q->tail) {
				q->array[j] = q->array[(j + 1) % THREAD_MAX_THREADS];
				j = (j + 1) % THREAD_MAX_THREADS;
			}
			q->tail = (q->tail - 1) % THREAD_MAX_THREADS;
			q->array[q->tail] = THREAD_NONE;

			if (debug) {
				printf("%s(%d): ", __func__, id);
				q_print(q);
			}
			
            return true;
        }
        i = (i + 1) % THREAD_MAX_THREADS;
    }

	return false;
}

void q_add(Queue *q, Tid id) {
    if (q->size == THREAD_MAX_THREADS) {
        return;
    }
    q->array[q->tail] = id;
    q->tail = (q->tail + 1) % THREAD_MAX_THREADS;
    q->size++;
	if (debug) {
		printf("%s(%d): ", __func__, id);
		q_print(q);
	}
}

void q_print(Queue *q) {
    if (q->size == 0) {
        printf("Queue is empty\n");
        return;
    }
    int i = q->head;
    while (i != q->tail) {
        printf("%d->", q->array[i]);
        i = (i + 1) % THREAD_MAX_THREADS;
    }
	printf("NULL\n");
}


void t_clean_dead_threads(){
	for (int i = 0; i < THREAD_MAX_THREADS; i++) {
		if (THREADS[i] != NULL && THREADS[i]->state == DEAD && i != t_running) {
			free(THREADS[i]->stack_base);
			wait_queue_destroy(THREADS[i]->wait_q);
			free(THREADS[i]);
			THREADS[i] = NULL;
		}
	}
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
	t->cur_q = t->wait_q = NULL;
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
	interrupts_on();
	thread_main(arg); // call thread_main() function with arg
	thread_exit(0);
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
	int signals_enabled = interrupts_off();

	// free dead threads
	t_clean_dead_threads();

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
		interrupts_set(signals_enabled);
		return THREAD_NOMORE;
	}

	// create the thread && allocate stack space:
	Thread *t = (Thread *)malloc(sizeof(Thread));
	void *stack = malloc(THREAD_MIN_STACK+24);

	if (!t || !stack) {
		interrupts_set(signals_enabled);
		return THREAD_NOMEMORY;	
	}

	THREADS[tid] = t;

	t->id = tid;
	t->state = READY;
	t->stack_base = stack;
	t->cur_q = ready_q;
	t->wait_q = NULL;
	assert(getcontext(&t->context) == 0);
	
	// 	set rip, rsp, rsi & rdi GREGS
	t->context.uc_mcontext.gregs[REG_RIP] = (greg_t) &thread_stub;
	t->context.uc_mcontext.gregs[REG_RDI] = (greg_t) fn;	// run function
	t->context.uc_mcontext.gregs[REG_RSI] = (greg_t) parg;
	t->context.uc_mcontext.gregs[REG_RSP] = (greg_t) (
		t->stack_base + THREAD_MIN_STACK - 8 - (greg_t)stack%16
	); 

#ifdef DEBUG_USE_VALGRIND
	unsigned valgrind_register_retval = VALGRIND_STACK_REGISTER(t->context.uc_mcontext.gregs[REG_RSP] - THREAD_MIN_STACK, t->context.uc_mcontext.gregs[REG_RSP]);
	assert(valgrind_register_retval);
#endif


	// add new thread to ready queue
	q_add(ready_q, t->id);
	
	interrupts_set(signals_enabled);
	return t->id;
}

Tid
thread_yield(Tid want_tid)
{
	int signals_enabled = interrupts_off();
	if(t_invalid(want_tid)) {
		interrupts_set(signals_enabled);
		return THREAD_INVALID;
	}
	
	if (debug) printf("Thread %d yields -> %d\n", thread_id(), want_tid);

	// get the wanted thread
	if (want_tid == THREAD_ANY) {
		do {
			want_tid = q_pop(ready_q);
			if (want_tid == THREAD_NONE) {
				interrupts_set(signals_enabled);
				return THREAD_NONE;
			}
		} while (THREADS[want_tid] == NULL || THREADS[want_tid]->state == DEAD);
	} 
	if (want_tid == THREAD_SELF || want_tid == t_running) {
		if (debug) printf("Thread %d returning from yield to self\n", thread_id());
		q_remove(ready_q, t_running);

		interrupts_set(signals_enabled);
		return t_running;
	}

	// volatile forces this variable unto the caller's stack
	volatile int setcontext_called = 0;

	// change states for VALID caller & save its context
	if (THREADS[t_running]->state == RUNNING) {
		THREADS[t_running]->state = READY;
		q_add(ready_q, t_running);
		t_clean_dead_threads();
		// could free dead threads here?
	}
	assert(getcontext(&THREADS[t_running]->context) == 0);

	if (setcontext_called){
		if (debug) printf("Thread %d returning from yield to %d\n", thread_id(), want_tid);
		
		interrupts_set(signals_enabled);
		return want_tid;
	}

	// sets up the changes for the wanted thread
	q_remove(ready_q, want_tid);	
	t_running = want_tid;
	THREADS[t_running]->state = RUNNING;
	setcontext_called = 1;
	assert(setcontext(&THREADS[t_running]->context) >= 0);

	// should never get here
	return THREAD_FAILED;
}

void
thread_exit(int exit_code)
{
	int signals_enabled = interrupts_off();
	t_clean_dead_threads();

	THREADS[t_running]->state = DEAD;
	q_remove(ready_q, t_running);

	// store exit code
	THREAD_EXIT_STATUS[t_running] = exit_code;

	// wake up all threads waiting on this thread
	if (THREADS[t_running]->wait_q) {
		while (THREADS[t_running]->wait_q->size) {
			Tid tid = q_pop(THREADS[t_running]->wait_q);
			q_add(ready_q, tid);
			// thread_awaken(tid);
		}
	}

	if (ready_q->size == 0) {
		exit(exit_code);
	}

	// reenable interrupts before yielding
	interrupts_set(signals_enabled);
	thread_yield(THREAD_ANY);
}

Tid
thread_kill(Tid tid)
{
	int signals_enabled = interrupts_off();

	t_clean_dead_threads();
	if (t_invalid(tid) || tid == t_running) {
		interrupts_set(signals_enabled);
		return THREAD_INVALID;
	}

	if (THREADS[tid]->state == SLEEPING) thread_awaken(tid);
	THREADS[tid]->state = KILLED;

	// set thread to run thread_exit with code 9
	THREADS[tid]->context.uc_mcontext.gregs[REG_RIP] = (greg_t) &thread_exit;
	THREADS[tid]->context.uc_mcontext.gregs[REG_RDI] = (greg_t) 9;

	interrupts_set(signals_enabled);
	return tid;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue *
wait_queue_create()
{
	int signals_enabled = interrupts_off();
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	wq->head = wq->tail = wq->size = 0;
	for (int i = 0; i < THREAD_MAX_THREADS; i++) {
		wq->array[i] = THREAD_NONE;
	}

	interrupts_set(signals_enabled);
	return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
	int signals_enabled = interrupts_off();
	if (wq) {
		assert(wq->size == 0);
		free(wq);
	}

	interrupts_set(signals_enabled);
}

Tid
thread_sleep(struct wait_queue *queue)
{
	int signals_enabled = interrupts_off();

	// check validitiy of wait queue
	if(queue == NULL) {
		interrupts_set(signals_enabled);
		return THREAD_INVALID;
	}
	if (ready_q->size == 0) {
		interrupts_set(signals_enabled);
		return THREAD_NONE;
	}
	
	THREADS[thread_id()]->state = SLEEPING;
	THREADS[thread_id()]->cur_q = queue;
	q_add(queue, thread_id());

	// yield will return THREAD_NONE if there are no threads in the ready queue
	interrupts_set(signals_enabled);
	return thread_yield(THREAD_ANY);
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
	int signals_enabled = interrupts_off();
	if (queue == NULL || queue->size == 0) {
		interrupts_set(signals_enabled);
		return 0;
	}

	int t_count;

	if (all) {
		t_count = all = queue->size;
	} else {
		t_count = all = 1;
	}

	while(all) {
		Tid tid = q_pop(queue);
		THREADS[tid]->state = READY;
		q_add(ready_q, tid);
		all--;
	}

	interrupts_set(signals_enabled);
	return t_count;
}

/* suspend current thread until Thread tid exits */
Tid
thread_wait(Tid tid, int *exit_code)
{
	int signals_enabled = interrupts_off();

	if (tid < 0 || tid >= THREAD_MAX_THREADS || THREADS[tid] == NULL || tid == t_running) {
		interrupts_set(signals_enabled);
		return THREAD_INVALID;
	}

	Tid ret;

	if (THREADS[tid]->wait_q == NULL) {
		THREADS[tid]->wait_q = wait_queue_create();
		ret = tid;
	} else {
		ret = THREAD_INVALID;
	}

	thread_sleep(THREADS[tid]->wait_q);

	if (ret != THREAD_INVALID && exit_code != NULL) {
		*exit_code = THREAD_EXIT_STATUS[tid];
		THREAD_EXIT_STATUS[tid] = THREAD_INVALID;
	}

	interrupts_set(signals_enabled);
	return ret;
}

void
thread_awaken(Tid tid) {
	int signals_enabled = interrupts_off();

	if (t_invalid(tid) || THREADS[tid]->state != SLEEPING) {
		interrupts_set(signals_enabled);
		return;
	}

	THREADS[tid]->state = READY;
	q_remove(THREADS[tid]->cur_q, tid);
	q_add(ready_q, tid);

	interrupts_set(signals_enabled);
}

struct lock {
	/* ... Fill this in ... */
	Tid owner;
	Queue *queue;
};

struct lock *
lock_create()
{
	int signals_enabled = interrupts_off();
	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	lock->owner = THREAD_NONE;
	lock->queue = wait_queue_create();

	interrupts_set(signals_enabled);
	return lock;
}

void
lock_destroy(struct lock *lock)
{
	int signals_enabled = interrupts_off();
	assert(lock != NULL);

	assert(lock->owner == THREAD_NONE);
	assert(lock->queue->size == 0);

	wait_queue_destroy(lock->queue);
	free(lock);
	interrupts_set(signals_enabled);
}

void
lock_acquire(struct lock *lock)
{
	int signals_enabled = interrupts_off();
	assert(lock != NULL);
	assert(lock->owner != thread_id());

	while (lock->owner != THREAD_NONE) {
		thread_sleep(lock->queue);
	}
	 
	lock->owner = thread_id();

	interrupts_set(signals_enabled);
	return;
}

void
lock_release(struct lock *lock)
{
	int signals_enabled = interrupts_off();
	assert(lock != NULL);
	assert(lock->owner == thread_id());

	if (lock->owner == thread_id()) {
		lock->owner = THREAD_NONE;
		thread_wakeup(lock->queue, 1);
	}

	interrupts_set(signals_enabled);
}

struct cv {
	/* ... Fill this in ... */
	Queue *queue;
};

struct cv *
cv_create()
{
	int signals_enabled = interrupts_off();
	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

	cv->queue = wait_queue_create();

	interrupts_set(signals_enabled);
	return cv;
}

void
cv_destroy(struct cv *cv)
{
	int signals_enabled = interrupts_off();
	assert(cv != NULL);
	assert(cv->queue->size == 0);

	wait_queue_destroy(cv->queue);
	free(cv);
	interrupts_set(signals_enabled);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	int signals_enabled = interrupts_off();
	assert(cv != NULL);
	assert(lock != NULL);

	assert(lock->owner == thread_id());
	lock_release(lock);
	thread_sleep(cv->queue);
	lock_acquire(lock);
	interrupts_set(signals_enabled);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	int signals_enabled = interrupts_off();
	assert(cv != NULL);
	assert(lock != NULL);

	assert(lock->owner == thread_id());
	thread_wakeup(cv->queue, 0);
	interrupts_set(signals_enabled);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	int signals_enabled = interrupts_off();
	assert(cv != NULL);
	assert(lock != NULL);

	assert(lock->owner == thread_id());
	thread_wakeup(cv->queue, 1);
	interrupts_set(signals_enabled);
}
