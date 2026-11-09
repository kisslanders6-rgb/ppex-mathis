/*
 * src/main.c
 *
 * entry point: read the options, read the makefile, then print or build
 */

#include "command_line.h"
#include "execution.h"
#include "parser.h"

// read the makefile then print or build it, return the exit code
static int load_and_run(const struct options *options)
{
    struct makefile makefile;
    int exit_code = 2;

    makefile_init(&makefile);
    if (makefile_load(&makefile, options) == 0)
    {
        // -p only prints the makefile, nothing is built
        if (options->pretty_print)
        {
            makefile_print(&makefile);
            exit_code = 0;
        }
        else
        {
            exit_code = build_targets(&makefile, options);
        }
    }

    makefile_destroy(&makefile);
    return exit_code;
}

int main(int argc, char **argv)
{
    struct options options;
    enum parse_result parse_status = parse_args(argc, argv, &options);
    // used if we stop right after reading the arguments
    int exit_code = parse_status == PARSE_STOP_SUCCESS ? 0 : 2;

    if (parse_status == PARSE_CONTINUE)
        exit_code = load_and_run(&options);

    free_options(&options);
    return exit_code;
}
