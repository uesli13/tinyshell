## Project Overview
TinyShell is a lightweight Unix shell implementation in C. It replicates core shell functionality—parsing user input, resolving paths, and executing commands—using standard POSIX system calls.

## Key Features
* **REPL Interface:** Interactive prompt (`tinyshell>`) with input parsing.
* **Command Execution:** Uses `fork`, `execve`, and `waitpid` to run programs.
* **Path Resolution:** Dynamically searches the `PATH` variable for executables.
* **Built-ins:** Supports `exit` and EOF termination.
* **Status Reporting:** Reports process exit codes upon completion.

## File Structure
* `tinyshell.c`: The main source code containing the shell logic.
* `Makefile`: Automation script for compiling and cleaning the project.
* `README.md`: Documentation and usage guide.

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
```