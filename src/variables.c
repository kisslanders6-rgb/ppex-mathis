/*
 * src/variables.c
 *
 * variables of the makefile and expansion of the references
 *
 * see src/variables.h
 */

#include "variables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "safe_memory.h"
#include "text_tools.h"

struct var *var_find(const struct fifo *vars, const char *name)
{
    struct list *curr = vars->head;

    while (curr)
    {
        struct var *var = curr->data;
        if (strcmp(var->name, name) == 0)
            return var;

        curr = curr->next;
    }

    return NULL;
}

void var_set(struct fifo *vars, const char *name, const char *value)
{
    struct var *var = var_find(vars, name);
    // the variable exists: only replace its value
    if (var)
    {
        free(var->value);
        var->value = safe_strdup(value);
        return;
    }

    // first time we see this name: create the variable
    var = safe_malloc(sizeof(struct var));
    var->name = safe_strdup(name);
    var->value = safe_strdup(value);
    fifo_push(vars, var);
}

// add the value of the variable, an unknown variable is just empty
static void add_value(const struct fifo *vars, struct buffer *expanded,
                      const char *name)
{
    struct var *var = var_find(vars, name);
    if (var)
        buffer_add_str(expanded, var->value);
}

// handle $(NAME) and ${NAME}, *pos is on the opening character
static int expand_delimited(const struct fifo *vars, struct buffer *expanded,
                            const char **pos)
{
    char closing = **pos == '(' ? ')' : '}';
    const char *name_start = *pos + 1;
    const char *name_end = strchr(name_start, closing);

    // a reference that is not closed is a syntax error
    if (!name_end)
    {
        fprintf(stderr,
                "minimake: *** unterminated variable reference. Stop.\n");
        return -1;
    }

    char *name = safe_strndup(name_start, name_end - name_start);
    add_value(vars, expanded, name);
    free(name);

    // continue just after the closing character
    *pos = name_end + 1;
    return 0;
}

// *pos is on the character just after the $
static int expand_dollar(const struct fifo *vars, struct buffer *expanded,
                         const char **pos)
{
    char current_char = **pos;

    if (current_char == '(' || current_char == '{')
        return expand_delimited(vars, expanded, pos);

    // a $ alone at the end of the string stays a $
    if (current_char == '\0')
    {
        buffer_add_char(expanded, '$');
        return 0;
    }

    // $$ gives one $
    if (current_char == '$')
    {
        buffer_add_char(expanded, '$');
    }
    else
    {
        // $V: the name is only one character
        char name[2] = { current_char, '\0' };
        add_value(vars, expanded, name);
    }

    *pos = *pos + 1;
    return 0;
}

char *expand_text(const struct fifo *vars, const char *text)
{
    struct buffer expanded;

    buffer_init(&expanded);

    // copy the text and replace each reference we find
    while (*text)
    {
        char current_char = *text;
        text++;

        if (current_char != '$')
        {
            buffer_add_char(&expanded, current_char);
        }
        else if (expand_dollar(vars, &expanded, &text) == -1)
        {
            free(expanded.text);
            return NULL;
        }
    }

    return expanded.text;
}

void var_destroy(void *to_destroy)
{
    // void* -> struct var*
    struct var *var = to_destroy;

    free(var->name);
    free(var->value);
    free(var);
}
