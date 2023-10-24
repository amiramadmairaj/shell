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
  char * name;
  int job_id;
  pid_t pid;
  int is_background;
  int is_stopped;
};

struct Job jobs[5];

void killer(char * token) {
  int job_id;
  token = strtok(NULL, " ");
  if (token != NULL && token[0] == '%') {
    token = token + 1;
    job_id = atoi(token);
  } else {
    int pid = atoi(token);
    for (int i = 0; i < next_job_id; i++) {
      if (jobs[i].pid == pid) {
        job_id = jobs[i].job_id;
        break;
      }
    }
  }
  for (int i = 0; i < next_job_id; i++) {
    if (jobs[i].job_id == job_id) {
      kill(jobs[i].pid, SIGINT); // send signal to kill process
      setpgid(jobs[i].pid, 0);
      break;
    }
  }

}

void kill_everyone() {
  for (int i = 0; i < next_job_id; i++) {
    kill(jobs[i].pid, SIGINT);
  }
}

void remove_job(int job_id) {
  for (int i = job_id; i < next_job_id - 1; i++) {
    jobs[i] = jobs[i + 1];
    jobs[i].job_id--;
  }
  next_job_id--;
}

void sigint_handler(int sig) { // when ctrl-c is pressed
  for (int i = 0; i <= next_job_id; i++) {
    if (!jobs[i].is_background) {
      kill(jobs[i].pid, sig);
      remove_job(i);
    }
  }
}

void sigchld_handler(int sig) { // terminates and reaps child processes
  int status;
  pid_t child_pid;
  while ((child_pid = waitpid(-1, & status, WNOHANG)) > 0) {
    if (child_pid > 0) {
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
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
  for (int i = 0; i < next_job_id; i++) {
    if (jobs[i].is_background == 0) {
      jobs[i].is_stopped = 1;
      kill(jobs[i].pid, sig);
    }
  }
}

void job_status_to_file(const char * filename, int append) {
  FILE * file;
  if (append) {
    file = fopen(filename, "a");
  } else {
    file = fopen(filename, "w");
  }

  if (file == NULL) {
    perror("Error opening file");
    return;
  }

  for (int i = 0; i < next_job_id; i++) {
    if (jobs[i].is_background == 1) {
      fprintf(file, "[%d] (%d) Running %s &\n", jobs[i].job_id, jobs[i].pid, jobs[i].name);
    } else {
      fprintf(file, "[%d] (%d) Stopped %s\n", jobs[i].job_id, jobs[i].pid, jobs[i].name);
    }
  }

  fclose(file);
}

void job_status() {
  for (int i = 0; i < next_job_id; i++) {
    if (jobs[i].is_background == 1) {
      printf("[%d] (%d) Running %s &\n", jobs[i].job_id, jobs[i].pid, jobs[i].name);
    } else {
      printf("[%d] (%d) Stopped %s\n", jobs[i].job_id, jobs[i].pid, jobs[i].name);
    }
  }
}

void change_directory(const char * path) {
  if (chdir(path) != 0) {
    perror("Error changing directory");
  }
}

void print_working_directory_to_file(const char * filename, int append_to_file) {
  FILE * file;
  if (append_to_file) {
    file = fopen(filename, "a");
  } else {
    file = fopen(filename, "w");
  }

  if (file == NULL) {
    perror("Error opening file");
    return;
  }

  char cwd[80];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    fprintf(file, "%s\n", cwd);
  } else {
    perror("Error getting current directory");
  }

  fclose(file);
}

void print_working_directory() {
  char cwd[80];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    printf("%s\n", cwd);
  } else {
    perror("Error getting current directory");
  }
}

void execute_file_2_outfile(char * token, char * outputfile, int append) {
  pid_t pid;
  pid = fork();
  if (pid < 0) {
    perror("Fork failed");
    exit(1);
  }
  char * background_process = strtok(NULL, " ");
  if (pid == 0) {
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    int outFileID;
    if (append) {
      outFileID = open(outputfile, O_WRONLY | O_CREAT | O_APPEND, mode);
    } else {
      outFileID = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, mode);
    }
    if (outFileID < 0) {
      perror("Error opening output file");
      exit(1);
    }
    dup2(outFileID, STDOUT_FILENO); // Redirect STDOUT to the output file

    char * args[] = {
      token,
      NULL
    };
    if (execv(token, args) < 0) {
      perror("Error executing file");
      exit(1);
    }
  }

  int c = WUNTRACED;
  waitpid(pid, NULL, c);
}

void execute_file_2_inputfile(char * token, char * inputfile, int output2fileafter, char * outputfile, int append) {
  pid_t pid;
  pid = fork();
  if (pid < 0) {
    perror("Fork failed");
    exit(1);
  }
  char * background_process = strtok(NULL, " ");
  if (pid == 0) {
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    int inFileID = open(inputfile, O_RDONLY, mode);
    if (inFileID < 0) {
      perror("Error opening input file");
      exit(1);
    }
    dup2(inFileID, STDIN_FILENO); // Redirect STDIN to the input file

    // Check if output should be redirected to a file
    if (output2fileafter == 1) {
      int outFileID;
      if (append) {
        outFileID = open(outputfile, O_WRONLY | O_CREAT | O_APPEND, mode);
      } else {
        outFileID = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, mode);
      }
      if (outFileID < 0) {
        perror("Error opening output file");
        exit(1);
      }
      dup2(outFileID, STDOUT_FILENO); // Redirect STDOUT to the output file
    }

    char * args[] = {
      token,
      NULL
    };
    if (execv(token, args) < 0) {
      perror("Error executing file");
      exit(1);
    }
  }

  int c = WUNTRACED;
  waitpid(pid, NULL, c);
}

void execute_file(char * token, int bg_process) {
  pid = fork();
  char file_name[80];
  strcpy(file_name, token);
  // copy input to file so we keep the file name handy
  jobs[next_job_id].job_id = next_job_id;
  jobs[next_job_id].pid = pid;
  jobs[next_job_id].is_background = 0;
  jobs[next_job_id].is_stopped = 0;
  jobs[next_job_id].name = strdup(file_name);

  char * background_process = strtok(NULL, " ");

  if (bg_process == 1) {
    jobs[next_job_id].is_background = 1;
    setpgid(pid, 0);
  }

  if (pid == 0) {
    char * args[] = {
      token,
      NULL
    };
    //FIX THIS MAY NOT WORK ./counter
    if (execvp(token, args) < 0) {
      if (execv(token, args) < 0) {
        perror("Error executing file");
      }
    }
  }

  if (!jobs[next_job_id].is_background) {
    int c = WUNTRACED;
    int currentState;
    waitpid(jobs[next_job_id].pid, & currentState, c);
    if (WIFEXITED(currentState)) {
      remove_job(jobs[next_job_id].job_id);
    }
  }

  next_job_id++;
}

void fg(char * token) {
  int job_id;
  token = strtok(NULL, " ");
  if (token != NULL && token[0] == '%') {
    token = token + 1;
    job_id = atoi(token);
    printf("JOB ID from percent: %d\n", job_id);
  } else {
    int pid = atoi(token);
    for (int i = 0; i < next_job_id; i++) {
      if (jobs[i].pid == pid) {
        job_id = jobs[i].job_id;
        break;
      }
    }
  }
  int c = WUNTRACED;
  for (int i = 0; i <= next_job_id; i++) {
    if (jobs[i].job_id == job_id) {
      jobs[i].is_background = 0; // set to foreground
      jobs[i].is_stopped = 0; // set to running
      kill(jobs[i].pid, SIGCONT); // send signal to continue
      int currentState;

      waitpid(jobs[i].pid, & currentState, c);
      if (WIFEXITED(currentState)) {
        remove_job(i);
      }
    }
  }
}

//changes the state of a job currently in the Stopped state to the Background/Running state
void bg(char * token) {
  int job_id;
  token = strtok(NULL, " ");
  getpgid(0);
  if (token != NULL && token[0] == '%') {
    token = token + 1;
    job_id = atoi(token);
    setpgid(jobs[job_id].pid, 0);
    jobs[job_id].is_background = 1;
    jobs[job_id].is_stopped = 0; // set to running
    kill(jobs[job_id].pid, SIGCONT);
    signal(SIGCHLD, sigchld_handler);
  } 
  else {
    int pid = atoi(token);
    job_id = -1;
    for (int i = 0; i < next_job_id; i++) {
      if (jobs[i].pid == pid) {
        job_id = jobs[i].job_id;
        break;
      }
    }
    if (job_id != -1) {
      setpgid(jobs[job_id].pid, 0);
      jobs[job_id].is_stopped = 0; // set to running
      jobs[job_id].is_background = 1;
      kill(jobs[job_id].pid, SIGCONT);
      signal(SIGCHLD, sigchld_handler);
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

    char * token = strtok(input, " ");

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
        char * direction = strtok(NULL, " ");
        int append_to_file = 0;
        if (direction != NULL && (strcmp(direction, ">") == 0 || strcmp(direction, ">>") == 0)) {
          if (strcmp(direction, ">>") == 0) {
            append_to_file = 1;
          }
          char * file = strtok(NULL, " ");
          print_working_directory_to_file(file, append_to_file);
        } else {
          print_working_directory();
        }
        // quit command
      } else if (strcmp(token, "quit") == 0) {
        kill_everyone();
        exit(0);
        // jobs command
      } else if (strcmp(token, "jobs") == 0) {
        // check if we need to output to file
        char * direction = strtok(NULL, " ");
        int append_to_file = 0;
        if (direction != NULL && (strcmp(direction, ">") == 0 || strcmp(direction, ">>") == 0)) {
          if (strcmp(direction, ">>") == 0) {
            int append_to_file = 1;
          }
          char * file = strtok(NULL, " ");
          job_status_to_file(file, append_to_file);
        }
        job_status();

        // fg command
      } else if (strcmp(token, "fg") == 0) {
        fg(token);
        // bg command
      } else if (strcmp(token, "bg") == 0) {
        bg(token);
        // kill command
      } else if (strcmp(token, "kill") == 0) {
        killer(token);
        // execute file
      } else if (access(token, X_OK) == 0) {
        char * direction = strtok(NULL, " ");
        int append_to_file = 0;
        if (direction != NULL && strcmp(direction, "&") == 0) { // if background process (no direction possible)
          execute_file(token, 1); // background process on
        } else if (direction == NULL) {
          execute_file(token, 0); // background process off
        } else if (direction != NULL && strcmp(direction, ">") == 0 || strcmp(direction, ">>") == 0) {
          if (strcmp(direction, ">>") == 0) {
            append_to_file = 1;
          }
          char * file = strtok(NULL, " ");
          execute_file_2_outfile(token, file, append_to_file);
        } else if (direction != NULL && strcmp(direction, "<") == 0) {
          char * file = strtok(NULL, " ");
          char * file2;
          char * direction2 = strtok(NULL, " ");
          int output_2_file_after = 0;
          int append_to_file = 0;
          if (direction2 != NULL && (strcmp(direction2, ">") == 0 || strcmp(direction2, ">>") == 0)) {
            if (strcmp(direction2, ">>") == 0) {
              append_to_file = 1;
            }
            output_2_file_after = 1;
            file2 = strtok(NULL, " ");
          }
          execute_file_2_inputfile(token, file, output_2_file_after, file2, append_to_file);
        }
      }
    }
  }
  return 0;
}