/*
 * src/text_tools.c
 *
 * small functions to read and build strings
 *
 * see src/text_tools.h
 */

#include "text_tools.h"

#include "safe_memory.h"

// first size of a buffer
#define BUFFER_SIZE 64

int is_blank(char c)
{
    return c == ' ' || c == '\t';
}

const char *skip_blanks(const char *str)
{
    while (is_blank(*str))
        str++;

    return str;
}

void split_words(const char *str, struct fifo *words)
{
    str = skip_blanks(str);

    while (*str)
    {
        size_t word_len = 0;

        // a word stops at the next blank or at the end of the string
        while (str[word_len] && !is_blank(str[word_len]))
            word_len++;

        fifo_push(words, safe_strndup(str, word_len));
        str = skip_blanks(str + word_len);
    }
}

char *single_spaces(const char *str)
{
    struct buffer buf;

    buffer_init(&buf);
    // no blanks at the beginning of the result
    str = skip_blanks(str);

    while (*str)
    {
        if (is_blank(*str))
        {
            // a group of blanks becomes only one space
            buffer_add_char(&buf, ' ');
            str = skip_blanks(str);
        }
        else
        {
            buffer_add_char(&buf, *str);
            str++;
        }
    }

    return buf.text;
}

void buffer_init(struct buffer *buf)
{
    buf->text = safe_malloc(BUFFER_SIZE);
    buf->text[0] = '\0';
    buf->len = 0;
    buf->capacity = BUFFER_SIZE;
}

void buffer_add_char(struct buffer *buf, char c)
{
    // + 2 for the new character and the '\0'
    // when the buffer is full we double its size
    if (buf->len + 2 > buf->capacity)
    {
        buf->capacity *= 2;
        buf->text = safe_realloc(buf->text, buf->capacity);
    }

    buf->text[buf->len] = c;
    buf->len++;
    buf->text[buf->len] = '\0';
}

void buffer_add_str(struct buffer *buf, const char *str)
{
    while (*str)
    {
        buffer_add_char(buf, *str);
        str++;
    }
}
