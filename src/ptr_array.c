#include "ptr_array.h"
#include "xmalloc.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

struct PtrArray {
    void **ptrs;
    size_t size;
    size_t capacity;
};

PtrArray *ptr_array_create(void) {
    PtrArray *array = xmalloc(sizeof(PtrArray));
    array->capacity = 10;
    array->ptrs = xmalloc(sizeof(void *) * array->capacity);
    array->size = 0;
    return array;
}

void ptr_array_destroy(PtrArray *array, void (*ptr_destroy)(void *)) {
    for (size_t i = 0; i < array->size; i++) {
        ptr_destroy(array->ptrs[i]);
    }
    free(array->ptrs);
    free(array);
}

bool ptr_array_is_empty(const PtrArray *array) {
    return array->size == 0;
}

size_t ptr_array_get_size(const PtrArray *array) {
    return array->size;
}

const void *ptr_array_get_const(const PtrArray *array, size_t index) {
    assert(index < array->size);
    return array->ptrs[index];
}

void *ptr_array_get(PtrArray *array, size_t index) {
    assert(index < array->size);
    return array->ptrs[index];
}

void ptr_array_append(PtrArray *array, void *ptr) {
    if (array->size == array->capacity) {
        array->capacity *= 2;
        array->ptrs = xrealloc(array->ptrs, sizeof(void *) * array->capacity);
    }
    array->ptrs[array->size++] = ptr;
}

void **ptr_array_get_c_array(PtrArray *array) {
    return array->ptrs;
}
