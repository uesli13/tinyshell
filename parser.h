#ifndef PARSER_H
#define PARSER_H
#include "tinyshell.h"

int parse_pipeline(char *line, Command *commands);
void parse_command(char *input, Command *cmd);
void strip_quotes(char *str);

#endif