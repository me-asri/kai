#ifndef BUILTIN_H
#define BUILTIN_H

#include "parser.h"
#include "eval.h"
#include "kai.h"

int eval_builtin(command_t *cmd, eval_res_t *result, kai_ctx_t *kai_ctx);

#endif