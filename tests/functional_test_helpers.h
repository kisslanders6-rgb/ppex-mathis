/*
 * tests/functional_test_helpers.h
 *
 * run the real minimake in a temporary directory and read its outputs
 */

#ifndef FUNCTIONAL_TEST_HELPERS_H
#define FUNCTIONAL_TEST_HELPERS_H

/* what we can see from outside after running minimake */
struct minimake_result
{
    int exit_code; // value returned by minimake
    char *standard_output; // everything printed on stdout
    char *standard_error; // everything printed on stderr
};

/* build the list of arguments given to run_minimake */
#define ARGUMENTS(...) ((char *[]){ __VA_ARGS__, NULL })

/* empty list of arguments for run_minimake */
#define NO_ARGUMENTS ((char *[]){ NULL })

/* criterion init: create a temporary directory and go inside */
void create_and_enter_test_directory(void);

/* criterion fini: leave the temporary directory and delete it */
void leave_and_delete_test_directory(void);

/* create a file with this content in the current directory */
void write_text_file(const char *file_name, const char *content);

/* put the modification date of the file in the past */
void set_file_age_in_seconds(const char *file_name, long age_in_seconds);

/* run minimake in the current directory with these arguments */
struct minimake_result run_minimake(char **arguments);

/* write the text in a file named Makefile, then run minimake */
struct minimake_result run_minimake_on_makefile(const char *makefile_text,
                                                char **arguments);

/* return the path of a file of the project, it must be freed */
char *get_path_in_project(const char *relative_path);

#endif /* ! FUNCTIONAL_TEST_HELPERS_H */
