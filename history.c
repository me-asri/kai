#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "history.h"

#define HIST_TAIL_NONE -1
#define HIST_IDX_END -2
#define HIST_IDX_NONE -1

static int copy_line(history_t *hist, size_t index, const char *line, size_t len);
static ssize_t copy_to_buf(char **dest, size_t *destlen, const char *src);

void history_init(history_t *hist)
{
    size_t i;

    for (i = 0; i < MAX_HIST_ENTRIES; i++)
    {
        hist->entries[i] = NULL;
        hist->entries_len[i] = 0;
    }
    hist->tail = HIST_TAIL_NONE;
    hist->index = HIST_IDX_NONE;
    hist->last_index = HIST_IDX_NONE;
}

void history_free(history_t *hist)
{
    size_t i;

    for (i = 0; i < MAX_HIST_ENTRIES; i++)
    {
        if (hist->entries[i])
            free(hist->entries[i]);
        hist->entries_len[i] = 0;
    }
}

ssize_t history_add(history_t *hist, const char *line)
{
    ssize_t len;
    size_t next_idx;

    // Don't repeat the same entry
    if (hist->last_index >= 0 && strcmp(hist->entries[hist->last_index], line) == 0)
    {
        hist->index = hist->tail;
        hist->last_index = HIST_IDX_NONE;

        return 0;
    }

    len = strlen(line);
    if (len == 0)
        return 0;

    next_idx = (hist->tail + 1) % MAX_HIST_ENTRIES;
    if (copy_line(hist, next_idx, line, len) < 0)
        return -1;

    hist->tail = next_idx;
    hist->index = hist->tail;
    hist->last_index = HIST_IDX_NONE;

    return len;
}

ssize_t history_get_prev(history_t *hist, char **buf, size_t *buflen)
{
    ssize_t slen;
    ssize_t next_idx;

    if (hist->tail == HIST_TAIL_NONE)
        return 0; // Empty history
    if (hist->index == HIST_IDX_END)
        return 0; // No more entries

    slen = copy_to_buf(buf, buflen, hist->entries[hist->index]);
    if (slen < 0)
        return -1;

    hist->last_index = hist->index; // Preserve last index

    next_idx = (MAX_HIST_ENTRIES + hist->index - 1) % MAX_HIST_ENTRIES;
    if (next_idx == hist->tail || hist->entries[next_idx] == NULL)
    {
        hist->index = HIST_IDX_END; // No more entries
    }
    else
    {
        hist->index = next_idx;
    }

    return slen;
}

ssize_t history_peek_last(history_t *hist, char **buf, size_t *buflen)
{
    ssize_t slen;

    if (hist->tail == HIST_TAIL_NONE)
        return 0; // Empty history

    slen = copy_to_buf(buf, buflen, hist->entries[hist->tail]);
    if (slen < 0)
        return -1;

    return slen;
}

ssize_t history_get_next(history_t *hist, char **buf, size_t *buflen)
{
    ssize_t slen;
    ssize_t next_idx;

    if (hist->tail == HIST_TAIL_NONE)
        return 0;
    if (hist->last_index == HIST_IDX_NONE)
        return 0;

    next_idx = (hist->last_index + 1) % MAX_HIST_ENTRIES;

    if (next_idx == (hist->tail + 1) % MAX_HIST_ENTRIES || hist->entries[next_idx] == NULL)
    {
        // Reached beginning
        hist->index = hist->tail;
        hist->last_index = HIST_IDX_NONE;
        return 0;
    }

    slen = copy_to_buf(buf, buflen, hist->entries[next_idx]);
    if (slen < 0)
        return -1;

    hist->index = hist->last_index;
    hist->last_index = next_idx;
    return slen;
}

int copy_line(history_t *hist, size_t index, const char *line, size_t len)
{
    char *new_buf;

    len++; // + '\0'

    if (hist->entries_len[index] < len)
    {
        new_buf = realloc(hist->entries[index], len * sizeof(char));
        if (!new_buf)
            return -1;
        hist->entries[index] = new_buf;
        hist->entries_len[index] = len;
    }

    strcpy(hist->entries[index], line);
    return 0;
}

ssize_t copy_to_buf(char **dest, size_t *destlen, const char *src)
{
    size_t slen;
    char *new_buf;

    slen = strlen(src) + 1; // + '\0'
    if (*destlen < slen)
    {
        new_buf = realloc(*dest, slen);
        if (!new_buf)
            return -1;

        *dest = new_buf;
        *destlen = slen;
    }

    strcpy(*dest, src);
    return slen - 1;
}
