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
// pid_t foreground_pid = -1; 

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
                        break;
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
            jobs[i].is_stopped = 1; 
            kill(jobs[i].pid, sig);
        }
    }
}



void job_status() {
    for (int i = 0; i < next_job_id; i++) {
        // printf("IS IT STOPPED? %d\n", jobs[i].is_stopped);
        if (jobs[i].is_background == 1){
            printf("[%d] (%d) Running %s &\n", jobs[i].job_id, jobs[i].pid, jobs[i].name);
        }
        else{
            printf("[%d] (%d) Stopped %s\n", jobs[i].job_id, jobs[i].pid, jobs[i].name);
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
    pid = fork();
    char file_name[80];
    strcpy(file_name, token);
    // copy input to file so we keep the file name handy
    jobs[next_job_id].job_id = next_job_id;
    jobs[next_job_id].pid = pid;
    jobs[next_job_id].is_background = 0;
    jobs[next_job_id].is_stopped = 0;
    jobs[next_job_id].name = strdup(file_name);

    char* background_process = strtok(NULL, " ");

    if (background_process != NULL && strcmp(background_process, "&") == 0) {
        jobs[next_job_id].is_background = 1;
        setpgid(pid, 0);
    }

    if (pid == 0) {
        char* args[] = {token, NULL};
        if (execvp(token, args) < 0) {
            if (execv(token, args) < 0) {
                perror("Error executing file");
            }
        }
    }

    if (!jobs[next_job_id].is_background) {
        int c = WUNTRACED | WCONTINUED;
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
    int c = WUNTRACED|WCONTINUED;
    for(int i = 0; i <= next_job_id; i++){
        if(jobs[i].job_id == job_id){
            jobs[i].is_background = 0; // set to foreground
            jobs[i].is_stopped = 0; // set to running
            kill(jobs[i].pid, SIGCONT); // send signal to continue
            int currentState;
            pid_t childpid;
            childpid = waitpid(jobs[i].pid, &currentState, c);
            if(WIFEXITED(currentState)){
                remove_job(i);
            }
        }
    }
}

//Your code should accept the command kill <job_id|pid> that terminates a job by sending it a SIGINT signal using the kill() system call. Be sure to reap a terminated process
void killer(char* token){
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
            kill(jobs[i].pid, SIGINT); // send signal to kill process
            setpgid(jobs[i].pid, 0);
            break;
        }
    }

}

//changes the state of a job currently in the Stopped state to the Background/Running state
void bg(char* token){
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
            jobs[i].is_background = 1; // set to background
            jobs[i].is_stopped = 0; // set to running
            setpgid(jobs[i].pid, 0); 
            kill(jobs[i].pid, SIGCONT); // send signal to continue
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
            // bg command
            } else if (strcmp(token, "bg") == 0) {
                bg(token);
            // kill command
            }else if (strcmp(token, "kill") == 0) {
                killer(token);
            // execute file
        } else if (access(token, X_OK) == 0) {
                execute_file(token);
        }
    }
}
    return 0;
}