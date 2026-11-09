/*
 * src/execution.h
 *
 * choose which targets must be built and run their recipes
 */

#ifndef EXECUTION_H
#define EXECUTION_H

#include "command_line.h"
#include "parser.h"

/* build the asked targets, or the first rule if there is none */
/* return the exit code of minimake: 0 on success, 2 on error */
int build_targets(struct makefile *makefile, const struct options *options);

#endif /* ! EXECUTION_H */
