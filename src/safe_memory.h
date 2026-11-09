/*
 * src/safe_memory.h
 *
 * allocation functions that stop the program when malloc fails
 */

#ifndef SAFE_MEMORY_H
#define SAFE_MEMORY_H

#include <stddef.h>

/* same as malloc, but exit with code 2 if there is no memory left */
void *safe_malloc(size_t size);

/* same as realloc, but exit with code 2 if there is no memory left */
void *safe_realloc(void *ptr, size_t size);

/* return a copy of the first len characters of str */
char *safe_strndup(const char *str, size_t len);

/* return a copy of str, strdup is not available in c99 */
char *safe_strdup(const char *str);

#endif /* ! SAFE_MEMORY_H */
