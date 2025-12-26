#include "jobs.h"
#include "executor.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h> 
#include <sys/wait.h>

char *get_executable_path(char *command) {
    // Check if the command is already an absolute or relative path
    // If it contains '/', treat it as a direct path
    if (strchr(command, '/') != NULL) {
        if (access(command, X_OK) == 0) {
            return strdup(command); // Return a copy of the string
        }
        return NULL;
    }

    // Get the PATH environment variable
    char *path_env = getenv("PATH");
    if (path_env == NULL) return NULL;

    // Duplicate the PATH string because strtok modifies it
    char *path_copy = strdup(path_env);

    // Split by colon
    char *dir = strtok(path_copy, ":"); 

    // Allocate enough space: directory length + / + command length + null terminator
    char *full_path = malloc(BUFFER_SIZE);

    while (dir != NULL) {
        // Construct the full path: dir + "/" + command
        sprintf(full_path, "%s/%s", dir, command);

        // Check if this file exists and is executable
        if (access(full_path, X_OK) == 0) {
            free(path_copy); // Clean up the copy
            return full_path; // Return the successful path
        }

        dir = strtok(NULL, ":"); // Move to next directory
    }

    // Clean up if nothing found
    free(path_copy);
    free(full_path);
    return NULL;
}


void execute_pipeline(Command *commands, int num_cmds, int background) {
    int pipefd[2];
    int prev_read_fd = -1; 
    pid_t pgid = 0; // Process Group ID for the entire pipeline

    for (int i = 0; i < num_cmds; i++) {
        
        if (i < num_cmds - 1) {
            if (pipe(pipefd) < 0) {
                perror("Pipe failed");
                return;
            }
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("Fork failed");
            return;
        } 
        else if (pid == 0) {
            // --- CHILD PROCESS ---

            // 1. Process group setup
            // The child joins a new process group. If it is the first child
            // (pgid == 0), it becomes the group leader.
            pid_t my_pid = getpid();
            if (pgid == 0) pgid = my_pid;
            setpgid(my_pid, pgid); 

            // 2. Restore default signal handling
            // Children must respond to Ctrl-C / Ctrl-Z
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);

            // 3. Foreground control
            // If this is a foreground job, give terminal control to the pipeline
            if (!background) {
                tcsetpgrp(shell_terminal, pgid);
            }

            // Pipe & Redirection Logic
            if (prev_read_fd != -1) {
                dup2(prev_read_fd, STDIN_FILENO);
                close(prev_read_fd);
            }
            if (i < num_cmds - 1) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
                close(pipefd[0]);
            }

            if (commands[i].input_file) {
                int fd = open(commands[i].input_file, O_RDONLY);
                if (fd < 0) { perror("Input error"); exit(EXIT_FAILURE); }
                dup2(fd, STDIN_FILENO); close(fd);
            }
            if (commands[i].output_file) {
                int flags = O_WRONLY | O_CREAT | (commands[i].append_mode ? O_APPEND : O_TRUNC);
                int fd = open(commands[i].output_file, flags, 0644);
                if (fd < 0) { perror("Output error"); exit(EXIT_FAILURE); }
                dup2(fd, STDOUT_FILENO); close(fd);
            }
            if (commands[i].error_file) {
                int fd = open(commands[i].error_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror("Error file error"); exit(EXIT_FAILURE); }
                dup2(fd, STDERR_FILENO); close(fd);
            }

            // Execution
            char *executable_path = get_executable_path(commands[i].args[0]);
            if (!executable_path) {
                fprintf(stderr, "%s: command not found\n", commands[i].args[0]);
                exit(127);
            }
            execve(executable_path, commands[i].args, NULL);
            perror("Execution failed");
            exit(EXIT_FAILURE);

        } 
        else {
            // --- PARENT PROCESS ---
            
            // Prevent race conditions:
            // setpgid is also called in the parent to ensure correctness
            if (pgid == 0) pgid = pid;
            setpgid(pid, pgid);

            // Close unused pipe ends
            if (prev_read_fd != -1) close(prev_read_fd);
            if (i < num_cmds - 1) {
                prev_read_fd = pipefd[0];
                close(pipefd[1]);
            }

            // If this is the last command in the pipeline
            if (i == num_cmds - 1) {
                 if (!background) {
                    // FOREGROUND: wait for completion
                    // Give terminal control to the child's process group
                    tcsetpgrp(shell_terminal, pgid);
                    
                    int status;
                    // Wait for any process in the group (waitpid with -pgid)
                    waitpid(-pgid, &status, WUNTRACED);
                    
                    // If the job was stopped (Ctrl-Z), add it to the job list
                    if (WIFSTOPPED(status)) {
                        int job_id = add_job(pgid, commands[0].args[0], JOB_STOPPED);
                        printf("\n[%d]+ Stopped    %s\n", job_id, commands[0].args[0]);
                    }

                    // Restore terminal control to the shell
                    tcsetpgrp(shell_terminal, shell_pgid);
                } 
                else {
                    // BACKGROUND: do not wait
                    int job_id = add_job(pgid, commands[0].args[0], JOB_RUNNING);
                    printf("[%d] %d\n", job_id, pid);
                }
            }
        }
    }
}