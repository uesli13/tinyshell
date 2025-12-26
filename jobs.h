#ifndef JOBS_H
#define JOBS_H
#include <sys/types.h>
#include "tinyshell.h"

// Καταστάσεις
#define JOB_RUNNING 1
#define JOB_STOPPED 2
#define JOB_DONE    3

typedef struct {
    int id;
    pid_t pgid;
    char command[BUFFER_SIZE];
    int state;
} Job;


void init_jobs();
int add_job(pid_t pgid, char *command, int state);
void remove_job(pid_t pgid);
Job* find_job_by_id(int id);
Job* find_job_by_pgid(pid_t pgid);

// Built-in commands
void execute_jobs();
void execute_fg(char *arg);
void execute_bg(char *arg);

// Signal Handler
void sigchld_handler(int sig);

#endif