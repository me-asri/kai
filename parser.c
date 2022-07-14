#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>

#include "parser.h"

#define QUOTE_DOUBLE (1U << 1)
#define QUOTE_SINGLE (1U << 2)

int parse_command_list(command_list_t *list, const char *input)
{
    size_t len;
    char *copy;
    char *cmd;
    unsigned int quotes;
    size_t offset;

    size_t i;

    // Skip beginning whitespaces
    while (isspace(*input))
        input++;

    len = strlen(input);
    if (len == 0)
        return PARSER_RET_EMPTY; // All whitespace

    // Skip trailing whitespaces
    while (isspace(input[len - 1]))
        len--;

    if (len == 0)
        return PARSER_RET_EMPTY;

    if (*input == '|')
        return PARSER_RET_INVALID; // Line can't start with pipe symbol

    copy = malloc((len + 1) * sizeof(char));
    if (!copy)
        return PARSER_RET_MEM;

    strncpy(copy, input, len);
    copy[len] = '\0'; // Make sure string is null terminated

    // We should have at least one command
    list->count = 1;

    i = 0;
    quotes = 0;
    while (i < len)
    {
        if (!(quotes & QUOTE_SINGLE) && copy[i] == '"')
            quotes ^= QUOTE_DOUBLE;
        if (!(quotes & QUOTE_DOUBLE) && copy[i] == '\'')
            quotes ^= QUOTE_SINGLE;

        if (copy[i] == '|' && !quotes)
        {
            list->count++;
            copy[i] = '\0';

            if (++i >= len)
                goto bad_input; // Missing other side of pipe

            while (copy[i] == ' ')
            {
                copy[i] = '\0';
                i++; // Skip whitespace
            }

            if (copy[i] == '|')
                goto bad_input; // Empty pipe

            continue;
        }

        i++;
    }
    if (quotes)
        goto bad_input; // Mismatched quotation

    list->commands = malloc(list->count * sizeof(command_t));
    if (!list->commands)
    {
        free(copy);
        return PARSER_RET_MEM;
    }

    for (i = 0, offset = 0; offset < len; i++)
    {
        cmd = copy + offset;
        if (parse_command(&list->commands[i], cmd) <= 0)
            goto bad_input;

        offset += strlen(cmd);

        while (offset < len && copy[offset] == '\0')
            offset++;
    }

    free(copy);
    return PARSER_OK;

bad_input:
    free(copy);
    free(list->commands);
    return PARSER_RET_INVALID;
}

int parse_command(command_t *cmd, const char *input)
{
    size_t len;
    unsigned int quotes;
    size_t offset;
    char **redir_file;

    size_t i, j;

    // Skip beginning whitespaces
    while (isspace(*input))
        input++;

    len = strlen(input);
    if (len == 0)
        return PARSER_RET_EMPTY;

    // Skip trailing whitespaces
    while (isspace(input[len - 1]))
        len--;
    if (len == 0)
        return PARSER_RET_EMPTY;

    // Look for ampersand
    if (input[len - 1] == '&')
    {
        cmd->in_bg = true;
        len--;
    }
    else
    {
        cmd->in_bg = false;
    }
    // Skip whitespaces between command and ampersand
    while (isspace(input[len - 1]))
        len--;
    if (len == 0)
        return PARSER_RET_EMPTY;

    cmd->buffer = malloc((len + 1) * sizeof(char));
    if (!cmd->buffer)
        return PARSER_RET_MEM;

    strncpy(cmd->buffer, input, len);
    cmd->buffer[len] = '\0'; // Make sure string is null terminated

    cmd->output_file = NULL;
    cmd->input_file = NULL;
    for (i = len - 1, quotes = 0; i > 0 && (!cmd->output_file || !cmd->input_file); i--)
    {
        if (!(quotes & QUOTE_SINGLE) && cmd->buffer[i] == '"')
        {
            quotes ^= QUOTE_DOUBLE;
        }
        else if (!(quotes & QUOTE_DOUBLE) && cmd->buffer[i] == '\'')
        {
            quotes ^= QUOTE_SINGLE;
        }
        else if (cmd->buffer[i] == '>' || cmd->buffer[i] == '<')
        {
            if (quotes)
                break; // Quoted redirection operator

            if (cmd->buffer[i] == '>')
            {
                if (cmd->output_file)
                    continue;
                redir_file = &cmd->output_file;
            }
            else
            {
                if (cmd->input_file)
                    continue;
                redir_file = &cmd->input_file;
            }

            offset = i;
            cmd->buffer[i] = '\0';

            while (cmd->buffer[++i] == ' ')
                if (i >= len)
                    goto bad_input; // No file after redir

            if (cmd->buffer[i] == '"')
            {
                quotes ^= QUOTE_DOUBLE;
                i++;
            }
            else if (cmd->buffer[i] == '\"')
            {
                quotes ^= QUOTE_SINGLE;
                i++;
            }

            if (quotes)
            {
                for (j = i; j < len; j++)
                {
                    if (!(quotes & QUOTE_SINGLE) && cmd->buffer[j] == '"')
                    {
                        quotes ^= QUOTE_DOUBLE;
                        cmd->buffer[j] = '\0';
                    }
                    else if (!(quotes & QUOTE_DOUBLE) && cmd->buffer[j] == '\'')
                    {
                        quotes ^= QUOTE_SINGLE;
                        cmd->buffer[j] = '\0';
                    }
                }

                if (j - i == 1)
                    goto bad_input; // Empty quoted string
            }

            *redir_file = &cmd->buffer[i];

            len = offset;
            while (isspace(cmd->buffer[len - 1]))
            {
                cmd->buffer[len - 1] = '\0';
                len--;
            }
        }
    }

    cmd->argc = 1; // For program name
    for (i = 0, quotes = 0; i < len;)
    {
        if (!(quotes & QUOTE_SINGLE) && cmd->buffer[i] == '"')
        {
            cmd->buffer[i] = '\0';
            quotes ^= QUOTE_DOUBLE;
        }
        if (!(quotes & QUOTE_DOUBLE) && cmd->buffer[i] == '\'')
        {
            cmd->buffer[i] = '\0';
            quotes ^= QUOTE_SINGLE;
        }

        if (!quotes && isspace(cmd->buffer[i]))
        { 
            cmd->argc++;

            cmd->buffer[i] = '\0';

            while (++i < len && isspace(cmd->buffer[i]))
                cmd->buffer[i] = '\0';

            continue;
        }

        i++;
    }
    if (quotes)
        goto bad_input; // Mismatched quotation

    cmd->argv = malloc((cmd->argc + 1) * sizeof(char *)); // + 1 for null terminating array
    if (!cmd->argv)
    {
        free(cmd->buffer);
        return PARSER_RET_MEM;
    }

    // If the program name is quoted, buffer[0] will be '\0', we don't want that
    if (cmd->buffer[0] == '\0')
        offset = 1;
    else
        offset = 0;

    for (i = 0; offset < len && i < cmd->argc; i++)
    {
        cmd->argv[i] = cmd->buffer + offset;
        offset += strlen(cmd->argv[i]);

        while (offset < len && cmd->buffer[offset] == '\0')
            offset++;
    }
    cmd->argv[i] = NULL; // exec requires NULL terminated array

    return PARSER_OK;

bad_input:
    free(cmd->buffer);
    return PARSER_RET_INVALID;
}

void free_command_list(command_list_t *cmdlist)
{
    size_t i;

    for (i = 0; i < cmdlist->count; i++)
        free_command(&cmdlist->commands[i]);

    free(cmdlist->commands);
}

void free_command(command_t *cmd)
{
    free(cmd->argv);
    free(cmd->buffer);
}
