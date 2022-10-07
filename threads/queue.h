

/* queue data structure */
typedef struct node {
	Tid id;
	struct node *next;
} Node;

typedef struct queue {
	Node *head, *tail;
	int size;
} Queue;

// returns the top node from a queue
Tid q_pop(Queue *q);

// remove thread with Tid tid from the queue. 
// returns 1 if successful and 0 if there were no matching threads in the queue
int q_remove(Queue *q, Tid id);
// adds a new node with Tid tid to the END of Queue q
void q_add(Queue *q, Tid id);
void q_print(Queue *q);