/*
 * tests/functional_tests.c
 *
 * functional tests: each test runs the real minimake binary
 */

#include <criterion/criterion.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "functional_test_helpers.h"

// options and choice of the makefile
TestSuite(command_line, .init = create_and_enter_test_directory,
          .fini = leave_and_delete_test_directory);

Test(command_line, short_file_option_reads_given_file)
{
    write_text_file("custom", "all:\n\t@echo from custom\n");
    struct minimake_result result = run_minimake(ARGUMENTS("-f", "custom"));

    cr_assert_eq(result.exit_code, 0);
    cr_assert_str_eq(result.standard_output, "from custom\n");
}

Test(command_line, long_file_options_read_given_file)
{
    write_text_file("custom", "all:\n\t@echo from custom\n");
    struct minimake_result file_result =
        run_minimake(ARGUMENTS("--file=custom"));
    struct minimake_result makefile_result =
        run_minimake(ARGUMENTS("--makefile=custom"));

    cr_assert_str_eq(file_result.standard_output, "from custom\n");
    cr_assert_str_eq(makefile_result.standard_output, "from custom\n");
}

Test(command_line, last_file_option_wins)
{
    write_text_file("one", "all:\n\t@echo one\n");
    write_text_file("two", "all:\n\t@echo two\n");
    struct minimake_result result =
        run_minimake(ARGUMENTS("-f", "one", "-f", "two"));

    cr_assert_str_eq(result.standard_output, "two\n");
}

Test(command_line, options_can_be_after_the_target)
{
    write_text_file("file", "all:\n\t@echo ok\n");
    struct minimake_result result =
        run_minimake(ARGUMENTS("all", "-f", "file"));

    cr_assert_str_eq(result.standard_output, "ok\n");
}

Test(command_line, missing_file_returns_two_with_message)
{
    struct minimake_result result = run_minimake(ARGUMENTS("-f", "nothing"));

    cr_assert_eq(result.exit_code, 2);
    cr_assert_neq(strlen(result.standard_error), 0);
}

Test(command_line, help_prints_on_stdout_and_returns_zero)
{
    struct minimake_result result = run_minimake(ARGUMENTS("--help"));

    cr_assert_eq(result.exit_code, 0);
    cr_assert_neq(strlen(result.standard_output), 0);
}

Test(command_line, help_has_priority_and_builds_nothing)
{
    struct minimake_result result = run_minimake_on_makefile(
        "all:\n\t@touch created\n", ARGUMENTS("all", "-h"));

    cr_assert_eq(result.exit_code, 0);
    cr_assert_eq(access("created", F_OK), -1);
}

Test(command_line, unknown_option_prints_help_and_returns_two)
{
    struct minimake_result result = run_minimake(ARGUMENTS("--wrong"));

    cr_assert_eq(result.exit_code, 2);
    cr_assert_not_null(strstr(result.standard_output, "Usage"));
}

Test(command_line, empty_argument_has_exact_message)
{
    struct minimake_result result = run_minimake(ARGUMENTS("all", ""));

    cr_assert_eq(result.exit_code, 2);
    cr_assert_str_eq(result.standard_error,
                     "minimake: *** empty string invalid as argument. Stop.\n");
}

Test(command_line, lowercase_makefile_is_found_without_option)
{
    write_text_file("makefile", "all:\n\t@echo lower\n");
    struct minimake_result result = run_minimake(NO_ARGUMENTS);

    cr_assert_str_eq(result.standard_output, "lower\n");
}

Test(command_line, capitalized_makefile_is_found_without_option)
{
    write_text_file("Makefile", "all:\n\t@echo upper\n");
    struct minimake_result result = run_minimake(NO_ARGUMENTS);

    cr_assert_str_eq(result.standard_output, "upper\n");
}

Test(command_line, no_makefile_without_target_has_exact_message)
{
    struct minimake_result result = run_minimake(NO_ARGUMENTS);

    cr_assert_eq(result.exit_code, 2);
    cr_assert_str_eq(
        result.standard_error,
        "minimake: *** No targets specified and no makefile found. Stop.\n");
}

// which targets are built and when
TestSuite(building, .init = create_and_enter_test_directory,
          .fini = leave_and_delete_test_directory);

Test(building, first_standard_target_is_the_default)
{
    struct minimake_result result = run_minimake_on_makefile(
        "%.o: %.c\n\t@echo pattern\nfirst:\n\t@echo first\nsecond:\n",
        NO_ARGUMENTS);

    cr_assert_str_eq(result.standard_output, "first\n");
}

Test(building, only_pattern_rules_means_no_targets)
{
    struct minimake_result result =
        run_minimake_on_makefile("%.o: %.c\n\techo a\n", NO_ARGUMENTS);

    cr_assert_eq(result.exit_code, 2);
    cr_assert_str_eq(result.standard_error,
                     "minimake: *** No targets. Stop.\n");
}

Test(building, unknown_target_has_exact_message)
{
    struct minimake_result result =
        run_minimake_on_makefile("%.o: %.c\n", ARGUMENTS("toto"));

    cr_assert_eq(result.exit_code, 2);
    cr_assert_str_eq(result.standard_error,
                     "minimake: *** No rule to make target 'toto'. Stop.\n");
}

Test(building, missing_dependency_names_the_parent)
{
    struct minimake_result result = run_minimake_on_makefile(
        "all: tutu\n\techo This is all\n", ARGUMENTS("all"));

    cr_assert_eq(result.exit_code, 2);
    cr_assert_str_eq(
        result.standard_error,
        "minimake: *** No rule to make target 'tutu', needed by 'all'. "
        "Stop.\n");
}

Test(building, targets_are_built_in_the_given_order)
{
    struct minimake_result result = run_minimake_on_makefile(
        "a:\n\t@echo a\nb:\n\t@echo b\n", ARGUMENTS("b", "a"));

    cr_assert_str_eq(result.standard_output, "b\na\n");
}

Test(building, dependencies_are_built_before_the_target)
{
    struct minimake_result result = run_minimake_on_makefile(
        "all: one two\n\t@echo all\none:\n\t@echo one\ntwo:\n\t@echo two\n",
        NO_ARGUMENTS);

    cr_assert_str_eq(result.standard_output, "one\ntwo\nall\n");
}

Test(building, shared_dependency_is_built_once)
{
    struct minimake_result result = run_minimake_on_makefile(
        "all: a b\na: c\nb: c\nc:\n\t@echo c\n", NO_ARGUMENTS);

    cr_assert_str_eq(result.standard_output, "c\n");
}

Test(building, same_target_twice_is_built_once)
{
    struct minimake_result result = run_minimake_on_makefile(
        "all:\n\techo toto\n", ARGUMENTS("all", "all"));

    cr_assert_str_eq(result.standard_output,
                     "echo toto\ntoto\nminimake: 'all' is up to date.\n");
}

Test(building, empty_rule_is_nothing_to_be_done)
{
    struct minimake_result result =
        run_minimake_on_makefile("empty:\n", ARGUMENTS("empty"));

    cr_assert_eq(result.exit_code, 0);
    cr_assert_str_eq(result.standard_output,
                     "minimake: Nothing to be done for 'empty'.\n");
}

Test(building, existing_file_with_recipe_is_up_to_date)
{
    write_text_file("toto", "");
    struct minimake_result result =
        run_minimake_on_makefile("toto:\n\t@echo run\n", ARGUMENTS("toto"));

    cr_assert_str_eq(result.standard_output,
                     "minimake: 'toto' is up to date.\n");
}

Test(building, target_newer_than_dependency_is_up_to_date)
{
    write_text_file("file.c", "");
    write_text_file("file.o", "");
    set_file_age_in_seconds("file.c", 100);
    set_file_age_in_seconds("file.o", 10);
    struct minimake_result result = run_minimake_on_makefile(
        "file.o: file.c\n\t@echo rebuilt\n", ARGUMENTS("file.o"));

    cr_assert_str_eq(result.standard_output,
                     "minimake: 'file.o' is up to date.\n");
}

Test(building, target_older_than_dependency_is_rebuilt)
{
    write_text_file("file.c", "");
    write_text_file("file.o", "");
    set_file_age_in_seconds("file.c", 10);
    set_file_age_in_seconds("file.o", 100);
    struct minimake_result result = run_minimake_on_makefile(
        "file.o: file.c\n\t@echo rebuilt\n", ARGUMENTS("file.o"));

    cr_assert_str_eq(result.standard_output, "rebuilt\n");
}

Test(building, rebuilt_dependency_forces_the_rebuild_of_its_parent)
{
    write_text_file("parent", "");
    struct minimake_result result = run_minimake_on_makefile(
        "parent: child\n\t@echo parent\nchild:\n\t@echo child\n",
        ARGUMENTS("parent"));

    cr_assert_str_eq(result.standard_output, "child\nparent\n");
}

Test(building, built_dependency_then_empty_parent_of_the_subject)
{
    struct minimake_result result = run_minimake_on_makefile(
        "all: toto\ntoto:\n\t@echo toto file does not exist\n",
        ARGUMENTS("toto", "all"));

    cr_assert_str_eq(result.standard_output,
                     "toto file does not exist\n"
                     "minimake: Nothing to be done for 'all'.\n");
}

Test(building, real_c_project_of_the_subject_is_compiled_then_up_to_date)
{
    write_text_file("foo.c", "int main(void) { return 0; }\n");
    set_file_age_in_seconds("foo.c", 100);
    const char *text = "NEEDED = foo\nall: $(NEEDED)\n"
                       "foo: foo.o\n\tgcc -o foo foo.o\n"
                       "foo.o: foo.c\n\tgcc -c foo.c\n";

    struct minimake_result first = run_minimake_on_makefile(text, NO_ARGUMENTS);
    struct minimake_result second =
        run_minimake_on_makefile(text, NO_ARGUMENTS);

    cr_assert_eq(first.exit_code, 0);
    cr_assert_str_eq(first.standard_output, "gcc -c foo.c\ngcc -o foo foo.o\n");
    cr_assert_eq(access("foo", X_OK), 0);
    cr_assert_str_eq(second.standard_output,
                     "minimake: Nothing to be done for 'all'.\n");
}

// how the commands are run and logged
TestSuite(execution, .init = create_and_enter_test_directory,
          .fini = leave_and_delete_test_directory);

Test(execution, commands_are_logged_before_their_output)
{
    struct minimake_result result = run_minimake_on_makefile(
        "all:\n\techo toto\n\techo foo\n", NO_ARGUMENTS);

    cr_assert_str_eq(result.standard_output,
                     "echo toto\ntoto\necho foo\nfoo\n");
}

Test(execution, blanks_before_the_command_are_not_logged)
{
    struct minimake_result result =
        run_minimake_on_makefile("all:\n\t  \t echo a\n", NO_ARGUMENTS);

    cr_assert_str_eq(result.standard_output, "echo a\na\n");
}

Test(execution, at_sign_hides_the_command)
{
    struct minimake_result result =
        run_minimake_on_makefile("all:\n\t@echo a line\n", NO_ARGUMENTS);

    cr_assert_str_eq(result.standard_output, "a line\n");
}

Test(execution, each_command_has_its_own_shell)
{
    struct minimake_result result = run_minimake_on_makefile(
        "all:\n\t@A=42\n\t@echo \"-$$A-\"\n\t@A=42; echo \"-$$A-\"\n",
        NO_ARGUMENTS);

    cr_assert_str_eq(result.standard_output, "--\n-42-\n");
}

Test(execution, failing_command_stops_the_recipe_and_returns_two)
{
    struct minimake_result result = run_minimake_on_makefile(
        "all:\n\tfalse\n\techo will not appear\n", NO_ARGUMENTS);

    cr_assert_eq(result.exit_code, 2);
    cr_assert_str_eq(result.standard_output, "false\n");
    cr_assert_neq(strlen(result.standard_error), 0);
}

Test(execution, unknown_command_returns_two)
{
    struct minimake_result result = run_minimake_on_makefile(
        "all:\n\t@notACommand\n\t@echo no\n", NO_ARGUMENTS);

    cr_assert_eq(result.exit_code, 2);
    cr_assert_str_eq(result.standard_output, "");
}

// variables in real makefiles
TestSuite(variables, .init = create_and_enter_test_directory,
          .fini = leave_and_delete_test_directory);

Test(variables, parentheses_braces_and_one_letter_syntaxes_are_equal)
{
    struct minimake_result result = run_minimake_on_makefile(
        "V=x\nall:\n\t@echo $(V) ${V} $V\n", NO_ARGUMENTS);

    cr_assert_str_eq(result.standard_output, "x x x\n");
}

Test(variables, undefined_variable_is_empty)
{
    struct minimake_result result =
        run_minimake_on_makefile("all:\n\t@echo a$(NOPE)b\n", NO_ARGUMENTS);

    cr_assert_eq(result.exit_code, 0);
    cr_assert_str_eq(result.standard_output, "ab\n");
}

Test(variables, double_dollar_gives_a_dollar)
{
    struct minimake_result result =
        run_minimake_on_makefile("all:\n\techo $$\n", NO_ARGUMENTS);

    cr_assert_str_eq(result.standard_output, "echo $\n$\n");
}

Test(variables, variable_in_a_variable_name)
{
    struct minimake_result result = run_minimake_on_makefile(
        "all:\n\t@echo $(FOO)\nVAR=FOO\n$(VAR)=Hello, World!\n", NO_ARGUMENTS);

    cr_assert_str_eq(result.standard_output, "Hello, World!\n");
}

Test(variables, recipe_can_use_a_variable_defined_later)
{
    struct minimake_result result = run_minimake_on_makefile(
        "all:\n\t@echo The answer is ${FOO}\nFOO = 42\n", NO_ARGUMENTS);

    cr_assert_str_eq(result.standard_output, "The answer is 42\n");
}

Test(variables, dependency_is_expanded_at_parsing)
{
    struct minimake_result result = run_minimake_on_makefile(
        "all: $(FILE)\n\t@echo all\nFILE=missing\n", NO_ARGUMENTS);

    cr_assert_eq(result.exit_code, 0);
    cr_assert_str_eq(result.standard_output, "all\n");
}

Test(variables, reference_is_replaced_between_single_quotes)
{
    struct minimake_result result =
        run_minimake_on_makefile("V=x\nall:\n\t@echo '$(V)'\n", NO_ARGUMENTS);

    cr_assert_str_eq(result.standard_output, "x\n");
}

Test(variables, unterminated_reference_in_recipe_returns_two)
{
    struct minimake_result result = run_minimake_on_makefile(
        "VAR=42\nall:\n\techo The answer is $(VAR\n", NO_ARGUMENTS);

    cr_assert_eq(result.exit_code, 2);
    cr_assert_neq(strlen(result.standard_error), 0);
}

// option -p and syntax errors
TestSuite(pretty_print, .init = create_and_enter_test_directory,
          .fini = leave_and_delete_test_directory);

Test(pretty_print, syntax_test_file_of_the_subject_matches_exactly)
{
    char *path = get_path_in_project("tests/Makefile.syntax-test");
    struct minimake_result result = run_minimake(ARGUMENTS("-p", "-f", path));

    cr_assert_eq(result.exit_code, 0);
    cr_assert_str_eq(result.standard_output,
                     "# variables\n"
                     "'SIMPLE_VAR' = 'coucou'\n"
                     "'SIMPLE_VAR_COMMENT' = 'the comment is gone '\n"
                     "'SPACES_BEFORE_TAB' = 'var_beginning var_end'\n"
                     "'B' = 'B_var_beginning B_var_end'\n"
                     "# rules\n"
                     "(sparse_rule): [depa] [depb]\n"
                     "'command 1'\n"
                     "'command 2'\n"
                     "(packed_rule): [depa] [depb]\n"
                     "'command 1'\n"
                     "'command 2'\n"
                     "(silent_rule): [depa] [depb]\n"
                     "'@ command 1'\n"
                     "'@command 2'\n"
                     "(rule_comment): [depa] [depb]\n"
                     "(command_space_rule): [depa] [depb]\n"
                     "'echo spaces before'\n"
                     "'echo spaces after '\n"
                     "'echo this is a # comment'\n"
                     "(simple_rule): [simple_dep]\n"
                     "(no_dep_rule):\n"
                     "(variable_rule): [beginning] [coucou] [end]\n"
                     "'echo \"should not be expanded: $(SIMPLE_VAR)\"'\n");
    free(path);
}

Test(pretty_print, no_rule_is_executed)
{
    struct minimake_result result =
        run_minimake_on_makefile("all:\n\ttouch created\n", ARGUMENTS("-p"));

    cr_assert_eq(result.exit_code, 0);
    cr_assert_eq(access("created", F_OK), -1);
}

Test(pretty_print, line_without_separator_is_an_error)
{
    struct minimake_result result =
        run_minimake_on_makefile("this is not valid\n", ARGUMENTS("-p"));

    cr_assert_eq(result.exit_code, 2);
}

Test(pretty_print, tab_outside_of_a_rule_is_an_error)
{
    struct minimake_result result =
        run_minimake_on_makefile("\techo a\nall:\n", ARGUMENTS("-p"));

    cr_assert_eq(result.exit_code, 2);
}

Test(pretty_print, comment_after_a_rule_is_removed)
{
    struct minimake_result result =
        run_minimake_on_makefile("all: dep # comment\n", ARGUMENTS("-p"));

    cr_assert_str_eq(result.standard_output,
                     "# variables\n# rules\n(all): [dep]\n");
}
