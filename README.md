## Project Overview
TinyShell is a lightweight Unix shell implementation in C. It replicates core shell functionality—parsing user input, resolving paths, and executing commands—using standard POSIX system calls. 

## Key Features
* **REPL Interface:** Interactive prompt (`tinyshell>`) with advanced input parsing.
* **Command Execution:** Uses `fork`, `execve`, and `waitpid` to run programs.
* **Path Resolution:** Dynamically searches the `PATH` variable for executables.
* **I/O Redirection:** Full support for file redirection operators:
  * Input (`<`): Read from files.
  * Output (`>`): Write to files (overwrite).
  * Append (`>>`): Write to files (append).
  * Error (`2>`): Redirect standard error.
* **Pipelines:** Connects multiple commands using pipes (`|`) (e.g., `ls | grep .c`).
* **Built-ins:** Supports `exit` and EOF (Ctrl+D) termination.
* **Status Reporting:** Reports process exit codes upon completion.

## File Structure
* `tinyshell.c`: The main source code containing the shell logic, parsing, and execution engine.
* `Makefile`: Automation script for compiling and cleaning the project.
* `README.md`: Documentation and usage guide.

## Build Instructions

1.  **Build the project:**
    ```bash
    make
    ```

2.  **Clean up object files:**
    ```bash
    make clean
    ```

3.  **Remove all binaries and build files:**
    ```bash
    make purge
    ```

## Usage
Start the shell by running the executable:

```bash
./tinyshell
