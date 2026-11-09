/*
 * src/text_tools.h
 *
 * small functions to read and build strings
 */

#ifndef TEXT_TOOLS_H
#define TEXT_TOOLS_H

#include <stddef.h>

#include "fifo.h"

/* string that grows when we add characters */
struct buffer
{
    char *text; // the string, always ends with '\0'
    size_t len; // number of characters without the '\0'
    size_t capacity; // size of the memory allocated for text
};

/* return 1 for a space or a tab, 0 otherwise */
int is_blank(char c);

/* return the position of the first character that is not a blank */
const char *skip_blanks(const char *str);

/* cut str on the blanks and add a copy of each word in words */
void split_words(const char *str, struct fifo *words);

/* return a copy without first blanks, each group of blanks becomes a space */
char *single_spaces(const char *str);

/* prepare an empty buffer, buf->text must be freed after */
void buffer_init(struct buffer *buf);

/* add one character at the end of the buffer */
void buffer_add_char(struct buffer *buf, char c);

/* add a whole string at the end of the buffer */
void buffer_add_str(struct buffer *buf, const char *str);

#endif /* ! TEXT_TOOLS_H */
