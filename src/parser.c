/*
 * src/parser.c
 *
 * read a makefile and store its variables and rules
 *
 * see src/parser.h
 */

#define _POSIX_C_SOURCE 200809L

#include "parser.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "rules.h"
#include "safe_memory.h"
#include "text_tools.h"
#include "variables.h"

// what we need to keep from one line to the next one
struct parser
{
    struct makefile *makefile; // where we store what we read
    struct rule *curr_rule; // rule that receives the commands, can be NULL
    int is_ignored; // 1 if the last rule had an empty target
};

void makefile_init(struct makefile *makefile)
{
    makefile->vars = fifo_init();
    makefile->rules = fifo_init();
}

void makefile_destroy(struct makefile *makefile)
{
    fifo_destroy(makefile->vars, var_destroy);
    fifo_destroy(makefile->rules, rule_destroy);
}

// expand a name that must be only one word
// *name stays NULL if the name is empty, return -1 on error
static int expand_name(struct parser *parser, const char *raw_name, char **name)
{
    *name = NULL;

    char *expanded = expand_text(parser->makefile->vars, raw_name);
    if (!expanded)
        return -1;

    struct fifo *words = fifo_init();
    int status = 0;

    split_words(expanded, words);
    free(expanded);

    // a target or a variable name can not have several words
    if (words->size > 1)
    {
        fprintf(stderr, "minimake: *** missing separator. Stop.\n");
        status = -1;
    }
    else if (words->size == 1)
    {
        *name = safe_strdup(words->head->data);
    }

    fifo_destroy(words, free);
    return status;
}

// handle "target: deps", both are expanded now (expansion at parsing)
static int parse_rule(struct parser *parser, const char *raw_target,
                      const char *raw_deps)
{
    char *target = NULL;

    // if we stop before the end, no rule receives the next commands
    parser->curr_rule = NULL;
    if (expand_name(parser, raw_target, &target) == -1)
        return -1;

    // a rule with an empty target is ignored with its recipe
    parser->is_ignored = !target;
    if (!target)
        return 0;

    char *deps = expand_text(parser->makefile->vars, raw_deps);
    if (!deps)
    {
        free(target);
        return -1;
    }

    // the next lines with a tab will be added to this rule
    parser->curr_rule = rule_create(parser->makefile->rules, target);
    split_words(deps, parser->curr_rule->deps);
    free(target);
    free(deps);

    return 0;
}

// handle "NAME = value", the value stays raw until it is used
static int parse_variable(struct parser *parser, const char *raw_name,
                          const char *value)
{
    char *name = NULL;

    // a variable ends the recipe of the previous rule
    parser->curr_rule = NULL;
    parser->is_ignored = 0;

    if (expand_name(parser, raw_name, &name) == -1)
        return -1;

    if (!name)
    {
        fprintf(stderr, "minimake: *** empty variable name. Stop.\n");
        return -1;
    }

    // skip the blanks after the '=', the end of the value is kept
    var_set(parser->makefile->vars, name, skip_blanks(value));
    free(name);

    return 0;
}

// the first ':' or '=' of the line tells if it is a rule or a variable
static int parse_definition(struct parser *parser, char *line)
{
    char *separator = strpbrk(line, ":=");

    if (!separator)
    {
        fprintf(stderr, "minimake: *** missing separator. Stop.\n");
        return -1;
    }

    // keep it because we cut the line in two just after
    char separator_char = *separator;
    *separator = '\0';

    if (separator_char == ':')
        return parse_rule(parser, line, separator + 1);

    return parse_variable(parser, line, separator + 1);
}

// handle one line of the makefile, return -1 on error
static int parse_line(struct parser *parser, char *line)
{
    // no comments in a recipe: /bin/sh will ignore them by itself
    if (line[0] == '\t' && (parser->curr_rule || parser->is_ignored))
    {
        const char *command = skip_blanks(line);

        if (parser->curr_rule && *command)
            fifo_push(parser->curr_rule->commands, safe_strdup(command));
        return 0;
    }

    // outside a recipe, everything after a # is ignored
    char *comment = strchr(line, '#');
    if (comment)
        *comment = '\0';

    // blank lines and comments do not end the current rule
    if (!*skip_blanks(line))
        return 0;

    // a tab here means there is no rule to receive this command
    if (line[0] == '\t')
    {
        fprintf(stderr, "minimake: *** command outside of rule. Stop.\n");
        return -1;
    }

    return parse_definition(parser, line);
}

int parse_lines(struct makefile *makefile, FILE *file)
{
    struct parser parser = { makefile, NULL, 0 };
    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_len = getline(&line, &line_capacity, file);
    int status = 0;

    while (line_len != -1 && status == 0)
    {
        // getline keeps the \n, we do not want it
        if (line_len > 0 && line[line_len - 1] == '\n')
            line[line_len - 1] = '\0';

        status = parse_line(&parser, line);
        line_len = getline(&line, &line_capacity, file);
    }

    // getline also returns -1 on a read error
    if (status == 0 && ferror(file))
    {
        perror("minimake: cannot read the makefile");
        status = -1;
    }

    // getline reuses the same memory, so only one free is needed
    free(line);
    return status;
}

// the message depends on whether a target was asked
static void no_makefile_error(const struct options *options)
{
    if (options->nb_targets == 0)
        fprintf(stderr,
                "minimake: *** No targets specified and no makefile found. "
                "Stop.\n");
    else
        fprintf(stderr, "minimake: *** No rule to make target '%s'. Stop.\n",
                options->targets[0]);
}

// open the makefile to read, return NULL after printing the error
static FILE *open_makefile(const struct options *options)
{
    FILE *file = NULL;

    // the file given with -f has priority
    if (options->makefile_path)
    {
        file = fopen(options->makefile_path, "r");
        if (!file)
            fprintf(stderr, "minimake: %s: %s\n", options->makefile_path,
                    strerror(errno));
        return file;
    }

    // without -f we try makefile first, then Makefile
    file = fopen("makefile", "r");
    if (!file)
        file = fopen("Makefile", "r");

    if (!file)
        no_makefile_error(options);

    return file;
}

int makefile_load(struct makefile *makefile, const struct options *options)
{
    FILE *file = open_makefile(options);
    if (!file)
        return -1;

    int status = parse_lines(makefile, file);

    if (fclose(file) == EOF)
    {
        perror("minimake: cannot close the makefile");
        status = -1;
    }

    return status;
}

// print str between quotes with only one space between the words
static void print_quoted(const char *str)
{
    char *clean = single_spaces(str);

    printf("'%s'", clean);
    free(clean);
}

// print the target, its dependencies, then its commands
static void print_rule(const struct rule *rule)
{
    struct list *curr = rule->deps->head;

    printf("(%s):", rule->target);
    while (curr)
    {
        // cast: data is a void* and printf needs a char*
        printf(" [%s]", (char *)curr->data);
        curr = curr->next;
    }
    printf("\n");

    // same thing for the commands, one per line
    curr = rule->commands->head;
    while (curr)
    {
        print_quoted(curr->data);
        printf("\n");
        curr = curr->next;
    }
}

void makefile_print(const struct makefile *makefile)
{
    struct list *curr = makefile->vars->head;

    // variables first, in the order of the makefile
    printf("# variables\n");
    while (curr)
    {
        struct var *var = curr->data;

        printf("'%s' = ", var->name);
        print_quoted(var->value);
        printf("\n");
        curr = curr->next;
    }

    // then the rules, also in the order of the makefile
    printf("# rules\n");
    curr = makefile->rules->head;
    while (curr)
    {
        print_rule(curr->data);
        curr = curr->next;
    }
}
