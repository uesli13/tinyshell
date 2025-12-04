#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define BUFFER_SIZE 1024
#define MAX_ARGS 64        // Maximum number of arguments allowed
#define DELIMITERS " \t\n" // Split on space, tab, or newline
#define MAX_PIPES 10

typedef struct {
    char *args[MAX_ARGS]; // The command and its arguments
    char *input_file;     // Filename for '<' redirection
    char *output_file;    // Filename for '>' or '>>' redirection 
    int append_mode;      // 1 if '>>', 0 if '>'
    char *error_file;     // Filename for '2>'
} Command;

// Parses a single command string into a Command struct
void parse_command(char *input, Command *cmd) {
    int arg_idx = 0;
    
    // Initialize struct
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->error_file = NULL;
    cmd->append_mode = 0;
    
    char *token = strtok(input, " \t\n");
    while (token != NULL && arg_idx < MAX_ARGS - 1) {
        
        // Input Redirection (<)
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " \t\n"); // Get the filename
            if (token) cmd->input_file = token;
        }
        // Output Redirection (>)
        else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " \t\n");
            if (token) {
                cmd->output_file = token;
                cmd->append_mode = 0;
            }
        }
        // Append Redirection (>>)
        else if (strcmp(token, ">>") == 0) {
            token = strtok(NULL, " \t\n");
            if (token) {
                cmd->output_file = token;
                cmd->append_mode = 1;
            }
        }
        // Error Redirection (2>)
        else if (strcmp(token, "2>") == 0) {
            token = strtok(NULL, " \t\n");
            if (token) cmd->error_file = token;
        }
        // Regular Argument
        else {
            cmd->args[arg_idx++] = token;
        }

        token = strtok(NULL, " \t\n");
    }
    cmd->args[arg_idx] = NULL;

}

// Parses the full line into an array of Command structs and  returns the number of commands found
int parse_pipeline(char *line, Command *commands) {
    int cmd_count = 0;
    char *line_cursor = line;
    
    // Split by pipe '|'
    char *next_pipe = strchr(line_cursor, '|');
    
    while (line_cursor != NULL && cmd_count < MAX_PIPES) {
        if (next_pipe != NULL) {
            *next_pipe = '\0'; // Replace '|' with null terminator
        }

        // Parse the current segment
        parse_command(line_cursor, &commands[cmd_count]);
        cmd_count++;

        if (next_pipe != NULL) {
            line_cursor = next_pipe + 1; // Move past the '|'
            next_pipe = strchr(line_cursor, '|');
        } else {
            line_cursor = NULL; // Finished
        }
    }
    return cmd_count;
}

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


void execute_pipeline(Command *commands, int num_cmds) {
    int pipefd[2];
    int prev_read_fd = -1; // Holds the read end of the previous pipe

    for (int i = 0; i < num_cmds; i++) {
        
        // Create a pipe if this is NOT the last command
        if (i < num_cmds - 1) {
            if (pipe(pipefd) < 0) {
                perror("Pipe failed");
                return;
            }
        }

        // Fork the process
        pid_t pid = fork();

        if (pid < 0) {
            perror("Fork failed");
            return;
        } 
        else if (pid == 0) {
            // CHILD PROCESS

            // Handle Pipes (Connecting commands together)
            
            // If there is a previous pipe, read from it (replace stdin)
            if (prev_read_fd != -1) {
                dup2(prev_read_fd, STDIN_FILENO);
                close(prev_read_fd);
            }

            // If there is a next command, write to the current pipe (replace stdout)
            if (i < num_cmds - 1) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]); // Close write end after dup
                close(pipefd[0]); // Close read end (not needed in writer)
            }

            // Handle File Redirection (Overriding pipes if specified)

            // Input Redirection (<)
            if (commands[i].input_file) {
                int fd = open(commands[i].input_file, O_RDONLY);
                if (fd < 0) {
                    perror("Error opening input file");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            // Output Redirection (> or >>)
            if (commands[i].output_file) {
                int flags = O_WRONLY | O_CREAT;
                if (commands[i].append_mode) 
                    flags |= O_APPEND;
                else 
                    flags |= O_TRUNC;

                int fd = open(commands[i].output_file, flags, 0644);
                if (fd < 0) {
                    perror("Error opening output file");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            // Error Redirection (2>)
            if (commands[i].error_file) {
                int fd = open(commands[i].error_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    perror("Error opening error file");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDERR_FILENO);
                close(fd);
            }

            // Execute Command
            char *executable_path = get_executable_path(commands[i].args[0]);
            
            if (executable_path == NULL) {
                fprintf(stderr, "%s: command not found\n", commands[i].args[0]);
                exit(127);
            }

            execve(executable_path, commands[i].args, NULL);
            perror("Execution failed"); // Only reaches here if execve fails
            free(executable_path);
            exit(EXIT_FAILURE);
        } 
        else {
            // PARENT PROCESS
            
            // Close the read end of the PREVIOUS pipe
            if (prev_read_fd != -1) {
                close(prev_read_fd);
            }

            if (i < num_cmds - 1) {
                prev_read_fd = pipefd[0];
                close(pipefd[1]);
            }
        }
    }

    // Wait for ALL children to finish
    for (int i = 0; i < num_cmds; i++) {
        wait(NULL);
    }
}

int main() {
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

        // 1. Parse
        int num_cmds = parse_pipeline(buffer, commands);

        // 2. Handle Empty Input
        if (num_cmds == 0) continue;

        // 3. Handle 'exit'
        if (num_cmds == 1 && strcmp(commands[0].args[0], "exit") == 0) {
            printf("Goodbye!\n");
            break;
        }

        // 4. Execute
        execute_pipeline(commands, num_cmds);
    }

    return 0;
}
