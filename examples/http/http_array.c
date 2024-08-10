#include "http_array.h"
#include <stdlib.h>

void HTTP_arrayPush(struct HTTP_linked_list* array, void* data) {
	if (array == NULL || data == NULL)
		return;

	struct HTTP_object* new = calloc(1, sizeof(struct HTTP_object));
	new->object = data;
	array->size++;
	if (array->base == NULL && array->top == NULL) {
		array->base = array->top = new;
		return;
	}
	array->top->next = new;
	new->prev = array->top;
	array->top = new;
}

void* HTTP_arrayPop(struct HTTP_linked_list* array) {
	if (array == NULL || array->top == NULL)
		return NULL;

	void* returned = array->top->object;
	struct HTTP_object* toFree = array->top;

	if (array->top->prev == NULL) {
		array->base = NULL;
	}
	else {
		array->top = array->top->prev;
		array->top->next = NULL;
	}

	free(toFree);
	array->size--;
	return returned;
}

void HTTP_arrayForEach(struct HTTP_linked_list* array, enum HTTP_linked_list_actions(*function)(struct HTTP_object*, void*, uint64_t), void* parms) {
	if (array == NULL || array->top == NULL || array->base == NULL)
		return;
	uint64_t count = 0;
	for (struct HTTP_object* obj = array->base; obj != NULL; ) {
		struct HTTP_object* prv = obj->prev;
		struct HTTP_object* next = obj->next;

		switch (function(obj, parms, count))
		{
		case ARRAY_STOP:
			return;
		case ARRAY_CONTINUE:
			obj = next;
			break;
		case ARRAY_FREE:
			if (prv != NULL)
				prv->next = next;
			if (obj == array->top)
				array->top = prv;
			if (obj == array->base)
				array->base = next;
			obj = next;
			break;
		}
		count++;
	}
}

static enum HTTP_linked_list_actions HTTP_arrayFreeAction(struct HTTP_object* obj, void* data, uint64_t count) {
	free(obj);
	return ARRAY_FREE;
}

void HTTP_arrayClear(struct HTTP_linked_list* array) {
	HTTP_arrayForEach(array, HTTP_arrayFreeAction, NULL);
}