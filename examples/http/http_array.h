#include <stdint.h>

enum HTTP_linked_list_actions {
	ARRAY_STOP,
	ARRAY_CONTINUE,
	ARRAY_FREE
};

struct HTTP_object {
	void* object;
	struct HTTP_object* next;
	struct HTTP_object* prev;
};

struct HTTP_linked_list {
	struct HTTP_object* top;
	struct HTTP_object* base;
	uint64_t size;
};

void HTTP_arrayPush(struct HTTP_linked_list* array, void* data);

void* HTTP_arrayPop(struct HTTP_linked_list* array);

void HTTP_arrayForEach(struct HTTP_linked_list* array, enum HTTP_linked_list_actions(*function)(struct HTTP_object*, void*, uint64_t), void* parms);

void HTTP_arrayClear(struct HTTP_linked_list* array);