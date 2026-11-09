/*
 * tests/unit_tests.c
 *
 * unit tests: each test calls the functions of the project directly
 */

#define _POSIX_C_SOURCE 200809L

#include <criterion/criterion.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "command_line.h"
#include "fifo.h"
#include "parser.h"
#include "rules.h"
#include "text_tools.h"
#include "variables.h"

// for the queues that only contain string literals
static void no_free(void *data)
{
    (void)data;
}

// put the text in a real stream and parse it, return like parse_lines
static int parse_text(struct makefile *makefile, const char *text)
{
    FILE *stream = tmpfile();
    cr_assert_not_null(stream);
    cr_assert_geq(fputs(text, stream), 0);
    rewind(stream);
    makefile_init(makefile);

    int result = parse_lines(makefile, stream);

    fclose(stream);
    return result;
}

// tests of the queue

Test(fifo, new_fifo_is_empty)
{
    struct fifo *fifo = fifo_init();

    cr_assert_null(fifo->head);
    cr_assert_null(fifo->tail);
    cr_assert_eq(fifo->size, 0);
    fifo_destroy(fifo, no_free);
}

Test(fifo, push_keeps_insertion_order)
{
    struct fifo *fifo = fifo_init();
    char *values[] = { "first", "second", "third" };

    for (int i = 0; i < 3; i++)
        fifo_push(fifo, values[i]);

    struct list *curr = fifo->head;

    for (int i = 0; i < 3; i++)
    {
        cr_assert_str_eq(curr->data, values[i]);
        curr = curr->next;
    }

    cr_assert_null(curr);
    cr_assert_eq(fifo->size, 3);
    fifo_destroy(fifo, no_free);
}

Test(fifo, tail_is_the_last_pushed_element)
{
    struct fifo *fifo = fifo_init();

    fifo_push(fifo, "first");
    fifo_push(fifo, "last");
    cr_assert_str_eq(fifo->tail->data, "last");
    fifo_destroy(fifo, no_free);
}

// tests of the string functions

Test(text_tools, split_words_on_any_blanks)
{
    struct fifo *words = fifo_init();

    split_words("  one \t two   three ", words);
    cr_assert_eq(words->size, 3);
    cr_assert_str_eq(words->head->data, "one");
    cr_assert_str_eq(words->head->next->data, "two");
    cr_assert_str_eq(words->tail->data, "three");
    fifo_destroy(words, free);
}

Test(text_tools, split_words_on_blank_string_gives_nothing)
{
    struct fifo *words = fifo_init();

    split_words(" \t ", words);
    cr_assert_eq(words->size, 0);
    fifo_destroy(words, free);
}

Test(text_tools, single_spaces_merges_blanks_and_keeps_last_one)
{
    char *result = single_spaces(" \ta \t b  ");

    cr_assert_str_eq(result, "a b ");
    free(result);
}

Test(text_tools, buffer_grows_past_its_first_size)
{
    struct buffer buf;

    buffer_init(&buf);
    for (int i = 0; i < 10000; i++)
        buffer_add_char(&buf, 'x');

    cr_assert_eq(strlen(buf.text), 10000);
    free(buf.text);
}

// tests of the variables and of the expansion

Test(variables_unit, set_then_find_gives_the_value)
{
    struct fifo *vars = fifo_init();

    var_set(vars, "NAME", "value");
    cr_assert_str_eq(var_find(vars, "NAME")->value, "value");
    cr_assert_null(var_find(vars, "OTHER"));
    fifo_destroy(vars, var_destroy);
}

Test(variables_unit, setting_twice_replaces_the_value)
{
    struct fifo *vars = fifo_init();

    var_set(vars, "NAME", "old");
    var_set(vars, "NAME", "new");
    cr_assert_eq(vars->size, 1);
    cr_assert_str_eq(var_find(vars, "NAME")->value, "new");
    fifo_destroy(vars, var_destroy);
}

Test(variables_unit, expand_replaces_the_three_syntaxes)
{
    struct fifo *vars = fifo_init();

    var_set(vars, "V", "x");

    char *result = expand_text(vars, "$(V) ${V} $V");

    cr_assert_str_eq(result, "x x x");
    free(result);
    fifo_destroy(vars, var_destroy);
}

Test(variables_unit, unterminated_reference_returns_null)
{
    struct fifo *vars = fifo_init();

    cr_assert_null(expand_text(vars, "$(NAME"));
    cr_assert_null(expand_text(vars, "${NAME)"));
    fifo_destroy(vars, var_destroy);
}

Test(variables_unit, double_dollar_gives_one_dollar)
{
    struct fifo *vars = fifo_init();
    char *result = expand_text(vars, "a$$b");

    cr_assert_str_eq(result, "a$b");
    free(result);
    fifo_destroy(vars, var_destroy);
}

// tests of the rules

Test(rules, find_returns_the_exact_target)
{
    struct fifo *rules = fifo_init();

    rule_create(rules, "first");

    struct rule *second = rule_create(rules, "second");

    cr_assert_eq(rule_find(rules, "second"), second);
    cr_assert_null(rule_find(rules, "third"));
    fifo_destroy(rules, rule_destroy);
}

Test(rules, default_rule_skips_patterns)
{
    struct fifo *rules = fifo_init();

    rule_create(rules, "%.o");

    struct rule *expected = rule_create(rules, "all");

    rule_create(rules, "other");
    cr_assert_eq(rule_find_default(rules), expected);
    fifo_destroy(rules, rule_destroy);
}

Test(rules, default_rule_is_null_with_only_patterns)
{
    struct fifo *rules = fifo_init();

    rule_create(rules, "%.o");
    cr_assert_null(rule_find_default(rules));
    fifo_destroy(rules, rule_destroy);
}

// tests of the parser

Test(parser, variable_value_has_no_leading_blanks)
{
    struct makefile makefile;

    cr_assert_eq(parse_text(&makefile, "NAME =  \t value \n"), 0);
    cr_assert_str_eq(var_find(makefile.vars, "NAME")->value, "value ");
    makefile_destroy(&makefile);
}

Test(parser, rule_has_its_dependencies)
{
    struct makefile makefile;

    cr_assert_eq(parse_text(&makefile, "target: one two\n"), 0);

    struct rule *rule = rule_find(makefile.rules, "target");

    cr_assert_not_null(rule);
    cr_assert_eq(rule->deps->size, 2);
    makefile_destroy(&makefile);
}

Test(parser, command_has_no_leading_blanks)
{
    struct makefile makefile;

    cr_assert_eq(parse_text(&makefile, "target:\n\t  echo a # b\n"), 0);

    struct rule *rule = rule_find(makefile.rules, "target");

    cr_assert_str_eq(rule->commands->head->data, "echo a # b");
    makefile_destroy(&makefile);
}

Test(parser, empty_target_is_ignored_with_its_recipe)
{
    struct makefile makefile;

    cr_assert_eq(parse_text(&makefile, ": dep\n\techo a\n"), 0);
    cr_assert_eq(makefile.rules->size, 0);
    makefile_destroy(&makefile);
}

// tests of the command line

Test(command_line_unit, targets_and_options_can_be_mixed)
{
    char *args[] = { "minimake", "one", "-p", "two", "-f", "file", NULL };
    struct options options;

    cr_assert_eq(parse_args(6, args, &options), PARSE_CONTINUE);
    cr_assert_eq(options.nb_targets, 2);
    cr_assert_str_eq(options.targets[0], "one");
    cr_assert_str_eq(options.targets[1], "two");
    cr_assert_str_eq(options.makefile_path, "file");
    cr_assert(options.pretty_print);
    free_options(&options);
}

Test(command_line_unit, value_of_file_option_is_not_a_target)
{
    char *args[] = { "minimake", "--file=one", NULL };
    struct options options;

    parse_args(2, args, &options);
    cr_assert_str_eq(options.makefile_path, "one");
    cr_assert_eq(options.nb_targets, 0);
    free_options(&options);
}

Test(command_line_unit, dash_f_takes_next_argument_even_if_option)
{
    char *args[] = { "minimake", "-f", "-h", NULL };
    struct options options;

    cr_assert_eq(parse_args(3, args, &options), PARSE_CONTINUE);
    cr_assert_str_eq(options.makefile_path, "-h");
    cr_assert_not(options.help);
    free_options(&options);
}
