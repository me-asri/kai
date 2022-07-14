#ifndef EVAL_H
#define EVAL_H

#include "kai.h"

#define EVAL_STATUS_OK 1
#define EVAL_STATUS_NO_EXEC 0
#define EVAL_STATUS_FAIL -1

typedef struct eval_res {
    int status;
    char const *err_msg;

    int bg_pid;
} eval_res_t;

void eval(eval_res_t *result, const char *input, kai_ctx_t *kai_ctx);

#endif