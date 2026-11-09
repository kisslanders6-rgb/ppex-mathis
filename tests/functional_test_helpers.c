/*
 * tests/functional_test_helpers.c
 *
 * run the real minimake in a temporary directory and read its outputs
 *
 * see tests/functional_test_helpers.h
 */

#include "functional_test_helpers.h"

#include <criterion/criterion.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#define PROJECT_DIRECTORY_VARIABLE "MINIMAKE_PROJECT_DIRECTORY"
#define CAPTURED_OUTPUT_FILE ".captured_standard_output"
#define CAPTURED_ERROR_FILE ".captured_standard_error"

void create_and_enter_test_directory(void)
{
    char project_directory[PATH_MAX];
    char directory_template[PATH_MAX];
    const char *temporary_root = getenv("TMPDIR");

    // criterion starts in the project: remember it before leaving
    cr_assert_not_null(getcwd(project_directory, sizeof(project_directory)));
    cr_assert_eq(setenv(PROJECT_DIRECTORY_VARIABLE, project_directory, 1), 0);

    snprintf(directory_template, sizeof(directory_template),
             "%s/minimake_test_XXXXXX",
             temporary_root ? temporary_root : "/tmp");
    cr_assert_not_null(mkdtemp(directory_template));
    cr_assert_eq(chdir(directory_template), 0);
}

void leave_and_delete_test_directory(void)
{
    char test_directory[PATH_MAX];

    if (!getcwd(test_directory, sizeof(test_directory)) || chdir("/") == -1)
        return;

    pid_t process_id = fork();

    if (process_id == 0)
    {
        execlp("rm", "rm", "-rf", test_directory, NULL);
        _exit(127);
    }

    waitpid(process_id, NULL, 0);
}

char *get_path_in_project(const char *relative_path)
{
    const char *project_directory = getenv(PROJECT_DIRECTORY_VARIABLE);
    size_t path_size = strlen(project_directory) + strlen(relative_path) + 2;
    char *full_path = malloc(path_size);

    cr_assert_not_null(full_path);
    snprintf(full_path, path_size, "%s/%s", project_directory, relative_path);
    return full_path;
}

void write_text_file(const char *file_name, const char *content)
{
    FILE *file = fopen(file_name, "w");

    cr_assert_not_null(file, "cannot create %s", file_name);
    cr_assert_geq(fputs(content, file), 0);
    cr_assert_eq(fclose(file), 0);
}

void set_file_age_in_seconds(const char *file_name, long age_in_seconds)
{
    struct timeval now;

    cr_assert_eq(gettimeofday(&now, NULL), 0);
    now.tv_sec -= age_in_seconds;

    struct timeval access_and_modification[2] = { now, now };

    cr_assert_eq(utimes(file_name, access_and_modification), 0);
}

// return all the content of a file, it must be freed
static char *read_whole_file(const char *file_name)
{
    FILE *file = fopen(file_name, "r");

    cr_assert_not_null(file);

    size_t capacity = 4096;
    size_t length = 0;
    char *content = malloc(capacity);

    cr_assert_not_null(content);
    while (!feof(file))
    {
        length += fread(content + length, 1, capacity - length - 1, file);
        if (length + 1 == capacity)
        {
            capacity *= 2;
            content = realloc(content, capacity);
            cr_assert_not_null(content);
        }
    }

    content[length] = '\0';
    cr_assert_eq(fclose(file), 0);
    return content;
}

// child process: send the outputs to files, then become minimake
static void redirect_outputs_and_execute_minimake(char *binary_path,
                                                  char **arguments)
{
    int output_file =
        open(CAPTURED_OUTPUT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int error_file =
        open(CAPTURED_ERROR_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (output_file == -1 || error_file == -1
        || dup2(output_file, STDOUT_FILENO) == -1
        || dup2(error_file, STDERR_FILENO) == -1)
        _exit(126);

    close(output_file);
    close(error_file);
    execv(binary_path, arguments);
    _exit(127);
}

struct minimake_result run_minimake(char **arguments)
{
    char *binary_path = get_path_in_project("minimake");
    char *full_arguments[64] = { binary_path };
    int argument_count = 0;

    while (arguments[argument_count])
    {
        cr_assert_lt(argument_count, 62);
        full_arguments[argument_count + 1] = arguments[argument_count];
        argument_count++;
    }

    pid_t process_id = fork();

    cr_assert_neq(process_id, -1);
    if (process_id == 0)
        redirect_outputs_and_execute_minimake(binary_path, full_arguments);

    int status = 0;
    struct minimake_result result = { 0, NULL, NULL };

    cr_assert_neq(waitpid(process_id, &status, 0), -1);
    cr_assert(WIFEXITED(status), "minimake crashed");
    result.exit_code = WEXITSTATUS(status);
    result.standard_output = read_whole_file(CAPTURED_OUTPUT_FILE);
    result.standard_error = read_whole_file(CAPTURED_ERROR_FILE);
    free(binary_path);
    return result;
}

struct minimake_result run_minimake_on_makefile(const char *makefile_text,
                                                char **arguments)
{
    write_text_file("Makefile", makefile_text);
    return run_minimake(arguments);
}
