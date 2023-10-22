
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

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
#include <stdbool.h>

#define INP_SIZE 100


struct Job {    //struct/class for every single job
    int pid;    //pid from fork
    int job_id; //index in jobs, start from 1
    int state;  //0 = foreground, 1 = background, 2 = stopped, 3 = terminated
    char user_inp[INP_SIZE];
};

struct Job jobs[50];
int new_job = 1;

int fgid = 0;

void printjobs(){
    for(int i = 1; i < new_job; ++i){
        char curr_state[100];
        if(jobs[i].state == 0){
            strcpy(curr_state, "foreground");
        }else if(jobs[i].state == 1){
            strcpy(curr_state, "background");
        }else if(jobs[i].state == 2){
            strcpy(curr_state, "stopped");
        }else{
            continue;
        }
        printf("[%2d] (%6d) %s %s \n", jobs[i].job_id, jobs[i].pid, curr_state, jobs[i].user_inp);
    }
}

void waiting4pid(pid_t processID){
    //main process waiting for foreground process to finish

    int waitCondition = WUNTRACED | WCONTINUED;
    int currentState;
    pid_t childpid;
    childpid = waitpid(processID, &currentState, waitCondition);
    if(WIFSIGNALED(currentState)){
        printf("\n currentState = Child Exited!\n");
    }
    if(__WIFSTOPPED(currentState)){
        printf("\n currentState - Child stopped\n");
    }
    return;
}

void sig_handler(int signum){
    //used when quit, kills and reaps all child processes
    if (fgid > 0) {
        // Process is in the foreground, send the signal
        if(signum == SIGINT){
            for(int i = 0; i < new_job; i++){
                if(fgid == jobs[i].pid){
                    jobs[i].state = 3;
                    break;
                }
            }
            kill(fgid, SIGINT);
            waiting4pid(fgid);
        } else{
            kill(fgid, signum);
            for(int i = 0; i < new_job; i++){
                if(fgid == jobs[i].pid){
                    jobs[i].state = 2;
                    break;
                }
            }
        }
    }
}

void sigchild_handler(int signum){
    //reaps all child processes
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for(int i = 0; i < new_job; i++){
            if(pid == jobs[i].pid){
                jobs[i].state = 3;
                break;
            }
        }
        if (WIFEXITED(status)) {
            printf("Child process %d exited with status %d\n", pid, WEXITSTATUS(status));
            // Handle child process exit here
        }
        // You can add more checks for other child process events (e.g., stopped, continued).
    }
}

int main(){
    char inp[INP_SIZE];

    //signal init
    signal(SIGINT, sig_handler);
    signal(SIGCHLD, sigchild_handler);
    signal(SIGTSTP, sig_handler);

    while(true){
        printf("prompt > ");
        fflush(stdout);

        // parse argv from input
        fgets(inp, INP_SIZE, stdin);
        inp[strlen(inp)-1] = '\0';
        int inFileID=-1;
        int outFileID=-1;
        int saved_stdin;
        int saved_stdout;
        char* inp_copy = strdup(inp);
        mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO;
    
        char* input_redirect = strstr(inp, "<");
        if (input_redirect != NULL) {
            char* input_file = strtok(input_redirect + 1, " ");
            printf("Input file %s\n", input_file);
            inFileID = open(input_file, O_RDONLY, mode);
            if (inFileID < 0) {
                perror("Failed to open input file");
                continue;
            }
            saved_stdin = dup(0);
            dup2(inFileID, STDIN_FILENO);
        }
        
        char* output_redirect = strstr(inp_copy, ">");
        if (output_redirect != NULL) { // If > is in the command then output is set to the following file
            char* output_file;
            
            if (output_redirect[1] == '>') { // If there is 2 arrows, append to file
                output_file = strtok(output_redirect + 2, " ");
                outFileID = open(output_file, O_CREAT|O_APPEND|O_WRONLY, mode);
            } else { // If there is 1 arrow, overwrite the file
                output_file = strtok(output_redirect + 1, " ");
                outFileID = open(output_file, O_CREAT|O_WRONLY|O_TRUNC, mode);
            }
            if (outFileID < 0) {
                perror("Failed to open output file");
                continue;
            }
            saved_stdout = dup(1);
            dup2(outFileID, STDOUT_FILENO);
        }

        // tokenize input to make it into usable as argv
        char *tmp = strtok(inp, " ");
        if(tmp == NULL){
            continue;
        }
        char *argv[50];
        int i = 0;
        while (tmp != NULL) {
            argv[i] = strdup(tmp);
            i+=1;
            tmp = strtok(NULL, " ");
        }

        // built in commands - quit, cd, pwd, kill, jobs
        if(strcmp(argv[0], "quit") == 0){
            for(int i = 0; i < new_job; i++){
                if(jobs[i].state != 3){
                    kill(jobs[i].pid, SIGTERM);
                    waiting4pid(jobs[i].pid);
                }
            }
            kill(fgid, SIGTERM);
            break;
        } else if(strcmp(argv[0], "pwd") == 0){
            char res[INP_SIZE];
            getcwd(res, INP_SIZE);
            printf("%s\n", res);
        } else if(strcmp(argv[0], "cd") == 0){
            chdir(argv[1]);
        } else if(strcmp(argv[0], "jobs") == 0){
            printjobs();
        } else if(strcmp(argv[0], "kill") == 0){
            int p_id;
            int p_state;
            if (argv[1][0] == '%') {
                p_id = jobs[atoi(argv[1] + 1)].pid;
                p_state = jobs[atoi(argv[1] + 1)].state;
                jobs[atoi(argv[1] + 1)].state = 3;
            } else {
                p_id = atoi(argv[1]);
                for (int i = 1; i < new_job; i++) {
                    if (jobs[i].pid == p_id) {
                        p_state = jobs[i].state;
                        jobs[i].state = 3;
                        break;
                    }
                }
            }
            if(p_state == 2){
                kill(p_id, SIGCONT);
            }
            kill(p_id, SIGINT);
        }
        // Stopped foreground process is set to the background
        else if (strcmp(argv[0], "bg") == 0) {
            int p_id;
            if (argv[1][0] == '%') {
                // If the job id is given, it can be used to index directly to change the Job status
                p_id = jobs[atoi(argv[1] + 1)].pid;
                jobs[atoi(argv[1] + 1)].state = 1; // Set to background
            } else {
                // If pid is given, then find the matching Job in Jobs to change its status
                p_id = atoi(argv[1]);
                for (int i = 0; i < new_job; i++) {
                    if (jobs[i].pid == p_id) {
                        printf("CHANGING %d TO BACKGROUND\n", p_id);
                        jobs[i].state = 1; // Set to background
                        break;
                    }
                }
            }
            kill(p_id, SIGCONT); // Continue previously stopped process
        
        }
        // running background process is set to the foreground
        else if (strcmp(argv[0], "fg") == 0) {
            int p_id;
            if (argv[1][0] == '%') {
                p_id = jobs[atoi(argv[1] + 1)].pid;
                jobs[atoi(argv[1] + 1)].state = 0; // Set to foreground
            } else {
                p_id = atoi(argv[1]);
                for (int i = 0; i < new_job; i++) {
                    if (jobs[i].pid == p_id) {
                        jobs[i].state = 0; // Set to foreground
                        break;
                    }
                }
            }
            fgid = p_id;
            kill(p_id, SIGCONT);
            waiting4pid(p_id); // wait for new foreground process to terminate
            fgid = 0;
        }
        
        else{
            // execute inputted command if not one of previous built-ins
            int bg = 0;
            if(i > 0 && strcmp(argv[i-1],"&") == 0){
                // background process: bg=1
                argv[i-1] = NULL;
                bg = 1;
            } else{
                // foreground process: bg=0
                argv[i]=NULL;
            }
            int pid = fork();
            if(pid == 0){
                // try both execvp and execv
                if(execvp(argv[0], argv) < 0){
                    if(execv(argv[0], argv) < 0){
                        exit(1);
                    }
                }
            } else {
                // add new Job to jobs list using its pid
                printf("CHILD %d\n", pid);
                struct Job j = {pid, new_job, bg, *inp_copy};
                strcpy(j.user_inp, inp_copy);
                jobs[new_job] = j;
                new_job+=1;
                if(bg == 0){
                    // wait for process if its foreground
                    fgid = pid;
                    waiting4pid(pid);
                    fgid = 0;
                } else{
                    setpgid(pid, 1);
                }
            }
        }
        // Close opened files and revert I/O to STD
        if (inFileID >= 0) {
            dup2(saved_stdin, 0);
            close(inFileID);
        }
        if (outFileID >= 0) {
            dup2(saved_stdout, 1);
            close(outFileID);
        }
    }
}