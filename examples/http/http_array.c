#include "http_array.h"
#include <stdlib.h>

/**
 * Adiciona um novo elemento ao final do array (lista ligada).
 *
 * @param array Ponteiro para a lista ligada onde o dado ser� adicionado.
 * @param data Ponteiro para o dado a ser adicionado.
 */
void HTTP_arrayPush(struct HTTP_linked_list* array, void* data) {
    if (array == NULL || data == NULL)
        return;

    // Alocar mem�ria para o novo objeto
    struct HTTP_object* newObj = calloc(1, sizeof(struct HTTP_object));
    if (newObj == NULL) {
        perror("Failed to allocate memory for new object");
        return;
    }
    newObj->object = data;

    // Atualizar os ponteiros da lista
    array->size++;
    if (array->base == NULL && array->top == NULL) {
        array->base = array->top = newObj;
    }
    else {
        array->top->next = newObj;
        newObj->prev = array->top;
        array->top = newObj;
    }
}

/**
 * Remove e retorna o elemento do final do array (lista ligada).
 *
 * @param array Ponteiro para a lista ligada de onde o dado ser� removido.
 * @return Ponteiro para o dado removido, ou NULL se a lista estiver vazia.
 */
void* HTTP_arrayPop(struct HTTP_linked_list* array) {
    if (array == NULL || array->top == NULL)
        return NULL;

    // Remover o elemento do final da lista
    void* returnedData = array->top->object;
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
    return returnedData;
}

/**
 * Executa uma fun��o para cada elemento da lista ligada.
 *
 * @param array Ponteiro para a lista ligada.
 * @param function Fun��o a ser executada para cada elemento.
 * @param parms Par�metros adicionais a serem passados para a fun��o.
 */
void HTTP_arrayForEach(struct HTTP_linked_list* array, enum HTTP_linked_list_actions(*function)(struct HTTP_object*, void*, uint64_t), void* parms) {
    if (array == NULL || array->top == NULL || array->base == NULL)
        return;

    uint64_t count = 0;
    for (struct HTTP_object* obj = array->base; obj != NULL; ) {
        struct HTTP_object* prev = obj->prev;
        struct HTTP_object* next = obj->next;

        switch (function(obj, parms, count)) {
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
            obj = next;
            break;
        }
        count++;
    }
}

/**
 * Fun��o auxiliar para liberar a mem�ria de cada objeto na lista.
 *
 * @param obj Ponteiro para o objeto a ser liberado.
 * @param data Dados adicionais (n�o utilizados).
 * @param count Contador de elementos (n�o utilizado).
 * @return Enumera��o indicando que a a��o ARRAY_FREE deve ser tomada.
 */
static enum HTTP_linked_list_actions HTTP_arrayFreeAction(struct HTTP_object* obj, void* data, uint64_t count) {
    (void)count;
    (void)data;
    free(obj->object);  // Assumindo que o objeto cont�m dados que devem ser liberados
    free(obj);
    return ARRAY_FREE;
}

/**
 * Limpa todos os elementos da lista ligada e libera a mem�ria.
 *
 * @param array Ponteiro para a lista ligada a ser limpa.
 */
void HTTP_arrayClear(struct HTTP_linked_list* array) {
    HTTP_arrayForEach(array, HTTP_arrayFreeAction, NULL);
}
