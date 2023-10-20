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
int next_job_id = 0;

struct Job {
    char *name;
    int job_id;
    pid_t pid;
    int is_background;
    int is_stopped;
};

struct Job jobs[5];

void kill_everyone(){
    for(int i = 0; i < next_job_id; i++){
        kill(jobs[i].pid, SIGINT);
        }
}

void sigint_handler(int sig) {
    // Terminate the process if it is not a background process
    for(int i = 0; i < next_job_id; i++){
        printf("Current Job %d: %d\n", jobs[i].job_id, jobs[i].is_background);
        if (!jobs[i].is_background){
            printf("Process %d received signal %d\n", jobs[i].pid, sig);
            kill(jobs[i].pid, sig);
        }

    }
}

void sigchld_handler(int sig) {
    int status;
    pid_t child_pid;
    
    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Child process %d terminated.\n", child_pid);
        for(int i = 0; i < next_job_id; i++){
            if (jobs[i].pid == child_pid){
                jobs[i].is_stopped = 1;
            }
        }
    }
}

void sigtstp_handler(int sig) {
    printf("SIGTSTP\n (THIS SHOULD STOP THE FOREGROUND PROCESS)\n");
    for(int i = 0; i < next_job_id; i++){
        if (jobs[i].is_background == 0){
            printf("Process %d received signal %d\n", jobs[i].pid, sig);
            jobs[i].is_stopped = 1;
            kill(jobs[i].pid, sig);
            printf("Process %d stopped %d\n", jobs[i].pid, jobs[i].is_stopped);
        }
    }
}

void job_status(){
    for(int i = 0; i < next_job_id; i++){
        printf("[%d] (%d) %s %s", jobs[i].job_id, jobs[i].pid, jobs[i].is_stopped ?  "Stopped" : "Running", jobs[i].name);
        if (jobs[i].is_background){
            printf(" &\n");
        }
        else{
            printf("\n");
        }
    }
}


int main() {
    char input[80];
    char cwd[80];
    signal(SIGINT, sigint_handler); // when user presses ctrl-c
    signal(SIGCHLD, sigchld_handler); // when child process terminates
    signal(SIGTSTP, sigtstp_handler); // when user presses ctrl-z , stop the foreground process

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
                kill_everyone();
                exit(0);
            }

            else if (strcmp(token, "jobs") == 0){
                job_status();
            }

            // Input is executable file
            else if (access(token, X_OK) == 0) {
                pid = fork();
                char file[80];
                strcpy(file, input);
                jobs[next_job_id].job_id = next_job_id;
                jobs[next_job_id].pid = pid;
                jobs[next_job_id].is_background = 0;
                jobs[next_job_id].is_stopped = 0;
                jobs[next_job_id].name = file; 

                int c = WUNTRACED; // for some reason this fixes the ctrl-z issue where the terminal would stop working after using ctrl-z
                printf("File: %s\n", file);
                char *background_process = strtok(NULL, " ");
                // printf("Background process: %s\n", background_process);
                
                if (background_process != NULL && strcmp(background_process, "&") == 0){
                    printf("Background process on\n");
                    jobs[next_job_id].name = file;
                    jobs[next_job_id].is_background = 1;
                    setpgid(pid, 0); // set process group id to pid
                }
                
                // Child process
                if (pid == 0){
                    printf("Executing file\n");
                    char * args[] = {token, NULL};
                    if (execv(token, args) == -1){
                        perror("Error executing file");
                    }
                } 
                // Parent process (skip if background process)
                else if (!jobs[next_job_id].is_background){ //parent is waiting for child to finish
                    waitpid(pid, NULL, c);
                }
                next_job_id++;
            }
    }
    
    }
    return 0;
}