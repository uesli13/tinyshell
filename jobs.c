#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h> 
#include "jobs.h"

Job jobs[MAX_JOBS];

void init_jobs() {
    for (int i = 0; i < MAX_JOBS; i++) {
        jobs[i].id = 0; // 0 for empty slots
    }
}

// Add a new job to the job list
int add_job(pid_t pgid, char *command, int state) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].id == 0) { // Found an empty slot
            jobs[i].id = i + 1;
            jobs[i].pgid = pgid;
            jobs[i].state = state;
            strncpy(jobs[i].command, command, BUFFER_SIZE - 1);
            return i + 1; // Return the Job ID
        }
    }
    printf("Error: Too many jobs\n");
    return -1;
}

// Remove a job based on PID (process group ID)
void remove_job(pid_t pgid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pgid == pgid) {
            jobs[i].id = 0; // Mark slot as empty
            return;
        }
    }
}

// Find a job by Job ID (used by fg/bg commands)
Job* find_job_by_id(int id) {
    if (id <= 0 || id > MAX_JOBS) return NULL;
    if (jobs[id-1].id != 0) return &jobs[id-1];
    return NULL;
}

// Find a job by process group ID (used by the signal handler)
Job* find_job_by_pgid(pid_t pgid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pgid == pgid) return &jobs[i];
    }
    return NULL;
}

// SIGCHLD handler (called when a child terminates or stops)
void sigchld_handler(int sig) {
    int status;
    pid_t pid;

    // WNOHANG: return immediately if there are no terminated children
    // WUNTRACED: also notify if a child has stopped (Ctrl-Z)
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        Job *job = find_job_by_pgid(pid);
        
        if (job) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                // The child terminated
                remove_job(pid);
            } 
            else if (WIFSTOPPED(status)) {
                // The child stopped (Ctrl-Z)
                job->state = JOB_STOPPED;
                printf("\n[%d]+ Stopped    %s\n", job->id, job->command);
            }
        }
    }
}

// 'jobs' command: print all active jobs
void execute_jobs() {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].id != 0) {
            const char *state_str = (jobs[i].state == JOB_STOPPED) ? "Stopped" : "Running";
            printf("[%d] %-10s %s\n", jobs[i].id, state_str, jobs[i].command);
        }
    }
}

// 'fg %N' command: bring a job to the foreground
void execute_fg(char *arg) {
    if (arg == NULL || arg[0] != '%') {
        printf("Usage: fg %%<job_id>\n");
        return;
    }
    
    int job_id = atoi(arg + 1); // Get the number after '%'
    Job *job = find_job_by_id(job_id);
    
    if (job == NULL) {
        printf("fg: %s: no such job\n", arg);
        return;
    }

    // Continue the job if it is stopped
    if (job->state == JOB_STOPPED) {
        kill(-job->pgid, SIGCONT);  // Send SIGCONT to the whole group
        job->state = JOB_RUNNING;
    }

    // Give terminal control to the job
    tcsetpgrp(shell_terminal, job->pgid);

    // Wait for it to finish or stop again
    int status;
    waitpid(-job->pgid, &status, WUNTRACED);

    //Take back control of the terminal
    tcsetpgrp(shell_terminal, shell_pgid);

    // Check what happened
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        remove_job(job->pgid); // Job finished
    } else if (WIFSTOPPED(status)) {
        job->state = JOB_STOPPED; // Job stopped again (e.g. Ctrl-Z)
        printf("\n[%d]+ Stopped    %s\n", job->id, job->command);
    }
}

// 'bg %N' command: continue a job in the background
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
        // Tell the job to continue
        kill(-job->pgid, SIGCONT); 
        job->state = JOB_RUNNING;
        printf("[%d]+ %s &\n", job->id, job->command);
    }
}