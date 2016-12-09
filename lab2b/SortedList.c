#define _GNU_SOURCE
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "SortedList.h"

int opt_yield = 0;

void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
	SortedListElement_t *p = list->next;

	// find the insert position
	while (p != list && strcmp(p->key, element->key) < 0)
		p = p->next;

	// call pthread_yield in middle of critical section
	if (opt_yield & INSERT_YIELD)
		pthread_yield();

	// insert
	element->next = p;
	element->prev = p->prev;
	element->prev->next = element;
	element->next->prev = element;
	
}

int SortedList_delete( SortedListElement_t *element) {
	// check if the node is corrupted
	if (element->next == NULL|| element->prev == NULL ||
		element->next->prev != element ||
		element->prev->next != element)
		return 1;

	if (opt_yield & DELETE_YIELD)
		pthread_yield();

	// delete the node from the list
	element->next->prev = element->prev;
	element->prev->next = element->next;
	return 0;

}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {
	SortedListElement_t *p = list->next;
	int rc;

	while (p != list) {
		rc = strcmp(p->key, key);
		if (rc == 0)
			return p;
		if (rc > 0)
			return NULL;
		p = p->next;

		if (opt_yield & SEARCH_YIELD)
			pthread_yield();
	}
	return NULL;
}

int SortedList_length(SortedList_t *list) {
	SortedListElement_t *p = list->next;
	int count = 0;

	while (p != list) {
		if (p->next == NULL || p->prev == NULL ||
			p->next->prev != p ||
			p->prev->next != p)
			return -1;
		count++;
		p = p->next;

		if (opt_yield & SEARCH_YIELD)
			pthread_yield();
	}

	return count;
}