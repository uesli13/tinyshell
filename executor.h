#ifndef EXECUTOR_H
#define EXECUTOR_H
#include "tinyshell.h"

void execute_pipeline(Command *commands, int num_cmds, int background);
char *get_executable_path(char *command);

#endif