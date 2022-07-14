#ifndef HISTORY_H
#define HISTORY_H

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#define MAX_HIST_ENTRIES 10

typedef struct history
{
    char *entries[MAX_HIST_ENTRIES];
    ssize_t entries_len[MAX_HIST_ENTRIES];

    ssize_t tail;
    ssize_t index;
    ssize_t last_index;
} history_t;

void history_init(history_t *hist);

void history_free(history_t *hist);

ssize_t history_add(history_t *hist, const char *line);

ssize_t history_get_prev(history_t *hist, char **buf, size_t *buflen);

ssize_t history_peek_last(history_t *hist, char **buf, size_t *buflen);

ssize_t history_get_next(history_t *hist, char **buf, size_t *buflen);

#endif