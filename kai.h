#ifndef KAI_H
#define KAI_H

#include <stddef.h>
#include <stdbool.h>

typedef struct kai_ctx {
    bool running;
    size_t jobs;
    int exit_code;
} kai_ctx_t;

#endif