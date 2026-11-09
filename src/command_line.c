/*
 * src/command_line.c
 *
 * read the arguments given to minimake
 *
 * see src/command_line.h
 */

#include "command_line.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "safe_memory.h"

// return 1 if the two strings are equal, 0 otherwise
static int str_eq(const char *first, const char *second)
{
    return strcmp(first, second) == 0;
}

// return 1 if str begins with prefix, 0 otherwise
static int starts_with(const char *str, const char *prefix)
{
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

// at the start no option is active
static void init_options(struct options *options)
{
    options->makefile_path = NULL;
    options->help = 0;
    options->pretty_print = 0;
    options->unknown_option = 0;
    options->targets = NULL;
    options->nb_targets = 0;
}

// return 1 if the option gives the makefile, 0 if not, -1 on error
static int read_file_option(int argc, char **argv, int *i,
                            struct options *options)
{
    const char *arg = argv[*i];

    if (str_eq(arg, "-f"))
    {
        if (*i + 1 >= argc)
        {
            fprintf(stderr, "minimake: option requires an argument -- 'f'\n");
            return -1;
        }

        // -f takes the next argument, even if it looks like an option
        *i = *i + 1;
        options->makefile_path = argv[*i];
        return 1;
    }

    if (starts_with(arg, "--file="))
    {
        options->makefile_path = arg + strlen("--file=");
        return 1;
    }

    if (starts_with(arg, "--makefile="))
    {
        options->makefile_path = arg + strlen("--makefile=");
        return 1;
    }

    return 0;
}

// return 1 if arg is a -h or -p option, 0 otherwise
static int read_flag(const char *arg, struct options *options)
{
    if (str_eq(arg, "-h") || str_eq(arg, "--help"))
    {
        options->help = 1;
        return 1;
    }

    if (str_eq(arg, "-p") || str_eq(arg, "--print-data-base")
        || str_eq(arg, "--pretty-print"))
    {
        options->pretty_print = 1;
        return 1;
    }

    return 0;
}

// handle one argument, return -1 if we must stop
static int read_arg(int argc, char **argv, int *i, struct options *options)
{
    char *arg = argv[*i];

    // an empty argument is always an error
    if (arg[0] == '\0')
    {
        fprintf(stderr,
                "minimake: *** empty string invalid as argument. Stop.\n");
        return -1;
    }

    int file_option = read_file_option(argc, argv, i, options);
    if (file_option != 0)
        return file_option == -1 ? -1 : 0;

    if (read_flag(arg, options))
        return 0;

    // the known options are handled above, so this one does not exist
    if (arg[0] == '-')
    {
        fprintf(stderr, "minimake: unrecognized option '%s'\n", arg);
        options->unknown_option = 1;
        return 0;
    }

    // not an option: it is a target
    options->targets[options->nb_targets] = arg;
    options->nb_targets++;
    return 0;
}

enum parse_result parse_args(int argc, char **argv, struct options *options)
{
    init_options(options);
    // there can not be more targets than arguments
    options->targets = safe_malloc((argc + 1) * sizeof(char *));

    // argv[0] is the name of the program, so we start at 1
    for (int i = 1; i < argc; i++)
    {
        if (read_arg(argc, argv, &i, options) == -1)
            return PARSE_STOP_ERROR;
    }

    // all the arguments are read first because -h has priority
    if (options->help)
    {
        print_help();
        return PARSE_STOP_SUCCESS;
    }

    // the help is also printed when an option does not exist
    if (options->unknown_option)
    {
        print_help();
        return PARSE_STOP_ERROR;
    }

    return PARSE_CONTINUE;
}

void print_help(void)
{
    printf("Usage: minimake [options] [target] ...\n"
           "Options:\n"
           "  -f FILE, --file=FILE, --makefile=FILE\n"
           "                    Read FILE as the makefile.\n"
           "  -h, --help        Print this message and exit.\n"
           "  -p, --print-data-base, --pretty-print\n"
           "                    Print the parsed makefile.\n");
}

void free_options(struct options *options)
{
    free(options->targets);
    options->targets = NULL;
    options->nb_targets = 0;
}
