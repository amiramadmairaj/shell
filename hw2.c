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

void kill_everyone() {
    for (int i = 0; i < next_job_id; i++) {
        kill(jobs[i].pid, SIGINT);
    }
}

void remove_job(int job_id) {
    free(jobs[job_id].name); // free memory for name
    for (int j = job_id; j < next_job_id - 1; j++) {
        jobs[j] = jobs[j + 1];
        jobs[j].job_id = j;

    }
    next_job_id--;
}

void sigint_handler(int sig) { // when ctrl-c is pressed
    for (int i = 0; i < next_job_id; i++) {
        if (!jobs[i].is_background) {
            printf("Process %d received signal %d\n", jobs[i].pid, sig);
            kill(jobs[i].pid, sig);
            remove_job(i);
        }
    }
}

void sigchld_handler(int sig) { // terminates and reaps child processes
    int status;
    pid_t child_pid;
    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (child_pid > 0) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                printf("Reaped child process id %d\n", child_pid);
                for (int i = 0; i < next_job_id; i++) {
                    if (jobs[i].pid == child_pid) {
                        remove_job(i);
                    }
                }
            }

        }
    }
}

void sigtstp_handler(int sig) { // when ctrl-z is pressed
    printf("(THIS SHOULD STOP THE FOREGROUND PROCESS)\n");
    for (int i = 0; i < next_job_id; i++) {
        if (!jobs[i].is_background) {
            jobs[i].is_stopped = 1; // update jobs status to stopped
            kill(jobs[i].pid, sig);
        }
    }
}

void job_status() {
    for (int i = 0; i < next_job_id; i++) {
        printf("[%d] (%d) %s %s", jobs[i].job_id, jobs[i].pid, jobs[i].is_stopped ?  "Stopped" : "Running", jobs[i].name);
        if (jobs[i].is_background) {
            printf(" &\n");
        } else {
            printf("\n");
        }
    }
}

void change_directory(const char* path) {
    if (chdir(path) != 0) {
        perror("Error changing directory");
    }
}

void print_working_directory() {
    char cwd[80];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("Error getting current directory");
    }
}

void execute_file(char* token) {
    printf("INPUT FOR EXC FILE%s\n", token);
    pid = fork();
    char file_name[80];
    strcpy(file_name, token);
    // copy input to file so we keep file name handy
    jobs[next_job_id].job_id = next_job_id;
    jobs[next_job_id].pid = pid;
    jobs[next_job_id].is_background = 0;
    jobs[next_job_id].is_stopped = 0;
    jobs[next_job_id].name = strdup(file_name);
    
    int c = WUNTRACED;

    char* background_process = strtok(NULL, " ");
    printf("BACKGROUND PROCESS: %s\n", background_process);
    if (background_process != NULL && strcmp(background_process, "&") == 0) {
        jobs[next_job_id].is_background = 1;
        setpgid(pid, 0);
    }

    if (pid == 0) {
        char* args[] = {token, NULL};
        if (execv(token, args) == -1) {
            perror("Error executing file");
        }
    } else if (!jobs[next_job_id].is_background) {
        waitpid(pid, NULL, c);
    }
    next_job_id++;
}

void fg(char* token){
    int job_id;
    token = strtok(NULL, " ");
    if (token != NULL && token[0] == '%') {
        token = token + 1;
        job_id = atoi(token);
        printf("JOB ID from percent: %d\n", job_id);
    }
    else{
        int pid = atoi(token);
        for (int i = 0; i < next_job_id; i++) {
            if (jobs[i].pid == pid) {
                job_id = jobs[i].job_id;
                break;
            }
        }
    }
    for(int i = 0; i < next_job_id; i++){
        if(jobs[i].job_id == job_id){
            jobs[i].is_background = 0; // set to foreground
            jobs[i].is_stopped = 0; // set to running
            kill(jobs[i].pid, SIGCONT); // send signal to continue
            waitpid(jobs[i].pid, NULL, WUNTRACED); // wait for process to finish (foreground)
            remove_job(i); // Remove the job after it finishes

            break;
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

        size_t input_len = strlen(input);
        if (input_len > 0 && input[input_len - 1] == '\n') {
            input[input_len - 1] = '\0';
        }

        char* token = strtok(input, " ");

        if (token != NULL) {
            // cd command
            if (strcmp(token, "cd") == 0) {
                token = strtok(NULL, " ");
                if (token == NULL) {
                    printf("No path specified\n");
                } else {
                    change_directory(token);
                }
            // pwd command
            } else if (strcmp(token, "pwd") == 0) {
                print_working_directory();
            // quit command
            } else if (strcmp(token, "quit") == 0) {
                kill_everyone();
                exit(0);
            // jobs command
            } else if (strcmp(token, "jobs") == 0) {
                job_status();
            // fg command
            } else if (strcmp(token, "fg") == 0) {
                fg(token);
           } else if (access(token, X_OK) == 0) {
                execute_file(token);
            }
        }
    }
    return 0;
}
