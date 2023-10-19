#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>

pid_t pid = -1; 
int next_job_id = 1;

struct Job {
    int job_id;
    pid_t pid;
    int is_background;
};


void sigint_handler(int sig) {
    printf("Process %d received signal %d\n", getpid(), sig);
    kill(pid, sig);
}

void sigchld_handler(int sig) {
    int status;
    pid_t child_pid;
    
    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Child process %d terminated.\n", child_pid);
    }
    // waitpid(pid, NULL, 0); 
}

void sigtstp_handler(int sig) {
    if (pid != -1) {
        // Toggle the foreground/background state of the job
        // Here, we're assuming the job information is stored in a struct named 'job'
        
        if (!job.is_background) {
            printf("Job %d stopped.\n", job.job_id);
            kill(job.pid, SIGTSTP); // Stop the background job
        }
    }
}


int main() {
    char input[80];
    char cwd[80];
    signal(SIGINT, sigint_handler);
    signal(SIGCHLD, sigchld_handler);
    signal(SIGTSTP, sigtstp_handler);

    while (1) {
        printf("prompt > ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            perror("Error reading input");
            exit(1);
        }
        // Remove trailing newline and replace with null terminator
        size_t input_len = strlen(input);
        if (input_len > 0 && input[input_len - 1] == '\n') {
            input[input_len - 1] = '\0';
        }

        // Tokenize input
        char *token = strtok(input, " ");      

        if (token != NULL) {
            // Change directory
            if (strcmp(token, "cd") == 0) {
                token = strtok(NULL, " "); // Get next token
                if (token == NULL){
                    printf("No path specified\n");
                }
                else if (chdir(token) != 0) { // chdir retuns 0 if change directory is successful
                    perror("Error changing directory");
                }
            }
            
            // Print working directory
            else if (strcmp(token, "pwd") == 0) {
                if (getcwd(cwd, sizeof(cwd)) != NULL) {
                    printf("%s\n", cwd);
                } else {
                    perror("Error getting current directory");
                }
            }
            // Quit shell
            else if (strcmp(token, "quit") == 0) {
                exit(0);
            }

            // Input is executable file
            else if (access(token, X_OK) == 0) {
                pid = fork();
                // int background = 0;
                char *file = token;

                struct Job job;
                job.job_id = next_job_id++;
                job.pid = pid;
                job.is_background = 0;
                printf("File: %s\n", file);
                char *background_process = strtok(NULL, " ");
                printf("Background process: %s\n", background_process);
                if (background_process != NULL && strcmp(background_process, "&") == 0){
                    printf("Background process on\n");
                    // background = 1;
                    job.is_background = 1;
                }
                // Child process
                if (pid == 0){
                    printf("Executing file\n");
                    char * args[] = {token, NULL};
                    // printf("JOB INFO: %d %d %d",job.job_id, job.pid, job.is_background);
                    if (execv(token, args) == -1){
                        perror("Error executing file");
                    }
                } 
                // Parent process (skip if background process)
                else if (!job.is_background){ //parent is waiting for child to finish
                    waitpid(pid, NULL, 0);
                }
            }
    }
    
    }
    return 0;
}