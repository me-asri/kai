#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <limits.h>
#include <unistd.h>
#include <errno.h>

#include "builtin.h"

static const char ERR_TOO_MANY_ARGS[] = "Too many arguments";
static const char ERR_NOT_ENOUGH_ARGS[] = "Not enough arguments";
static const char ERR_NUM_ARG_REQ[] = "Numeric argument required";
static const char ERR_PATH_TOO_BIG[] = "Path length exceeds max limit";
static const char ERR_NO_HOME[] = "Failed to determine home directory";

static const char HELP_MSG[] = "kai shell\n"
                               "Shell commands below are defined internally:\n\n"
                               " - cd <directory> : Change the current working directory\n"
                               "    (if directory is omitted, user's home directory is chosen)\n"
                               " - exec [cmd] : Replace shell with the given command\n"
                               " - set [var] [value] : Set environment variable\n"
                               " - get [var] : Get environment variable\n"
                               " - exit <status> : Exit from shell\n"
                               "    (if status is omitted, 0 is used)";

static int cd(command_t *cmd, eval_res_t *result);

static int exec(command_t *cmd, eval_res_t *result);

static int set(command_t *cmd, eval_res_t *result);

static int get(command_t *cmd, eval_res_t *result);

static int b_exit(command_t *cmd, eval_res_t *result, kai_ctx_t *kai_ctx);

static int help(command_t *cmd, eval_res_t *result);

int eval_builtin(command_t *cmd, eval_res_t *result, kai_ctx_t *kai_ctx)
{
    result->bg_pid = -1;

    if (strcmp(cmd->argv[0], "cd") == 0)
        return cd(cmd, result);
    if (strcmp(cmd->argv[0], "exec") == 0)
        return exec(cmd, result);
    if (strcmp(cmd->argv[0], "set") == 0)
        return set(cmd, result);
    if (strcmp(cmd->argv[0], "get") == 0)
        return get(cmd, result);
    if (strcmp(cmd->argv[0], "exit") == 0)
        return b_exit(cmd, result, kai_ctx);
    if (strcmp(cmd->argv[0], "help") == 0)
        return help(cmd, result);

    return 0;
}

int cd(command_t *cmd, eval_res_t *result)
{
    char path[PATH_MAX];
    size_t plen, arglen;
    char *homedir;

    int ret;

    if (cmd->argc > 2)
    {
        result->status = -1;
        result->err_msg = ERR_TOO_MANY_ARGS;

        return -1;
    }

    if (cmd->argc == 1)
    {
        homedir = getenv("HOME");
        if (!homedir)
        {
            result->status = -1;
            result->err_msg = ERR_NO_HOME;

            return -1;
        }

        ret = chdir(homedir);
    }
    else if (cmd->argv[1][0] == '/')
    {
        ret = chdir(cmd->argv[1]);
    }
    else
    {
        if (!getcwd(path, sizeof(path)))
        {
            result->status = -1;
            result->err_msg = strerror(errno);

            return -1;
        }

        plen = strlen(path);
        arglen = strlen(cmd->argv[1]);

        if (plen + arglen + 2 > sizeof(path))
        {
            result->status = -1;
            result->err_msg = ERR_PATH_TOO_BIG;

            return -1;
        }

        strcat(path, "/");
        strcat(path, cmd->argv[1]);

        ret = chdir(path);
    }

    if (ret < 0)
    {
        result->status = -1;
        result->err_msg = strerror(errno);

        return -1;
    }

    result->status = 1;
    result->err_msg = NULL;

    return 1;
}

int exec(command_t *cmd, eval_res_t *result)
{
    if (cmd->argc == 1)
    {
        result->status = -1;
        result->err_msg = ERR_NOT_ENOUGH_ARGS;

        return -1;
    }

    execvp(cmd->argv[1], &cmd->argv[1]);

    result->status = -1;
    result->err_msg = strerror(errno);

    return -1;
}

int set(command_t *cmd, eval_res_t *result)
{
    if (cmd->argc < 3)
    {
        result->status = -1;
        result->err_msg = ERR_NOT_ENOUGH_ARGS;

        return -1;
    }
    if (cmd->argc > 3)
    {
        result->status = -1;
        result->err_msg = ERR_TOO_MANY_ARGS;

        return -1;
    }

    if (setenv(cmd->argv[1], cmd->argv[2], 1) != 0)
    {
        result->status = -1;
        result->err_msg = strerror(errno);

        return -1;
    }

    result->status = 1;
    result->err_msg = NULL;

    return 1;
}

int get(command_t *cmd, eval_res_t *result)
{
    char *val;

    if (cmd->argc < 2)
    {
        result->status = -1;
        result->err_msg = ERR_NOT_ENOUGH_ARGS;

        return -1;
    }
    if (cmd->argc > 2)
    {
        result->status = -1;
        result->err_msg = ERR_TOO_MANY_ARGS;

        return -1;
    }

    val = getenv(cmd->argv[1]);
    if (val)
    {
        puts(val);
    }
    else if (errno != 0)
    {
        result->status = -1;
        result->err_msg = strerror(errno);

        return -1;
    }

    result->status = 1;
    result->err_msg = NULL;

    return 1;
}

static int b_exit(command_t *cmd, eval_res_t *result, kai_ctx_t *kai_ctx)
{
    int ecode;
    char *endptr;

    if (cmd->argc > 2)
    {
        result->status = -1;
        result->err_msg = ERR_TOO_MANY_ARGS;

        return -1;
    }

    if (cmd->argc == 1)
    {
        ecode = kai_ctx->exit_code = 0;
    }
    else
    {
        ecode = strtol(cmd->argv[1], &endptr, 10);
        if (*endptr != '\0')
        {
            result->status = -1;
            result->err_msg = ERR_NUM_ARG_REQ;

            return -1;
        }
    }

    kai_ctx->exit_code = ecode;
    kai_ctx->running = false;

    result->status = 0;
    result->err_msg = NULL;

    return 1;
}

int help(command_t *cmd, eval_res_t *result)
{
    puts(HELP_MSG);

    result->status = 0;
    result->err_msg = NULL;

    return 1;
}