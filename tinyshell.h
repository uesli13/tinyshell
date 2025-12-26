#ifndef TINYSHELL_H
#define TINYSHELL_H

#include <sys/types.h>

#define BUFFER_SIZE 1024
#define MAX_ARGS 64
#define MAX_JOBS 20
#define MAX_PIPES 10

extern pid_t shell_pgid;
extern int shell_terminal;

typedef struct {
    char *args[MAX_ARGS];
    char *input_file;
    char *output_file;
    int append_mode;
    char *error_file;
} Command;

#endif