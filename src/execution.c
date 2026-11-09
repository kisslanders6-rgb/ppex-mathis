/*
 * src/execution.c
 *
 * choose which targets must be built and run their recipes
 *
 * see src/execution.h
 */

#define _POSIX_C_SOURCE 200809L

#include "execution.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "rules.h"
#include "text_tools.h"
#include "variables.h"

// what all the build functions need to share
struct build_ctx
{
    struct makefile *makefile; // the variables and the rules
    size_t nb_started; // number of commands started since the beginning
};

// fill mtime with the last modification date, return -1 if no file
static int get_mtime(const char *path, struct timespec *mtime)
{
    struct stat file_info;

    if (stat(path, &file_info) == -1)
        return -1;

// the field st_mtim does not exist on macOS
#ifdef __APPLE__
    mtime->tv_sec = file_info.st_mtime;
    mtime->tv_nsec = file_info.st_mtimensec;
#else /* ! __APPLE__ */
    *mtime = file_info.st_mtim;
#endif /* __APPLE__ */

    return 0;
}

// return 1 if a file with this name exists, 0 otherwise
static int file_exists(const char *path)
{
    struct timespec mtime;

    return get_mtime(path, &mtime) == 0;
}

// compare the seconds first, then the nanoseconds if they are equal
static int is_after(const struct timespec *first_time,
                    const struct timespec *second_time)
{
    if (first_time->tv_sec != second_time->tv_sec)
        return first_time->tv_sec > second_time->tv_sec;

    return first_time->tv_nsec > second_time->tv_nsec;
}

// a dependency without a file has no date, so it is never newer
static int has_newer_dep(const struct rule *rule)
{
    struct timespec target_time;
    struct timespec dep_time;

    if (get_mtime(rule->target, &target_time) == -1)
        return 0;

    struct list *curr = rule->deps->head;

    while (curr)
    {
        if (get_mtime(curr->data, &dep_time) == 0
            && is_after(&dep_time, &target_time))
            return 1;

        curr = curr->next;
    }

    return 0;
}

// conditions from the section Target building of the subject
static int must_build(const struct rule *rule, int dep_rebuilt)
{
    if (dep_rebuilt)
        return 1;

    // no recipe: there is nothing to run for this target
    if (rule->commands->size == 0)
        return 0;

    return !file_exists(rule->target) || has_newer_dep(rule);
}

// empty the buffers so our logs appear before the output of the command
static void flush_outputs(void)
{
    if (fflush(stdout) == EOF || fflush(stderr) == EOF)
        perror("minimake: cannot flush the output");
}

// wait for the child and return the exit code of the command
static int wait_shell(pid_t pid)
{
    int status = 0;

    if (waitpid(pid, &status, 0) == -1)
    {
        perror("minimake: waitpid failed");
        return -1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    // killed by a signal, we do like the shell
    return 128 + WTERMSIG(status);
}

// run the command in a child process with /bin/sh -c
static int run_shell(char *command)
{
    char *args[] = { "sh", "-c", command, NULL };
    pid_t pid = fork();

    if (pid == -1)
    {
        perror("minimake: fork failed");
        return -1;
    }

    if (pid == 0)
    {
        // we are in the child: replace it with the shell
        execv("/bin/sh", args);
        // execv only comes back if it failed
        perror("minimake: cannot run /bin/sh");
        _exit(127);
    }

    // we are in the parent: wait for the end of the command
    return wait_shell(pid);
}

// print the command if needed then run it, return -1 if it fails
static int run_command(struct build_ctx *context, const struct rule *rule,
                       char *command)
{
    int silent = 0;

    // remove the blanks and the @ before the command
    while (*command == '@' || is_blank(*command))
    {
        if (*command == '@')
            silent = 1;
        command++;
    }

    // nothing left after the @ and the blanks
    if (!*command)
        return 0;

    // count it to know if this target did something
    context->nb_started++;
    if (!silent)
        printf("%s\n", command);
    flush_outputs();

    int exit_code = run_shell(command);

    if (exit_code != 0)
        fprintf(stderr, "minimake: *** [%s] Error %d. Stop.\n", rule->target,
                exit_code);

    return exit_code == 0 ? 0 : -1;
}

// expand everything first: a syntax error must stop before any command
static int expand_commands(struct build_ctx *context, const struct rule *rule,
                           struct fifo *expanded_commands)
{
    struct list *curr = rule->commands->head;

    while (curr)
    {
        char *command = expand_text(context->makefile->vars, curr->data);
        if (!command)
            return -1;

        fifo_push(expanded_commands, command);
        curr = curr->next;
    }

    return 0;
}

// stop at the first command that fails
static int run_recipe(struct build_ctx *context, const struct rule *rule)
{
    struct fifo *expanded_commands = fifo_init();
    int status = expand_commands(context, rule, expanded_commands);
    struct list *curr = expanded_commands->head;

    while (curr && status == 0)
    {
        status = run_command(context, rule, curr->data);
        curr = curr->next;
    }

    fifo_destroy(expanded_commands, free);
    return status;
}

// build_target and build_deps call each other
static enum build_state build_target(struct build_ctx *context,
                                     const char *target, const char *parent);

// return -1 on error, else 1 if a dependency was rebuilt
static int build_deps(struct build_ctx *context, const struct rule *rule)
{
    struct list *curr = rule->deps->head;
    int dep_rebuilt = 0;

    while (curr)
    {
        enum build_state state =
            build_target(context, curr->data, rule->target);

        // one dependency failed: the whole target fails
        if (state == STATE_FAILED)
            return -1;

        if (state == STATE_REBUILT)
            dep_rebuilt = 1;

        curr = curr->next;
    }

    return dep_rebuilt;
}

// build the dependencies first, then the target if needed
static enum build_state build_rule(struct build_ctx *context, struct rule *rule)
{
    // mark it now to detect a circular dependency
    rule->state = STATE_IN_PROGRESS;

    int dep_rebuilt = build_deps(context, rule);

    if (dep_rebuilt == -1)
        rule->state = STATE_FAILED;
    else if (!must_build(rule, dep_rebuilt))
        rule->state = STATE_UP_TO_DATE;
    else if (run_recipe(context, rule) == -1)
        rule->state = STATE_FAILED;
    else
        rule->state = STATE_REBUILT;

    return rule->state;
}

// without a rule, the target is only valid if the file exists
static enum build_state no_rule(const char *target, const char *parent)
{
    if (file_exists(target))
        return STATE_UP_TO_DATE;

    if (parent)
        fprintf(stderr,
                "minimake: *** No rule to make target '%s', needed by '%s'. "
                "Stop.\n",
                target, parent);
    else
        fprintf(stderr, "minimake: *** No rule to make target '%s'. Stop.\n",
                target);

    return STATE_FAILED;
}

// return the state of the target after trying to build it
static enum build_state build_target(struct build_ctx *context,
                                     const char *target, const char *parent)
{
    struct rule *rule = rule_find(context->makefile->rules, target);

    if (!rule)
        return no_rule(target, parent);

    // do not loop forever on a circular dependency
    if (rule->state == STATE_IN_PROGRESS)
        return STATE_UP_TO_DATE;

    // a target is never built twice in the same run
    if (rule->state != STATE_NOT_VISITED)
        return rule->state;

    return build_rule(context, rule);
}

// up to date if there is a recipe, nothing to be done otherwise
static void print_nothing_done(const struct build_ctx *context,
                               const char *target)
{
    struct rule *rule = rule_find(context->makefile->rules, target);

    if (rule && rule->commands->size > 0)
        printf("minimake: '%s' is up to date.\n", target);
    else
        printf("minimake: Nothing to be done for '%s'.\n", target);

    flush_outputs();
}

// the message only appears if this target did not start any command
static int build_one(struct build_ctx *context, const char *target)
{
    size_t nb_started_before = context->nb_started;

    if (build_target(context, target, NULL) == STATE_FAILED)
        return -1;

    if (context->nb_started == nb_started_before)
        print_nothing_done(context, target);

    return 0;
}

int build_targets(struct makefile *makefile, const struct options *options)
{
    struct build_ctx context = { makefile, 0 };

    if (options->nb_targets == 0)
    {
        // without a target on the command line we build the first rule
        struct rule *rule = rule_find_default(makefile->rules);

        if (!rule)
        {
            fprintf(stderr, "minimake: *** No targets. Stop.\n");
            return 2;
        }

        return build_one(&context, rule->target) == 0 ? 0 : 2;
    }

    // the targets are built in the given order
    for (int i = 0; i < options->nb_targets; i++)
    {
        if (build_one(&context, options->targets[i]) == -1)
            return 2;
    }

    return 0;
}
