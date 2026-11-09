/*
 * src/rules.h
 *
 * rules of the makefile: creation and search
 */

#ifndef RULES_H
#define RULES_H

#include "fifo.h"

/* what happened to a target during this run of minimake */
enum build_state
{
    STATE_NOT_VISITED, // not seen yet
    STATE_IN_PROGRESS, // we are building its dependencies
    STATE_REBUILT, // its recipe was run
    STATE_UP_TO_DATE, // nothing had to be done
    STATE_FAILED, // a command failed or a file is missing
};

struct rule
{
    char *target; // name of the target
    struct fifo *deps; // dependencies (char *), already expanded
    struct fifo *commands; // commands (char *), expanded just before running
    enum build_state state; // where the target is in this run
};

/* create an empty rule and add it at the end of rules */
struct rule *rule_create(struct fifo *rules, const char *target);

/* return the rule of this target, NULL if there is none */
struct rule *rule_find(const struct fifo *rules, const char *target);

/* return the first rule without a %, NULL if there is none */
struct rule *rule_find_default(const struct fifo *rules);

/* free a struct rule, void * lets us give it to fifo_destroy */
void rule_destroy(void *to_destroy);

#endif /* ! RULES_H */
