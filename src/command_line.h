/*
 * src/command_line.h
 *
 * read the arguments given to minimake
 */

#ifndef COMMAND_LINE_H
#define COMMAND_LINE_H

/* what main must do after reading the arguments */
enum parse_result
{
    PARSE_CONTINUE, // everything is fine: read the makefile
    PARSE_STOP_SUCCESS, // the help was printed: stop and return 0
    PARSE_STOP_ERROR, // an error was found: stop and return 2
};

struct options
{
    const char *makefile_path; // NULL: look for makefile then Makefile
    int help; // 1 if -h was given
    int pretty_print; // 1 if -p was given
    int unknown_option; // 1 if one option does not exist
    char **targets; // points into argv, strings are not copied
    int nb_targets; // number of strings in targets
};

/* fill options from argv and tell main if it must continue */
enum parse_result parse_args(int argc, char **argv, struct options *options);

/* print how to use minimake on the standard output */
void print_help(void);

/* free what parse_args has allocated */
void free_options(struct options *options);

#endif /* ! COMMAND_LINE_H */
