#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "tinyshell.h"
#include "parser.h"
#include "executor.h"
#include "jobs.h"

pid_t shell_pgid;   // Shell's process group ID
int shell_terminal; // File descriptor of the terminal

void init_shell() {
    shell_terminal = STDIN_FILENO;
    
    // Check if we are running in a terminal
    if (isatty(shell_terminal)) {
        // Make sure the shell is in the foreground
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);

        // Ignore signals in the shell (so it does not exit on Ctrl-C, etc.)
        signal(SIGINT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);

        // Put the shell in its own process group
        shell_pgid = getpid();
        if (setpgid(shell_pgid, shell_pgid) < 0) {
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }

        // Take control of the terminal
        tcsetpgrp(shell_terminal, shell_pgid);
    }
    
    // Set up the SIGCHLD handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART; // Restart interrupted system calls
    sigaction(SIGCHLD, &sa, NULL);
    
    init_jobs(); // Initialize / clear the jobs table
}


int main() {
    init_shell();
    char buffer[BUFFER_SIZE];
    Command commands[MAX_PIPES]; // Array of commands

    // Infinite Main Loop
    while (1) {
        printf("tinyshell> ");
        fflush(stdout);

        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            printf("\n");
            break;
        }

        // Parse
        int num_cmds = parse_pipeline(buffer, commands);

        // Handle Empty Input
        if (num_cmds == 0) continue;

        // Check for '&' (Background)
        int background = 0;
        int last_cmd_idx = num_cmds - 1;
        int arg_idx = 0;

        // Find the last argument of the last command
        while (commands[last_cmd_idx].args[arg_idx] != NULL) {
            arg_idx++;
        }

        if (arg_idx > 0 && strcmp(commands[last_cmd_idx].args[arg_idx - 1], "&") == 0) {
                    background = 1;
                    commands[last_cmd_idx].args[arg_idx - 1] = NULL; // Remove the '&'
                }

        // Handle built in commands: exit, jobs, fg, bg
        if (strcmp(commands[0].args[0], "exit") == 0) {
            printf("Goodbye!\n");
            break;
        }
        else if (strcmp(commands[0].args[0], "jobs") == 0) {
            execute_jobs();
        }
        else if (strcmp(commands[0].args[0], "fg") == 0) {
            execute_fg(commands[0].args[1]);
        }
        else if (strcmp(commands[0].args[0], "bg") == 0) {
            execute_bg(commands[0].args[1]);
        }
        else {
            // Execute External Command
            execute_pipeline(commands, num_cmds, background);
        }
    }

    return 0;
}
