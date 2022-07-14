#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>
#include <stdbool.h>

#define PARSER_OK 1
#define PARSER_RET_EMPTY 0
#define PARSER_RET_INVALID -1
#define PARSER_RET_MEM -2

typedef struct command
{
    char *buffer;

    size_t argc;
    char **argv;

    bool in_bg;

    char *input_file;
    char *output_file;
} command_t;

typedef struct command_list
{
    size_t count;
    command_t *commands;
} command_list_t;

int parse_command(command_t *cmd, const char *input);

void free_command(command_t *cmd);

int parse_command_list(command_list_t *cmdlist, const char *input);

void free_command_list(command_list_t *cmdlist);

#endif