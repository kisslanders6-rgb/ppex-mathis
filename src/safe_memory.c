/*
 * src/safe_memory.c
 *
 * allocation functions that stop the program when malloc fails
 *
 * see src/safe_memory.h
 */

#include "safe_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// no memory left: we can not continue, so we stop right now
static void memory_error(void)
{
    perror("minimake: memory allocation failed");
    exit(2);
}

void *safe_malloc(size_t size)
{
    void *new_memory = malloc(size);
    if (!new_memory)
        memory_error();

    return new_memory;
}

void *safe_realloc(void *ptr, size_t size)
{
    void *new_memory = realloc(ptr, size);
    if (!new_memory)
        memory_error();

    return new_memory;
}

char *safe_strndup(const char *str, size_t len)
{
    // + 1 for the '\0' at the end
    char *new_memory = safe_malloc(len + 1);
    memcpy(new_memory, str, len);
    new_memory[len] = '\0';

    return new_memory;
}

char *safe_strdup(const char *str)
{
    return safe_strndup(str, strlen(str));
}
