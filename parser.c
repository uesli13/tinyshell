#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "parser.h"

#define DELIMITERS " \t\n" // Split on space, tab, or newline

// Helper to remove surrounding quotes from a string
void strip_quotes(char *str) {
    if (!str) return;
    
    int len = strlen(str);
    if (len < 2) return; // Too short to have quotes

    // Check for double quotes "..." or single quotes '...'
    if ((str[0] == '"' && str[len-1] == '"') || 
        (str[0] == '\'' && str[len-1] == '\'')) {
        
        // Move memory: shift everything after the first quote to the left
        // This overwrites the first quote
        memmove(str, str + 1, len - 2);
        
        // Add null terminator to cut off the last quote
        str[len - 2] = '\0';
    }
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

// Parses a single command string into a Command struct
void parse_command(char *input, Command *cmd) {
    int arg_idx = 0;
    
    // Initialize struct
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->error_file = NULL;
    cmd->append_mode = 0;
    
    char *token = strtok(input, DELIMITERS);
    while (token != NULL && arg_idx < MAX_ARGS - 1) {
        
        // Input Redirection (<)
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, DELIMITERS); // Get the filename
            if (token) cmd->input_file = token;
        }
        // Output Redirection (>)
        else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, DELIMITERS);
            if (token) {
                cmd->output_file = token;
                cmd->append_mode = 0;
            }
        }
        // Append Redirection (>>)
        else if (strcmp(token, ">>") == 0) {
            token = strtok(NULL, DELIMITERS);
            if (token) {
                cmd->output_file = token;
                cmd->append_mode = 1;
            }
        }
        // Error Redirection (2>)
        else if (strcmp(token, "2>") == 0) {
            token = strtok(NULL, DELIMITERS);
            if (token) cmd->error_file = token;
        }
        // Regular Argument
        else {
            strip_quotes(token);
            cmd->args[arg_idx++] = token;
        }

        token = strtok(NULL, DELIMITERS);
    }
    cmd->args[arg_idx] = NULL;

}