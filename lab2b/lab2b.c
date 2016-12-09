// creates and initializes (with random keys) the required number (threads * iterations) of list elements.  Note that we do this before creating the threads so that this time is not included in our start-to-finish measurement.
// rand() and malloc() a large array
// O(n) insert and lookup
// 3 char key

// 2B.1A
// graph drop first because of overhead in thread create
// increase later because O(n) operation

// 2B.1B
// how to make the graph the same as before, monotonic decrease
// divide by size of list

// 2B.2A
// 100 iteration 20 threads spin 20s ~ 1s due to garbage line
// 1. list critical section larger than add() critical section.
// 2. lock is held much longer in list than add().
// 3. probability of conflict is much higher for the same number of threads
// 4. More conflict means more blocked threads, more overhead, less parallelism.

#include <pthread.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "SortedList.h"

#define KEY_SZ 3
typedef unsigned long long ull;

// global arguments
ull n_thread = 1;
ull n_iteration = 1;

// the list structures
SortedList_t head;
SortedListElement_t *elements;

// lock types
enum LockType {
	LOCK_NULL = 0,
	LOCK_MUTEX,
	LOCK_SPIN,
};
int opt_sync = LOCK_NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int spinlock = 0;

void lock() {
	switch (opt_sync) {
	case LOCK_MUTEX:
		pthread_mutex_lock(&mutex);
		break;
	case LOCK_SPIN:
		while (__sync_lock_test_and_set(&spinlock, 1));
		break;
	case LOCK_NULL:
	default:
		break;
	}
}

void unlock() {
	switch (opt_sync) {
	case LOCK_MUTEX:
		pthread_mutex_unlock(&mutex);
		break;
	case LOCK_SPIN:
		__sync_lock_release(&spinlock);
		break;
	case LOCK_NULL:
	default:
		break;
	}
}

void parse(int argc, char *argv[]) {
  struct option longOptions[] = 
  {
    {"threads", required_argument, NULL, 't'},
    {"iterations", required_argument, NULL, 'i'},
    {"yield", required_argument, NULL, 'y'},
    {"sync", required_argument, NULL, 's'},
    {NULL, 0, NULL, 0},
  };

  int opt, i;
  while ( (opt = getopt_long(argc, argv, "t:i:y:s:", longOptions, NULL)) != -1 ) {
    switch (opt) 
    {
    case 't':
      n_thread = atoi(optarg);
      break;

    case 'i':
      n_iteration = atoi(optarg);
      break;

    case 'y':
      for (i = 0; i < strlen(optarg); i++)
      	switch (optarg[i]) {
      	case 'i':
      		opt_yield |= INSERT_YIELD;
      		break;
      	case 'd':
      		opt_yield |= DELETE_YIELD;
      		break;
      	case 's':
      		opt_yield |= SEARCH_YIELD;
      		break;
      	default:
      		break;
      	}
      break;

    case 's':
      switch (optarg[0]) {
      	case 'm':
      	  opt_sync = LOCK_MUTEX;
      	  break;
      	case 's':
      	  opt_sync = LOCK_SPIN;
      	  break;
      	default:
      	  break;
      }
      break;

    default:
      break;
    }
  }
}

void rand_key(char *key, int size) {
	static char s[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int i;

	for (i = 0; i < size; i++)
		key[i] = s[rand() % (sizeof(s)-1)];

	key[size] = '\0';
}

void *worker(void *arg) {
	SortedListElement_t *e = (SortedListElement_t *) arg;
	SortedListElement_t *node;
	int i;

	lock();
	// insert elements in list
	for (i = 0; i < n_iteration; i++) {
		SortedList_insert(&head, &e[i]);
	}

	// get the list length
	SortedList_length(&head);

	// looks up and deletes each of the keys
	for (i = 0; i < n_iteration; i++) {
		node = SortedList_lookup(&head, e[i].key);
		SortedList_delete(node);
	}
	unlock();

	return NULL;
}

int main(int argc, char *argv[]) {
	pthread_t *threads;
	struct timespec tps, tpe;
	ull i, diff;

	// read arguments
	parse(argc, argv);

	// alloc space for thread structure
	threads = malloc(n_thread * sizeof(pthread_t));

	// create and allocate list elements
	head.prev = &head;
	head.next = &head;
	elements = malloc(n_thread * n_iteration * sizeof(SortedListElement_t));
	for (i = 0; i < n_thread * n_iteration; i++) {
		char *key = malloc(KEY_SZ + 1);
		rand_key(key, KEY_SZ);
		elements[i].key = key;
	}

	// time start
	clock_gettime(CLOCK_MONOTONIC, &tps);

	// create threads
	for (i = 0; i < n_thread; i++)
		pthread_create(&threads[i], NULL, worker, &elements[i * n_iteration]);

	// wait for threads
	for (i = 0; i < n_thread; i++)
		pthread_join(threads[i], NULL);

	// time end
	clock_gettime(CLOCK_MONOTONIC, &tpe);
	diff = (tpe.tv_sec - tps.tv_sec) * 1000000000LL + (tpe.tv_nsec - tps.tv_nsec);

	// print stats
	printf( 
		"%llu threads x %llu iterations x (insert + lookup/delete) x (%llu avg length)= %llu operations\n", 
		n_thread, 
		n_iteration,
		n_iteration/2,
		n_thread * n_iteration * n_iteration);
	printf("elapsed time: %lluns\n", diff);
	printf("per operation: %lfns\n", (double)diff/(n_thread * n_iteration * n_iteration));

	// check len
	int len = SortedList_length(&head);
	if (len != 0)
		fprintf(stderr, "ERROR: final length = %d\n", len);

	return len != 0;
}