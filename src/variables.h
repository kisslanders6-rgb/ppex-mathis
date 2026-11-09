/*
 * src/variables.h
 *
 * variables of the makefile and expansion of the references
 */

#ifndef VARIABLES_H
#define VARIABLES_H

#include "fifo.h"

struct var
{
    char *name; // name of the variable
    char *value; // value of the variable, not expanded
};

/* create the variable, or replace its value if it already exists */
void var_set(struct fifo *vars, const char *name, const char *value);

/* return the variable with this name, NULL if it does not exist */
struct var *var_find(const struct fifo *vars, const char *name);

/* return a new string with each reference replaced by its value */
/* return NULL if a reference is not closed */
char *expand_text(const struct fifo *vars, const char *text);

/* free a struct var, void * lets us give it to fifo_destroy */
void var_destroy(void *to_destroy);

#endif /* ! VARIABLES_H */