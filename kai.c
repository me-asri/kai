#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "kai.h"
#include "fetchline.h"
#include "eval.h"

#define INITIAL_LINE_LEN 64
#define INITIAL_PROMPT_LEN 128

static const char PROMPT_FMT[] = "\e[1m\e[34m%s\e[39m@\e[33m%s\e[39m \e[32m%s\e[39m%s ";
static const char PROMPT_USER_SYM[] = "\e[1m%\e[0m";
static const char PROMPT_ROOT_SYM[] = "\e[31m\e[1m#\e[0m";

static int gen_prompt(char **prompt, size_t *length);

int main()
{
    kai_ctx_t context = {.running = true, .jobs = 0, .exit_code = 0};

    char *prompt;
    size_t plen;
    char *buffer;
    size_t buflen;

    fetchline_ctx_t fctx;
    ssize_t slen;

    eval_res_t evresult;
    pid_t jpid;

    plen = INITIAL_PROMPT_LEN;
    prompt = malloc(sizeof(char) * plen);
    if (!prompt)
    {
        fputs("[!] Failed to allocate memory for prompt text", stderr);
        return 1;
    }

    buflen = INITIAL_LINE_LEN;
    buffer = malloc(sizeof(char) * buflen);
    if (!buffer)
    {
        free(prompt);

        fputs("[!] Failed to allocate memory for line buffer", stderr);
        return 1;
    }

    fetchline_ctx_init(&fctx);

    while (context.running)
    {
        if (context.jobs > 0)
        {
            jpid = waitpid(-1, NULL, WNOHANG);
            if (jpid < -1)
            {
                fputs("[!] waitpid() failed", stderr);
                context.exit_code = 1;
                break;
            }
            else if (jpid > 0)
            {
                context.jobs--;
                printf("[%d] job finished - total jobs: %zu\n", jpid, context.jobs);
            }
        }

        if (gen_prompt(&prompt, &plen) < 0)
        {
            fputs("[!] Failed to generate prompt", stderr);
            context.exit_code = 1;
            break;
        }

        slen = fetchline(&fctx, prompt, &buffer, &buflen);
        if (slen == FL_RET_EMPTY || slen == FL_RET_INTERRUPT)
            continue;
        if (slen < 0)
        {
            if (slen != FL_RET_EOF)
            {
                fputs("[!] Failed to process input\n", stderr);
                context.exit_code = 1;
            }

            break;
        }

        eval(&evresult, buffer, &context);
        if (evresult.status < 0)
        {
            printf("[!] Error: %s\n", evresult.err_msg);
            continue;
        }

        if (evresult.bg_pid > 0)
        {
            context.jobs++;
            printf("[%d] job started - total jobs: %zu\n", evresult.bg_pid, context.jobs);
        }
    }

    free(prompt);
    free(buffer);

    fetchline_ctx_free(&fctx);

    return context.exit_code;
}

int gen_prompt(char **prompt, size_t *length)
{
    char host[HOST_NAME_MAX + 1];
    char path[PATH_MAX + 1];
    char user[LOGIN_NAME_MAX + 1];
    uid_t uid;
    char const *sym;

    size_t len;
    char *newbuf;

    if (gethostname(host, sizeof(host)) != 0)
        return -1;

    if (!getcwd(path, sizeof(path)))
        return -1;

    uid = geteuid();
    if (uid == 0)
    {
        strcpy(user, "root");
        sym = PROMPT_ROOT_SYM;
    }
    else
    {
        if (getlogin_r(user, sizeof(user)) != 0)
            return -1;
        sym = PROMPT_USER_SYM;
    }

    len = snprintf(NULL, 0, PROMPT_FMT, user, host, path, sym) + 1;
    if (*length < len)
    {
        newbuf = realloc(*prompt, len);
        if (!newbuf)
            return -1;

        *length = len;
        *prompt = newbuf;
    }
    snprintf(*prompt, len, PROMPT_FMT, user, host, path, sym);

    return 0;
}
