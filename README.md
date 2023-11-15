Project Overview
This repository contains a custom shell implemented in C, designed to mimic the functionality of traditional Unix shells with added features. The shell supports a range of built-in commands, execution of local executables, and advanced job control, providing an interactive command-line interface for users to interact with the operating system.

Key Features
Command-Line Interface: A user-friendly interface for executing built-in commands and local executables.
Advanced Job Control: Manage foreground and background processes efficiently, with the ability to interrupt, resume, and switch between them.
I/O Redirection: Redirect standard input and output to and from files, along with managing file permissions for secure and flexible file operations.

Built-in Commands:

cd [directory]: Changes the current working directory. If the directory is .., it moves up one level in the directory tree.
pwd: Prints the current working directory.
quit: Exits the shell.
Execution of Local Executables:

Executing a program like hello: Runs the hello executable located in the current directory. If the program name is followed by an ampersand (&), it runs as a background job.
Job Control Commands:

jobs: Lists all current jobs with their status (running or stopped) and job IDs.
bg [job_id|pid]: Continues a stopped job in the background.
fg [job_id|pid]: Brings a job to the foreground, either from a stopped or background state.
kill [job_id|pid]: Terminates the specified job.
Signal Handling:

ctrl-C: Interrupts and terminates the foreground job.
ctrl-Z: Stops the foreground job, moving it to a stopped state.
I/O Redirection:

command > file: Redirects the output of command to file, overwriting it.
command >> file: Redirects the output of command to file, appending it to the end.
command < file: Uses file as the standard input for command.

