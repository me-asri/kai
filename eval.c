#define _GNU_SOURCE

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <stdio.h>

#include "eval.h"
#include "parser.h"
#include "builtin.h"
#include "kai.h"

static const char ERR_REDIR_FILE[] = "Failed to open file for redirection";
static const char ERR_SYNTAX[] = "Invalid syntax";

static void exec(command_list_t *cmds, eval_res_t *result);
static int exec_single(command_t *cmd, int infd, int outfd, bool bg);
static int exec_multi(command_list_t *cmds);

void eval(eval_res_t *result, const char *input, kai_ctx_t *kai_ctx)
{
    command_list_t cmds;
    int ret;

    ret = parse_command_list(&cmds, input);
    if (ret < 0)
    {
        result->status = EVAL_STATUS_FAIL;
        result->err_msg = ERR_SYNTAX;
        return;
    }
    if (ret == 0)
    {
        result->status = EVAL_STATUS_NO_EXEC;
        result->err_msg = NULL;
        return;
    }

    if (cmds.count == 1)
    {
        ret = eval_builtin(&cmds.commands[0], result, kai_ctx);
        if (ret != 0)
            goto end;
    }

    exec(&cmds, result);

end:
    free_command_list(&cmds);
}

void exec(command_list_t *cmds, eval_res_t *result)
{
    int outfd, infd;

    pid_t pid;
    command_t *cmd;

    int ret;

    result->bg_pid = -1;
    result->err_msg = NULL;

    if (cmds->count == 1)
    {
        outfd = STDOUT_FILENO;
        infd = STDIN_FILENO;

        cmd = &cmds->commands[0];

        if (cmd->output_file)
        {
            outfd = open(cmd->output_file, O_CREAT | O_WRONLY, 0664);
            if (outfd < 0)
            {
                result->status = EVAL_STATUS_FAIL;
                result->err_msg = ERR_REDIR_FILE;
                return;
            }
        }
        if (cmd->input_file)
        {
            infd = open(cmd->input_file, O_RDONLY);
            if (infd < 0)
            {
                result->status = EVAL_STATUS_FAIL;
                result->err_msg = ERR_REDIR_FILE;
                return;
            }
        }

        pid = exec_single(cmd, infd, outfd, cmd->in_bg);
        if (pid < 0)
        {
            result->status = EVAL_STATUS_FAIL;
            result->err_msg = strerror(errno);

            if (infd != STDIN_FILENO)
                close(infd);
            if (outfd != STDOUT_FILENO)
                close(outfd);

            return;
        }

        result->bg_pid = (cmd->in_bg) ? pid : 0;
        result->status = EVAL_STATUS_OK;

        if (infd != STDIN_FILENO)
            close(infd);
        if (outfd != STDOUT_FILENO)
            close(outfd);

        return;
    }

    ret = exec_multi(cmds);
    if (ret == 0)
    {
        result->status = EVAL_STATUS_OK;
        return;
    }

    result->status = EVAL_STATUS_FAIL;
    if (ret == -2)
        result->err_msg = ERR_REDIR_FILE;
    else
        result->err_msg = strerror(errno);

    return;
}

int exec_single(command_t *cmd, int infd, int outfd, bool bg)
{
    pid_t fpid;
    int exec_errno;
    int pipefd[2];

    int ret;

    if (pipe2(pipefd, O_CLOEXEC) < 0)
        return -1;

    fpid = fork();
    if (fpid < 0)
    {
        return -1;
    }
    else if (fpid == 0)
    {
        // Close reading end of pipe on child
        close(pipefd[0]);

        if (dup2(outfd, STDOUT_FILENO) < 0)
        {
            close(outfd);
            goto error;
        }
        if (dup2(infd, STDIN_FILENO) < 0)
        {
            close(infd);
            goto error;
        }

        if (outfd != STDOUT_FILENO)
            close(outfd);
        if (infd != STDIN_FILENO)
            close(infd);

        execvp(cmd->argv[0], cmd->argv);

    error:
        ret = write(pipefd[1], &errno, sizeof(errno));
        close(pipefd[1]);
        exit(-1);
    }

    // Close writing end of pipe on parent
    close(pipefd[1]);

    ret = read(pipefd[0], &exec_errno, sizeof(exec_errno));
    if (ret < 0)
        return -1;
    else if (ret == 0)
        exec_errno = 0;

    close(pipefd[0]);

    if (!bg)
    {
        ret = waitpid(fpid, NULL, 0);
        if (ret < 0)
            return -1;
    }

    if (exec_errno != 0)
    {
        errno = exec_errno;
        return -1;
    }

    return fpid;
}

int exec_multi(command_list_t *cmds)
{
    int pipes[2];
    int infd, outfd;
    int in_file_fd = -1, out_file_fd = -1;
    size_t i;

    int ret;
    size_t spawned = 0;

    if (cmds->count < 2)
        return -1;

    if (cmds->commands[0].input_file)
    {
        in_file_fd = open(cmds->commands[0].input_file, O_RDONLY);
        if (in_file_fd < 0)
            return -2;

        infd = in_file_fd;
    }
    else
    {
        infd = STDIN_FILENO;
    }
    if (cmds->commands[cmds->count - 1].output_file)
    {
        out_file_fd = open(cmds->commands[cmds->count - 1].output_file, O_CREAT | O_WRONLY, 0664);
        if (out_file_fd < 0)
        {
            if (in_file_fd > 0)
                close(in_file_fd);

            return -2;
        }

        outfd = out_file_fd;
    }
    else
    {
        outfd = STDOUT_FILENO;
    }

    for (i = 0; i < cmds->count - 1; i++)
    {
        if (pipe(pipes) < 0)
        {
            close(infd);

            goto error;
        }

        ret = exec_single(&cmds->commands[i], infd, pipes[1], true);
        if (ret < 0)
        {
            close(infd);
            close(pipes[0]);
            close(pipes[1]);

            goto error;
        }
        spawned++;

        close(pipes[1]);

        infd = pipes[0];
    }

    ret = exec_single(&cmds->commands[i], infd, outfd, true);
    if (ret < 0)
    {
        close(infd);
        goto error;
    }
    spawned++;

    close(infd);

    if (in_file_fd > 0)
        close(in_file_fd);
    if (out_file_fd > 0)
        close(out_file_fd);

    while (spawned > 0)
    {
        ret = wait(NULL);
        if (ret < 0)
            return -1;

        spawned--;
    }

    return 0;

error:
    if (in_file_fd > 0)
        close(in_file_fd);
    if (out_file_fd > 0)
        close(out_file_fd);

    while (spawned > 0)
    {
        ret = wait(NULL);
        if (ret < 0)
            return -1;

        spawned--;
    }

    return -1;
}