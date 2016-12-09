#define _GNU_SOURCE
#include <pthread.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef unsigned long long ull;

// global arguments
ull n_thread = 1;
ull n_iteration = 1;
long long counter;
int opt_yield;
int opt_sync;

// lock types
enum LockType {
	LOCK_NULL = 0,
	LOCK_MUTEX,
	LOCK_SPIN,
	LOCK_COMPARE_AND_SWAP
};
pthread_mutex_t lock;
int spin_lock;

void parse(int argc, char *argv[]) {
  struct option longOptions[] = 
  {
    {"threads", required_argument, NULL, 't'},
    {"iterations", required_argument, NULL, 'i'},
    {"yield", no_argument, NULL, 'y'},
    {"sync", required_argument, NULL, 's'},
    {NULL, 0, NULL, 0},
  };

  int opt;
  while ( (opt = getopt_long(argc, argv, "t:i:ys:", longOptions, NULL)) != -1 )
  {
    switch (opt) 
    {
    case 't':
      n_thread = atoi(optarg);
      break;

    case 'i':
      n_iteration = atoi(optarg);
      break;

    case 'y':
      opt_yield = 1;
      break;

    case 's':
      switch (optarg[0]) {
      	case 'm':
      	  opt_sync = LOCK_MUTEX;
      	  break;
      	case 's':
      	  opt_sync = LOCK_SPIN;
      	  break;
      	case 'c':
      	  opt_sync = LOCK_COMPARE_AND_SWAP;
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

void add(long long *pointer, long long value) {
		long long sum = *pointer + value;
		if (opt_yield)
			pthread_yield();
		*pointer = sum;
}

void *thread_entry(void *arg) {
	size_t i;
	long long prev;
	int wait;

	// increment
	for (i = 0; i < n_iteration; i++) {
		if (opt_sync == LOCK_MUTEX) {
			pthread_mutex_lock(&lock);
			add(&counter, 1);
			pthread_mutex_unlock(&lock);
		} else if (opt_sync == LOCK_SPIN) {
			while (__sync_lock_test_and_set(&spin_lock, 1 ) == 1)
				for (wait = 0; wait < 1000; wait++);
			add(&counter, 1);
			__sync_lock_release(&spin_lock);
		} else if (opt_sync == LOCK_COMPARE_AND_SWAP) {
			do {
				prev = counter;
				if (opt_yield)
					pthread_yield();
			} while (__sync_val_compare_and_swap(&counter, prev, prev+1) != prev);
		} else {
			add(&counter, 1);
		}
	}

	// decrement
	for (i = 0; i < n_iteration; i++) {
		if (opt_sync == LOCK_MUTEX) {
			pthread_mutex_lock(&lock);
			add(&counter, -1);
			pthread_mutex_unlock(&lock);
		} else if (opt_sync == LOCK_SPIN) {
			while (__sync_lock_test_and_set(&spin_lock, 1 ) == 1);
			add(&counter, -1);
			__sync_lock_release(&spin_lock);
		} else if (opt_sync == LOCK_COMPARE_AND_SWAP) {
			do {
				prev = counter;
				if (opt_yield)
					pthread_yield();
			} while (__sync_val_compare_and_swap(&counter, prev, prev-1) != prev);
		} else {
			add(&counter, -1);
		}
	}
	return NULL;
}

int main(int argc, char *argv[]) {
	ull i;
	pthread_t *threads;
	struct timespec tps, tpe;
	ull diff;

	// init lock
	pthread_mutex_init(&lock, NULL);

	// read arguments
	parse(argc, argv);

	// alloc space for thread structure
	threads = malloc(n_thread * sizeof(pthread_t));

	// time start
	clock_gettime(CLOCK_MONOTONIC, &tps);

	// create threads
	for (i = 0; i < n_thread; i++)
		pthread_create(&threads[i], NULL, thread_entry, NULL);

	// wait for threads
	for (i = 0; i < n_thread; i++)
		pthread_join(threads[i], NULL);

	// time end
	clock_gettime(CLOCK_MONOTONIC, &tpe);
	diff = (tpe.tv_sec - tps.tv_sec) * 1000000000LL + (tpe.tv_nsec - tps.tv_nsec);

	// print stats
	printf( 
		"%llu threads x %llu iterations x (add + subtract) = %llu operations\n", 
		n_thread, 
		n_iteration, 
		n_thread * n_iteration * 2);

	if (counter != 0)
		fprintf(stderr, "ERROR: final count = %lld\n", counter);

	printf("elapsed time: %lluns\n", diff);
	// printf("start:\t%luns\nend:\t%luns\n", tps.tv_nsec, tpe.tv_nsec);
	// printf("start:\t%lus\nend:\t%lus\n", tps.tv_sec, tpe.tv_sec);

	printf("per operation: %lluns\n", diff/(n_thread * n_iteration * 2));

	return counter == 0 ? 0 : 1;
}
