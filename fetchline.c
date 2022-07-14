#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "fetchline.h"
#include "history.h"

typedef enum ctrl_code
{
    CTRL_BKSP,
    CTRL_ENTER,
    CTRL_C,
    CTRL_D,
    CTRL_UNKNOWN,
    CTRL_ANSI_UP,
    CTRL_ANSI_DOWN,
    CTRL_ANSI_RIGHT,
    CTRL_ANSI_LEFT,
    CTRL_ANSI_DEL,
    CTRL_ANSI_INS,
    CTRL_ANSI_UNKNOWN
} ctrl_code_t;

static int term_set_raw(fetchline_ctx_t *ctx);

static int term_reset(fetchline_ctx_t *ctx);

static ctrl_code_t parse_ctrl(char c);

static ctrl_code_t parse_ansi();

static void move_cursor(int offset);

static void erase_line();

static int charcat(char **dest, size_t *destlen, size_t slen, char c, size_t index);

static void delchar(char *str, size_t index);

int fetchline_ctx_init(fetchline_ctx_t *context)
{
    history_init(&context->hist);

    if (tcgetattr(STDIN_FILENO, &context->old_opts) != 0)
        return -1;

    return 0;
}

void fetchline_ctx_free(fetchline_ctx_t *context)
{
    history_free(&context->hist);
}

ssize_t fetchline(fetchline_ctx_t *context, const char *prompt, char **buffer, size_t *buflen)
{
    ssize_t ret;

    char c;
    ctrl_code_t cc;
    bool end = false;

    size_t cursor_pos = 0;
    size_t slen = 0;
    ssize_t histlen;

    if (term_set_raw(context) < 0)
        return FL_RET_SYS_FAIL;

    // Make sure buffer is clear
    *buffer[0] = '\0';

    while (!end)
    {
        erase_line();
        fputs(prompt, stdout);
        fputs(*buffer, stdout);
        move_cursor(-1 * slen);
        move_cursor(cursor_pos);

        c = getchar();
        if (iscntrl(c))
        {
            cc = parse_ctrl(c);
            switch (cc)
            {
            case CTRL_ENTER:
                fputc('\n', stdout);

                if (strcmp("!!", *buffer) == 0)
                {
                    histlen = history_peek_last(&context->hist, buffer, buflen);
                    if (histlen == 0)
                    {
                        fputs("[!] No entries in history\n", stderr);
                        return FL_RET_EMPTY;
                    }
                    else if (histlen < 0)
                    {
                        return FL_RET_MEM_FAIL; // Memory failure
                    }

                    puts(*buffer);
                    return histlen;
                }

                if (history_add(&context->hist, *buffer) < 0)
                    return FL_RET_MEM_FAIL;

                end = true;
                ret = slen;

                break;
            case CTRL_C:
                end = true;
                ret = FL_RET_INTERRUPT;
                fputc('\n', stdout);

                break;
            case CTRL_D:
                if (*buffer[0] == '\0')
                {
                    end = true;
                    ret = FL_RET_EOF;
                    fputc('\n', stdout);
                }

                break;
            case CTRL_BKSP:
                if (cursor_pos > 0)
                {
                    delchar(*buffer, cursor_pos - 1);
                    cursor_pos--;
                    slen--;
                }

                break;
            case CTRL_ANSI_DEL:
                if (cursor_pos < slen)
                {
                    delchar(*buffer, cursor_pos);
                    slen--;
                }

                break;
            case CTRL_ANSI_LEFT:
                if (cursor_pos > 0)
                    cursor_pos--;

                break;
            case CTRL_ANSI_RIGHT:
                if (cursor_pos < slen)
                    cursor_pos++;

                break;
            case CTRL_ANSI_UP:
                histlen = history_get_prev(&context->hist, buffer, buflen);
                if (histlen < 0)
                    return FL_RET_MEM_FAIL;

                if (histlen != 0)
                {
                    slen = histlen;
                    cursor_pos = slen;
                }

                break;
            case CTRL_ANSI_DOWN:
                histlen = history_get_next(&context->hist, buffer, buflen);
                if (histlen < 0)
                    return FL_RET_MEM_FAIL;

                if (histlen != 0)
                {
                    slen = histlen;
                    cursor_pos = slen;
                }
                else
                {
                    // Clear line
                    cursor_pos = 0;
                    *buffer[0] = '\0';
                    slen = 0;
                }

                break;
            default:
                break;
            }
        }
        else
        {
            charcat(buffer, buflen, slen, c, cursor_pos);
            slen++;
            cursor_pos++;
        }

        fflush(stdout);
    }

    if (term_reset(context) < 0)
        return FL_RET_SYS_FAIL;

    return ret;
}

int term_set_raw(fetchline_ctx_t *ctx)
{
    struct termios opts;

    opts = ctx->old_opts;
    opts.c_iflag &= ~(IXOFF);
    opts.c_lflag &= ~(ISIG | ICANON | ECHO);

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &opts) != 0)
        return -1;

    return 0;
}

int term_reset(fetchline_ctx_t *ctx)
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ctx->old_opts) != 0)
        return -1;

    return 0;
}

ctrl_code_t parse_ctrl(char c)
{
    switch (c)
    {
    case '\n':
        return CTRL_ENTER;
    case 0x03:
        return CTRL_C;
    case 0x04:
        return CTRL_D;
    case 0x7f:
        return CTRL_BKSP;
    case '\e':
        return parse_ansi();
    default:
        return CTRL_UNKNOWN;
    }
}

ctrl_code_t parse_ansi()
{
    if (getchar() != '[')
        return CTRL_ANSI_UNKNOWN;

    switch (getchar())
    {
    case 'A':
        return CTRL_ANSI_UP;
    case 'B':
        return CTRL_ANSI_DOWN;
    case 'D':
        return CTRL_ANSI_LEFT;
    case 'C':
        return CTRL_ANSI_RIGHT;
    case '3':
        switch (getchar())
        {
        case '~':
            return CTRL_ANSI_DEL;
        default:
            return CTRL_ANSI_UNKNOWN;
        }
    case '2':
        switch (getchar())
        {
        case '~':
            return CTRL_ANSI_INS;
        default:
            return CTRL_ANSI_UNKNOWN;
        }
    default:
        return CTRL_ANSI_UNKNOWN;
    }
}

void move_cursor(int offset)
{
    if (offset == 0)
        return;
    printf("\e[%d%c", abs(offset), (offset > 0) ? 'C' : 'D');
}

void erase_line()
{
    fputs("\e[2K\r", stdout);
}

int charcat(char **dest, size_t *destlen, size_t slen, char c, size_t index)
{
    size_t new_size;
    char *new_buf;

    if (*destlen < slen + 1)
    {
        new_size = *destlen * 2 / 3;
        if (new_size < slen)
            new_size = slen + 1;

        new_buf = realloc(*dest, new_size);
        if (!new_buf)
            return -1;

        *destlen = new_size;
        *dest = new_buf;
    }

    if (slen == index)
    {
        (*dest)[slen] = c;
        (*dest)[slen + 1] = '\0';
    }
    else
    {
        memmove(*dest + index + 1, *dest + index, slen - index + 1); // + 1 in length for '\0'
        (*dest)[index] = c;
    }
    return 0;
}

void delchar(char *str, size_t index)
{
    size_t slen;

    slen = strlen(str);

    if (index == slen - 1)
    {
        str[index] = '\0';
        return;
    }

    memmove(str + index, str + index + 1, slen - index);
}