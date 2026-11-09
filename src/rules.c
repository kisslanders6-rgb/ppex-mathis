/*
 * src/rules.c
 *
 * rules of the makefile: creation and search
 *
 * see src/rules.h
 */

#include "rules.h"

#include <stdlib.h>
#include <string.h>

#include "safe_memory.h"

struct rule *rule_create(struct fifo *rules, const char *target)
{
    struct rule *rule = safe_malloc(sizeof(struct rule));
    rule->target = safe_strdup(target);
    rule->deps = fifo_init();
    rule->commands = fifo_init();
    // nothing is built at the start
    rule->state = STATE_NOT_VISITED;
    fifo_push(rules, rule);

    return rule;
}

struct rule *rule_find(const struct fifo *rules, const char *target)
{
    struct list *curr = rules->head;

    while (curr)
    {
        struct rule *rule = curr->data;
        if (strcmp(rule->target, target) == 0)
            return rule;

        curr = curr->next;
    }

    return NULL;
}

struct rule *rule_find_default(const struct fifo *rules)
{
    struct list *curr = rules->head;

    while (curr)
    {
        struct rule *rule = curr->data;
        // a rule with a % is never the default target
        if (!strchr(rule->target, '%'))
            return rule;

        curr = curr->next;
    }

    return NULL;
}

void rule_destroy(void *to_destroy)
{
    // void* -> struct rule*
    struct rule *rule = to_destroy;

    free(rule->target);
    fifo_destroy(rule->deps, free);
    fifo_destroy(rule->commands, free);
    free(rule);
}
