// #define _GRAPH
#include <pthread.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include "SortedList.h"

#define KEY_SZ 3
typedef unsigned long long ull;

// global arguments
ull n_thread = 1;
ull n_iteration = 1;
ull n_list = 1;

// the list structures
SortedList_t *heads;
SortedListElement_t *elements;

// lock types
enum LockType {
	LOCK_NULL = 0,
	LOCK_MUTEX,
	LOCK_SPIN,
};
int opt_sync = LOCK_NULL;
pthread_mutex_t *mutex;
int *spinlock;

void lock(int i) {
	switch (opt_sync) {
	case LOCK_MUTEX:
		pthread_mutex_lock(&mutex[i]);
		break;
	case LOCK_SPIN:
		while (__sync_lock_test_and_set(&spinlock[i], 1));
		break;
	case LOCK_NULL:
	default:
		break;
	}
}

void unlock(int i) {
	switch (opt_sync) {
	case LOCK_MUTEX:
		pthread_mutex_unlock(&mutex[i]);
		break;
	case LOCK_SPIN:
		__sync_lock_release(&spinlock[i]);
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
    {"lists", required_argument, NULL, 'l'},
    {NULL, 0, NULL, 0},
  };

  int opt, i;
  while ( (opt = getopt_long(argc, argv, "t:i:y:s:l:", longOptions, NULL)) != -1 ) {
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

     case 'l':
     	n_list = atoi(optarg);
     	break;

    default:
      break;
    }
  }
}

void rand_key(char *key, int size) {
	static char s[] = "abcdefghijklmnopqrstuvwxyz";
	int i;

	for (i = 0; i < size; i++)
		key[i] = s[rand() % (sizeof(s)-1)];

	key[size] = '\0';
}

unsigned long hash(const char *key) {
	unsigned long hash = 5381;
	int c;

	while ((c = *key++))
	    hash = ((hash << 5) + hash) + c;

	return hash % n_list;
}

void *worker(void *arg) {
	SortedListElement_t *e = (SortedListElement_t *) arg;
	SortedListElement_t *node;
	int i, list_idx, length;

	// insert elements in list
	for (i = 0; i < n_iteration; i++) {
		list_idx = hash(e[i].key);
		lock(list_idx);
		SortedList_insert(&heads[list_idx], &e[i]);
		unlock(list_idx);
	}


	// get the list length
	length = 0;
	for (i = 0; i < n_list; i++) {
		lock(i);
		length += SortedList_length(&heads[i]);
		unlock(i);
	}

	// looks up and deletes each of the keys
	for (i = 0; i < n_iteration; i++) {
		list_idx = hash(e[i].key);
		lock(list_idx);
		node = SortedList_lookup(&heads[list_idx], e[i].key);
		SortedList_delete(node);
		unlock(list_idx);
	}

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

	// alloc heads
	heads = malloc(n_list * sizeof(SortedList_t));
	for (i = 0; i < n_list; i++) {
		heads[i].prev = &heads[i];
		heads[i].next = &heads[i];
	}

	// create and allocate list elements
	elements = malloc(n_thread * n_iteration * sizeof(SortedListElement_t));
	for (i = 0; i < n_thread * n_iteration; i++) {
		char *key = malloc(KEY_SZ + 1);
		rand_key(key, KEY_SZ);
		elements[i].key = key;
	}

	// allocate mutex
	mutex = malloc(n_list * sizeof(pthread_mutex_t));
	for (i = 0; i < n_list; i++)
		pthread_mutex_init(&mutex[i], NULL);

	// alloc spinlock
	spinlock = malloc(n_list * sizeof(int));
	memset(spinlock, 0, n_list * sizeof(int));

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
		"%llu threads x %llu iterations x (insert + lookup/delete) = %llu operations\n", 
		n_thread, 
		n_iteration,
		n_thread * n_iteration * 2);
	printf("elapsed time: %lluns\n", diff);
	printf("per operation: %lluns\n", diff/(n_thread * n_iteration * 2));

	// check len
	int len = 0;
	for (i = 0; i < n_list; i++)
		len += SortedList_length(&heads[i]);
	if (len != 0)
		fprintf(stderr, "ERROR: final length = %d\n", len);

	#ifdef _GRAPH
	ull sublist_len = n_iteration / n_list;
	printf("sublist size: %lld\n", sublist_len);
	printf("corrected time: %lld\n", diff/(n_thread * n_iteration * 2) / sublist_len);
	#endif

	return len != 0;
}
