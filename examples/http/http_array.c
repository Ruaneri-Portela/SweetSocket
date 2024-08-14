#include "http_array.h"
#include <stdlib.h>

void *HTTP_arrayPop(struct HTTP_linked_list *array)
{
    if (array == NULL || array->top == NULL)
        return NULL;
    void *returnedData = array->top->object;
    struct HTTP_object *toFree = array->top;
    if (array->top->prev == NULL)
    {
        array->base = NULL;
    }
    else
    {
        array->top = array->top->prev;
        array->top->next = NULL;
    }
    free(toFree);
    array->size--;
    return returnedData;
}

void HTTP_arrayForEach(struct HTTP_linked_list *array, enum HTTP_linked_list_actions (*function)(struct HTTP_object *, void *, uint64_t), void *parms)
{
    if (array == NULL || array->top == NULL || array->base == NULL)
        return;
    uint64_t count = 0;
    for (struct HTTP_object *obj = array->base; obj != NULL;)
    {
        struct HTTP_object *prev = obj->prev;
        struct HTTP_object *next = obj->next;

        switch (function(obj, parms, count))
        {
        case ARRAY_STOP:
            return;
        case ARRAY_CONTINUE:
            obj = next;
            break;
        case ARRAY_FREE:
            if (prev != NULL)
                prev->next = next;
            if (obj == array->top)
                array->top = prev;
            if (obj == array->base)
                array->base = next;
            free(obj);
            obj = next;
            break;
        }
        count++;
    }
}

static enum HTTP_linked_list_actions HTTP_arrayFreeAction(struct HTTP_object *obj, void *data, uint64_t count)
{
    (void)count;
    (void)data;
    free(obj->object);
    return ARRAY_FREE;
}

void HTTP_arrayClear(struct HTTP_linked_list *array)
{
    HTTP_arrayForEach(array, HTTP_arrayFreeAction, NULL);
}