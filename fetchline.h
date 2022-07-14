#ifndef FETCHLINE_H
#define FETCHLINE_H

#include <stddef.h>
#include <sys/types.h>
#include <termios.h>

#include "history.h"

#define FL_RET_EMPTY 0
#define FL_RET_INTERRUPT -1
#define FL_RET_MEM_FAIL -2
#define FL_RET_SYS_FAIL -3
#define FL_RET_EOF -4

typedef struct fetchline_ctx
{
    history_t hist;
    struct termios old_opts;
} fetchline_ctx_t;

int fetchline_ctx_init(fetchline_ctx_t *context);

void fetchline_ctx_free(fetchline_ctx_t *context);

ssize_t fetchline(fetchline_ctx_t *context, const char *prompt, char **buffer, size_t *buflen);

#endif