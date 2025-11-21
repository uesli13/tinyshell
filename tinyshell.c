#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024
#define MAX_ARGS 64        // Maximum number of arguments allowed
#define DELIMITERS " \t\n" // Split on space, tab, or newline

void parse_args(char *line, char **argv) {
    int i = 0;
    
    // Get the first token
    char *token = strtok(line, DELIMITERS); 

    while (token != NULL && i < MAX_ARGS - 1) {
        argv[i] = token; // Store the pointer to the token
        i++;

        // Get the next token. passing NULL to strtok to continue from the previous string
        token = strtok(NULL, DELIMITERS); 
    }
    
    // Null terminate the argument list
    argv[i] = NULL; 
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

void execute_command(char **args) {
    
    // Find the full path of the executable
    char *command_path = get_executable_path(args[0]); 
    
    // If command is not found in PATH or via absolute path
    if (command_path == NULL) {
        fprintf(stderr, "%s: command not found\n", args[0]);
        return; 
    }

    // Create a new process
    pid_t pid = fork(); 

    // Error handling if fork fails
    if (pid < 0) {
        perror("Fork failed");
    }
    else if (pid == 0) {
        // This code runs only in the child process.

        // Execute the command
        execve(command_path, args, NULL);

        // If execve returns, it failed
        perror("Execution failed");

        // Clean up allocated memory
        free(command_path); 

        exit(EXIT_FAILURE);
    } 
    else {
        // This code runs only in the parent process.
        int status;
        
        // Wait for the specific child (pid) to finish
        waitpid(pid, &status, 0); 

        // Report exit status
        if (WIFEXITED(status)) {
            printf("[Process exited with code %d]\n", WEXITSTATUS(status));
        }

        // Clean up allocated memory
        free(command_path);
    }
}

int main() {
    char buffer[BUFFER_SIZE];
    char *args[MAX_ARGS]; // Array to hold command arguments

    // Infinite Main Loop
    while (1) {

        printf("tinyshell> ");
        fflush(stdout);

        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            printf("\n");
            break;
        }

        // Parse input into arguments
        parse_args(buffer, args); 

        // If no command was typed (empty line), continue
        if (args[0] == NULL) {
            continue; 
        }

        // Handle 'exit' built-in command
        if (strcmp(args[0], "exit") == 0) {
            printf("Goodbye!\n");
            break;
        }

        // Execute the command
        execute_command(args); 
    }

    return 0;
}