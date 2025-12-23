#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h> 
#include <termios.h>
#include <errno.h>

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



#define MAX_JOBS 20
// Καταστάσεις Εργασίας
#define JOB_RUNNING 1
#define JOB_STOPPED 2
#define JOB_DONE    3

typedef struct {
    int id;                 // Το Job ID (π.χ. [1], [2])
    pid_t pgid;             // Το Process Group ID της εργασίας
    char command[BUFFER_SIZE]; // Η εντολή που έτρεξε
    int state;              // RUNNING, STOPPED, ή DONE
} Job;

// Global μεταβλητές για διαχείριση του Shell
Job jobs[MAX_JOBS];         // Πίνακας εργασιών
pid_t shell_pgid;           // Το PID του shell (για να γυρνάμε τον έλεγχο σε εμάς)
int shell_terminal;         // Ο file descriptor του τερματικού (συνήθως STDIN_FILENO)





void init_jobs() {
    for (int i = 0; i < MAX_JOBS; i++) {
        jobs[i].id = 0; // 0 σημαίνει κενή θέση
    }
}

// Προσθήκη νέας εργασίας στη λίστα
int add_job(pid_t pgid, char *command, int state) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].id == 0) { // Βρήκαμε κενή θέση
            jobs[i].id = i + 1;
            jobs[i].pgid = pgid;
            jobs[i].state = state;
            strncpy(jobs[i].command, command, BUFFER_SIZE - 1);
            return i + 1; // Επιστρέφουμε το Job ID
        }
    }
    printf("Error: Too many jobs\n");
    return -1;
}

// Διαγραφή εργασίας με βάση το PID (Process Group ID)
void remove_job(pid_t pgid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pgid == pgid) {
            jobs[i].id = 0; // Μαρκάρισμα ως κενό
            return;
        }
    }
}

// Εύρεση εργασίας με βάση το Job ID (για τις εντολές fg/bg)
Job* find_job_by_id(int id) {
    if (id <= 0 || id > MAX_JOBS) return NULL;
    if (jobs[id-1].id != 0) return &jobs[id-1];
    return NULL;
}

// Εύρεση εργασίας με βάση το PID (για τον signal handler)
Job* find_job_by_pgid(pid_t pgid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pgid == pgid) return &jobs[i];
    }
    return NULL;
}


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

// Parses the full line into an array of Command structs and  returns the number of commands found
int parse_pipeline(char *line, Command *commands) {
    int cmd_count = 0;
    char *line_cursor = line;
    
    // Split by pipe '|'
    char *next_pipe = strchr(line_cursor, '|');

    //Debug line:
    // printf("Debug: Parsing pipeline: %s\n", line);

    //Debug next_pipe:
    // printf("Debug: Next pipe at: %s\n", next_pipe ? next_pipe : "NULL");

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




void execute_pipeline(Command *commands, int num_cmds, int background) {
    int pipefd[2];
    int prev_read_fd = -1; 
    pid_t pgid = 0; // Το Process Group ID για όλο το pipeline

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

            // 1. Process Group Setup
            // Το παιδί μπαίνει σε νέο group. Αν είναι το πρώτο παιδί (pgid == 0),
            // γίνεται αρχηγός του group.
            pid_t my_pid = getpid();
            if (pgid == 0) pgid = my_pid;
            setpgid(my_pid, pgid); 

            // 2. Signal Handling Restore
            // Επαναφέρουμε τα σήματα ώστε τα παιδιά να ακούνε το Ctrl-C/Z
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);

            // 3. Foreground Control
            // Αν είναι foreground, δίνουμε στο pipeline τον έλεγχο του τερματικού
            if (!background) {
                tcsetpgrp(shell_terminal, pgid);
            }

            // --- Pipe & Redirection Logic (Ίδιο με πριν) ---
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
            
            // Race Condition Prevention
            // Κάνουμε setpgid ΚΑΙ στον γονιό για να είμαστε σίγουροι
            if (pgid == 0) pgid = pid;
            setpgid(pid, pgid);

            // Κλείσιμο pipes
            if (prev_read_fd != -1) close(prev_read_fd);
            if (i < num_cmds - 1) {
                prev_read_fd = pipefd[0];
                close(pipefd[1]);
            }

            // Αν είναι το τελευταίο command της σωλήνωσης
            if (i == num_cmds - 1) {
                 if (!background) {
                    // FOREGROUND: Περιμένουμε να τελειώσει
                    // Δίνουμε έλεγχο τερματικού στο process group του παιδιού
                    tcsetpgrp(shell_terminal, pgid);
                    
                    int status;
                    // Περιμένουμε οποιοδήποτε παιδί του group (waitpid -pgid)
                    waitpid(-pgid, &status, WUNTRACED);
                    
                    // Αν σταμάτησε (Ctrl-Z), το προσθέτουμε στη λίστα
                    if (WIFSTOPPED(status)) {
                        int job_id = add_job(pgid, commands[0].args[0], JOB_STOPPED);
                        printf("\n[%d]+ Stopped    %s\n", job_id, commands[0].args[0]);
                    }

                    // Παίρνουμε πίσω τον έλεγχο του τερματικού
                    tcsetpgrp(shell_terminal, shell_pgid);
                } 
                else {
                    // BACKGROUND: Δεν περιμένουμε
                    int job_id = add_job(pgid, commands[0].args[0], JOB_RUNNING);
                    printf("[%d] %d\n", job_id, pid);
                }
            }
        }
    }
}




// Ο Handler για το SIGCHLD (όταν ένα παιδί τερματίζει ή σταματάει)
void sigchld_handler(int sig) {
    int status;
    pid_t pid;

    // Το WNOHANG επιστρέφει αμέσως αν δεν υπάρχουν νεκρά παιδιά
    // Το WUNTRACED μας ειδοποιεί και αν κάποιο παιδί σταμάτησε (Ctrl-Z)
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        Job *job = find_job_by_pgid(pid);
        
        if (job) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                // Το παιδί τερμάτισε
                remove_job(pid);
            } 
            else if (WIFSTOPPED(status)) {
                // Το παιδί σταμάτησε (Ctrl-Z)
                job->state = JOB_STOPPED;
                printf("\n[%d]+ Stopped    %s\n", job->id, job->command);
            }
        }
    }
}

// Αρχικοποίηση του Shell
void init_shell() {
    shell_terminal = STDIN_FILENO;
    
    // Ελέγχουμε αν τρέχουμε σε τερματικό
    if (isatty(shell_terminal)) {
        // Βάζουμε το shell στο δικό του process group
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);

        // Αγνοούμε τα σήματα στο Shell (για να μην κλείνει με Ctrl-C)
        signal(SIGINT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);

        // Βάζουμε το shell στο δικό του process group
        shell_pgid = getpid();
        if (setpgid(shell_pgid, shell_pgid) < 0) {
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }

        // Παίρνουμε τον έλεγχο του τερματικού
        tcsetpgrp(shell_terminal, shell_pgid);
    }
    
    // Ρύθμιση του SIGCHLD handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART; // Για να επανεκκινούνται οι system calls
    sigaction(SIGCHLD, &sa, NULL);
    
    init_jobs(); // Καθαρισμός του πίνακα jobs
}






// Εντολή 'jobs': Εκτυπώνει όλες τις ενεργές εργασίες
void execute_jobs() {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].id != 0) {
            const char *state_str = (jobs[i].state == JOB_STOPPED) ? "Stopped" : "Running";
            printf("[%d] %-10s %s\n", jobs[i].id, state_str, jobs[i].command);
        }
    }
}

// Εντολή 'fg %N': Φέρνει μια εργασία στο προσκήνιο
void execute_fg(char *arg) {
    if (arg == NULL || arg[0] != '%') {
        printf("Usage: fg %%<job_id>\n");
        return;
    }
    
    int job_id = atoi(arg + 1); // Παίρνουμε τον αριθμό μετά το '%'
    Job *job = find_job_by_id(job_id);
    
    if (job == NULL) {
        printf("fg: %s: no such job\n", arg);
        return;
    }

    // 1. Συνεχίζουμε την εργασία αν είναι σταματημένη
    if (job->state == JOB_STOPPED) {
        kill(-job->pgid, SIGCONT); // Στέλνουμε SIGCONT σε όλο το group
        job->state = JOB_RUNNING;
    }

    // 2. Δίνουμε τον έλεγχο του τερματικού στην εργασία
    tcsetpgrp(shell_terminal, job->pgid);

    // 3. Περιμένουμε να τελειώσει ή να ξανα-σταματήσει
    int status;
    waitpid(-job->pgid, &status, WUNTRACED);

    // 4. Παίρνουμε πίσω τον έλεγχο του τερματικού
    tcsetpgrp(shell_terminal, shell_pgid);

    // 5. Ελέγχουμε τι συνέβη
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        remove_job(job->pgid); // Τελείωσε
    } else if (WIFSTOPPED(status)) {
        job->state = JOB_STOPPED; // Σταμάτησε ξανά (π.χ. χρήστης πάτησε πάλι Ctrl-Z)
        printf("\n[%d]+ Stopped    %s\n", job->id, job->command);
    }
}

// Εντολή 'bg %N': Συνεχίζει μια εργασία στο παρασκήνιο
void execute_bg(char *arg) {
    if (arg == NULL || arg[0] != '%') {
        printf("Usage: bg %%<job_id>\n");
        return;
    }

    int job_id = atoi(arg + 1);
    Job *job = find_job_by_id(job_id);

    if (job == NULL) {
        printf("bg: %s: no such job\n", arg);
        return;
    }

    if (job->state == JOB_STOPPED) {
        // Της λέμε να συνεχίσει
        kill(-job->pgid, SIGCONT); 
        job->state = JOB_RUNNING;
        printf("[%d]+ %s &\n", job->id, job->command);
    }
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
            // Περνάμε το δεύτερο όρισμα (π.χ. "%1")
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
