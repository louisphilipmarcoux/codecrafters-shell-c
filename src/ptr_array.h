#ifndef CODECRAFTERS_SHELL_PTR_ARRAY_H_INCLUDED
#define CODECRAFTERS_SHELL_PTR_ARRAY_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>

typedef struct PtrArray PtrArray;

// Allocates memory for an empty array of pointers.
PtrArray *ptr_array_create(void);

// Deallocates memory for an array of pointers, and the objects pointed to by the pointers.
void ptr_array_destroy(PtrArray *array, void (*ptr_destroy)(void *));

// Checks whether an array of pointers is empty.
bool ptr_array_is_empty(const PtrArray *array);

// Gets the number of pointers in an array.
size_t ptr_array_get_size(const PtrArray *array);

// Gets the const pointer at a specific location in an array.
const void *ptr_array_get_const(const PtrArray *array, size_t index);

// Gets the pointer at a specific location in an array.
void *ptr_array_get(PtrArray *array, size_t index);

// Appends a pointer to the end of an array.
void ptr_array_append(PtrArray *array, void *ptr);

// Returns the underlying C array of pointers.
void **ptr_array_get_c_array(PtrArray *array);

#endif
