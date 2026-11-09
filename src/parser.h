/*
 * src/parser.h
 *
 * read a makefile and store its variables and rules
 */

#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>

#include "command_line.h"
#include "fifo.h"

struct makefile
{
    struct fifo *vars; // elements are struct var *
    struct fifo *rules; // elements are struct rule *
};

/* create the empty lists of the makefile */
void makefile_init(struct makefile *makefile);

/* find the makefile to read and parse it, return -1 on error */
int makefile_load(struct makefile *makefile, const struct options *options);

/* read all the lines of an opened makefile, return -1 on error */
int parse_lines(struct makefile *makefile, FILE *file);

/* option -p: print the variables then the rules */
void makefile_print(const struct makefile *makefile);

/* free all the variables and all the rules */
void makefile_destroy(struct makefile *makefile);

#endif /* ! PARSER_H */
