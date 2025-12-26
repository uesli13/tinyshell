# TinyShell

**TinyShell** is a lightweight, custom Unix shell implementation in C. It replicates core shell functionalities including command execution, I/O redirection, pipelines, and advanced job control, built using standard POSIX system calls.

## Project Overview

This project was developed as part of the "Operating Systems" course. It demonstrates a deep understanding of process management, signal handling, and system programming in Linux. The shell has been implemented in three phases, evolving from a simple command runner to a fully interactive shell with job control capabilities.

## Key Features

### 1. Command Execution
* Executes standard Unix commands (e.g., `ls`, `grep`, `sleep`).
* Dynamically searches the `PATH` environment variable for executables.
* Supports arguments and flags.

### 2. I/O Redirection
Full support for standard stream redirection:
* **Input (`<`):** Read input from a file.
* **Output (`>`):** Write output to a file (overwrite).
* **Append (`>>`):** Append output to a file.
* **Error (`2>`):** Redirect standard error to a file.

### 3. Pipelines
* Supports chaining multiple commands using pipes (`|`).
* Example: `cat file.txt | grep "search" | sort`

### 4. Job Control (Phase 3)
* **Background Execution (`&`):** Run commands in the background without blocking the shell.
* **Process Groups:** Isolates jobs in their own process groups for proper signal handling.
* **Signal Handling:**
    * `Ctrl-C` (SIGINT): Interrupts the foreground job only.
    * `Ctrl-Z` (SIGTSTP): Stops (pauses) the foreground job.
* **Job Management Commands:**
    * `jobs`: Lists all active (running or stopped) jobs.
    * `fg %N`: Brings job N to the foreground.
    * `bg %N`: Resumes stopped job N in the background.

## Build Instructions

To compile the project, run the following command in the terminal:

```bash
make