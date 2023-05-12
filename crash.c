#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

// change all printfs to write
// 2 spaces in print outs

#define MAXJOBS 32
#define MAXLINE 1024

char **environ;
int jobNumber = 0;

typedef struct {
    int number;  // job number
    int pid; // process ID
    char command[100]; // command that created the job
    bool status; // job status: running (1) supended (0)
    bool inForeGround; 
} job;


job jobs[MAXJOBS]; // array of job structures
int num_jobs = 0; // number of jobs currently in the jobs array


void sort_jobs() {
    int i, j;
    job tmp;

    // Bubble sort algorithm
    for (i = 0; i < MAXJOBS; i++) {
        for (j = 0; j < MAXJOBS - 1; j++) {
            if (jobs[j].number > jobs[j+1].number) {
                tmp = jobs[j];
                jobs[j] = jobs[j+1];
                jobs[j+1] = tmp;
            }
        }
    }
    // Move non-positive integers to end of array 
    int end_idx = MAXJOBS - 1;
    for (i = 0; i <= end_idx; i++) {
        if (jobs[i].number <= 0) {
            // Swap element with end of array
            tmp = jobs[i];
            jobs[i] = jobs[end_idx];
            jobs[end_idx] = tmp;
            end_idx--;
            i--;
        }
    }
}

bool inForeGround(){

    for (int i = 0; i < MAXJOBS; i++){
        if(jobs[i].inForeGround == 1){    
            return true;
        }
    }
    return false;
}

bool jobSuspended(int pid){

    for (int i = 0; i < MAXJOBS; i++){

        if(jobs[i].pid == pid && jobs[i].status == 0){
            return true;
        }
        
    }
    return false;
}

void waitForFGJob(int pid) {

    int status;
    waitpid(pid, &status, WUNTRACED);

    struct timespec timespc, remember;
    timespc.tv_sec = 0;
    timespc.tv_nsec = 100000; // 0.1ms

    while (nanosleep(&timespc, &remember) == -1) {
        timespc = remember;
        if (WIFSTOPPED(status)){
            break;
        }
    }
}
// might change to remove all finished jobs
void removeJobAndSort(int pid){
    
    for (int i = 0; i < MAXJOBS; i++){
        if(jobs[i].pid == pid && jobs[i].number > 0){    
            jobs[i].number = 0;
        }
        break;
    }
    sort_jobs();
}

void handle_sigchld(int sig) {
     
    int status;
    for (int i = 0; i < MAXJOBS; i++){
        if(jobs[i].number > 0){        
            int result = waitpid(jobs[i].pid, &status, WNOHANG);         
            if (result == 0) {
                // the child process is still running 
                continue;
            } else if (result == jobs[i].pid) {
                // the child process has exited and its status is in the 'status' variable
                if (WIFEXITED(status) && jobs[i].inForeGround == 0) {
                    char buffer[160];
                    sprintf(buffer, "[%d] (%d)  finished  %s\n", jobs[i].number, jobs[i].pid, jobs[i].command);
                    write(STDOUT_FILENO, buffer, strlen(buffer));
                    jobs[i].number = 0;

                } else if (WIFSIGNALED(status)) {
                    if (WCOREDUMP(status)) {
                        
                        char buffer[160];
                        sprintf(buffer, "[%d] (%d)  killed (core dumped)  %s\n", jobs[i].number, jobs[i].pid, jobs[i].command);
                        write(STDOUT_FILENO, buffer, strlen(buffer));
                    }
                    else if (WIFSTOPPED(status)){
                        char buffer[160];
                        sprintf(buffer, "[%d] (%d)  suspended  %s\n", jobs[i].number, jobs[i].pid, jobs[i].command);
                        write(STDOUT_FILENO, buffer, strlen(buffer));
                        jobs[i].status = 0;
                        jobs[i].inForeGround = false;

                    }
                    else {
                        char buffer[160];
                        sprintf(buffer, "[%d] (%d)  killed  %s\n", jobs[i].number, jobs[i].pid, jobs[i].command);
                        write(STDOUT_FILENO, buffer, strlen(buffer));
                    }
                    jobs[i].number = 0;
                }
            } else {
                // some error
            }
        }
    }
    sort_jobs();
}

void handle_sigtstp(int sig) {

    for (int i = 0; i < MAXJOBS; i++){

        if(jobs[i].inForeGround == 1 && jobs[i].number > 0){
            kill(jobs[i].pid, SIGTSTP);    
            jobs[i].status = 0;
            jobs[i].inForeGround = false;
            char msg[160];
            sprintf(msg, "[%d] (%d)  suspended  %s\n", jobs[i].number, jobs[i].pid,  jobs[i].command);
            write(STDOUT_FILENO, msg, strlen(msg));
        }
        break;
    }
    sort_jobs();
    
}

void handle_sigint(int sig) {
    
    for (int i = 0; i < MAXJOBS; i++){

        if(jobs[i].inForeGround == 1 && jobs[i].number > 0){
            kill(jobs[i].pid, SIGKILL);    
            char buffer[160];
            sprintf(buffer, "[%d] (%d)  killed  %s\n", jobs[i].number, jobs[i].pid, jobs[i].command);
            write(STDOUT_FILENO, buffer, strlen(buffer));
            jobs[i].number = 0;
        }
        break;
    }
    sort_jobs();
}

void handle_sigquit(int sig) {
    
    bool foreGroundExists = false;
    for (int i = 0; i < MAXJOBS; i++){
        if(jobs[i].inForeGround == 1 && jobs[i].number > 0){
            kill(jobs[i].pid, SIGQUIT);  

            char buffer[160];
            sprintf(buffer, "[%d] (%d)  killed (core dumped)  %s\n", jobs[i].number, jobs[i].pid, jobs[i].command);
            write(STDOUT_FILENO, buffer, strlen(buffer));
            jobs[i].number = 0;
            foreGroundExists = true;            
            break;
        }        
    }
    if (!foreGroundExists){
        exit(0);
    }
    sort_jobs();
}

void install_signal_handlers() {
    signal(SIGCHLD, handle_sigchld);
    signal(SIGINT, handle_sigint);
    signal(SIGQUIT, handle_sigquit);    
    signal(SIGTSTP, handle_sigtstp);

}

void spawn(const char **toks, bool bg) { // bg is true iff command ended with &
  
    if (num_jobs >= 32){
        char error_msg[60];
        sprintf(error_msg, "ERROR: too many jobs\n");
        write(STDERR_FILENO, error_msg, strlen(error_msg));
        return;
    } 

    if (bg == true){
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, NULL);    // blocking signals

        pid_t pid = fork();
        if (pid < 0) {
            char error_msg[60];
            sprintf(error_msg, "ERROR: cannot run %s\n", toks[0]);
            write(STDERR_FILENO, error_msg, strlen(error_msg));
            sigprocmask(SIG_UNBLOCK, &mask, NULL);
        }else if(pid == 0){
            sigprocmask(SIG_UNBLOCK, &mask, NULL);

            char *args[32];
            int i = 0;
            while (toks[i] != NULL){
                args[i] = strdup(toks[i]);
                i++;
            }
            args[i] = NULL;
            int exec = execvp(args[0], args);
            if (exec < 0){                
                char error_msg[60];
                sprintf(error_msg, "ERROR: cannot run %s\n", toks[0]);
                write(STDERR_FILENO, error_msg, strlen(error_msg));
            }
        }
        else {
            job newJob;
            newJob.number = ++jobNumber;
            newJob.pid = pid;
            strcpy(newJob.command, toks[0]);
            newJob.status = 1;
            newJob.inForeGround = false;
            jobs[31] = newJob;
            num_jobs++;
            sort_jobs();
            
            char msg[160];
            sprintf(msg, "[%d] (%d)  running  %s\n", newJob.number, newJob.pid, newJob.command);
            write(STDOUT_FILENO, msg, strlen(msg));
            sigprocmask(SIG_UNBLOCK, &mask, NULL);
            return;
        }
    }

    else {

        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, NULL);    // blocking signals

        pid_t pid = fork();

        if (pid == 0) {
            // Child process
            sigprocmask(SIG_UNBLOCK, &mask, NULL);

            setpgid(0, 0);  // Create new process group
            char *args[32];
            int i = 0;
            while (toks[i] != NULL){
                args[i] = strdup(toks[i]);
                i++;
            }
            args[i] = NULL;
            int exec = execvp(args[0], args);
            
            if (exec < 0){                
                char error_msg[60];
                sprintf(error_msg, "ERROR: cannot run %s\n", toks[0]);
                write(STDERR_FILENO, error_msg, strlen(error_msg));
            }

        } else if (pid > 0) {
            // Parent process
            job newJob;
            newJob.number = ++jobNumber;
            newJob.pid = pid;
            strcpy(newJob.command, toks[0]);
            newJob.status = 1;
            newJob.inForeGround = true;
            jobs[31] = newJob;
            num_jobs++;
            sort_jobs();

            sigprocmask(SIG_UNBLOCK, &mask, NULL);
            int status;
        
            waitForFGJob(pid);
            if(!jobSuspended(pid)){
                removeJobAndSort(pid); 
            }           

        } else {
            // Error handling
        }
    }
}

void cmd_jobs(const char **toks) {
    
    for (int i = MAXJOBS-1; i >=0; i--){
        if (jobs[i].number > 0){
            char status[10];
            if (jobs[i].status == 1) {strcpy(status, "running");} 
            else if (jobs[i].status == 0) {strcpy(status, "suspended");}
           
            char msg[160];
            sprintf(msg, "[%d] (%d)  %s  %s\n", jobs[i].number, jobs[i].pid, status, jobs[i].command);
            write(STDOUT_FILENO, msg, strlen(msg));
        }
    }
}

void cmd_fg(const char **toks) {

    if(toks[1] == NULL || toks[2]!= NULL){
        char msg[100];
        sprintf(msg,"ERROR: fg takes exactly one argument");
        write(STDERR_FILENO, msg, strlen(msg));
    }

    if (strncmp(toks[1], "%", 1) == 0) {   // if refering to job number
        char *endptr;
        int number = strtol(toks[1]+1, &endptr, 10);
        if (*endptr != '\0') {
            // error: the string was not completely converted to a number
        } else {

            for(int i = 0; i < MAXJOBS; i++){
                if (jobs[i].number == number && jobs[i].inForeGround == false){

                    if (jobs[i].status == 0){
                        kill(jobs[i].pid, SIGCONT);
                        jobs[i].status = 1;
                    }
                    jobs[i].inForeGround = true;
                    waitForFGJob(jobs[i].pid);

                    if(!jobSuspended(jobs[i].pid)){
                        removeJobAndSort(jobs[i].pid); 
                    }     
                    return;
                }
            }
            char error_msg[60];
            sprintf(error_msg, "ERROR: no job %%%d\n", number);
            write(STDERR_FILENO, error_msg, strlen(error_msg));
            return;
        }
    }
    
    else if (strncmp(toks[1], "%", 1) != 0) {  // if refering to PID
      
        char *endptr;
        int pid = strtol(toks[1], &endptr, 10);
        if (*endptr != '\0') {
            // error: the string was not completely converted to a number
        } else {

            for(int i = 0; i < MAXJOBS; i++){
                if (jobs[i].pid == pid && jobs[i].inForeGround == false){

                    if (jobs[i].status == 0){
                        kill(jobs[i].pid, SIGCONT);
                        jobs[i].status = 1;
                    }
                    jobs[i].inForeGround = true;
                    waitForFGJob(jobs[i].pid);

                    if(!jobSuspended(jobs[i].pid)){
                        removeJobAndSort(jobs[i].pid); 
                    } 
                    return;
                }
            }

            char error_msg[50];
            sprintf(error_msg, "ERROR: no PID %d\n", pid);
            write(STDERR_FILENO, error_msg, strlen(error_msg));
            return;
        }
    }
}

void cmd_bg(const char **toks) {
    
    if(toks[1] == NULL || toks[2]!= NULL){
        char msg[100];
        sprintf(msg,"ERROR: bg takes exactly one argument");
        write(STDERR_FILENO, msg, strlen(msg));
    }

    if (strncmp(toks[1], "%", 1) == 0) {   // if refering to job number
        char *endptr;
        int number = strtol(toks[1]+1, &endptr, 10);
        if (*endptr != '\0') {
            // error: the string was not completely converted to a number
        } else {

            for(int i = 0; i < MAXJOBS; i++){
                if (jobs[i].number == number ){

                    if (jobs[i].status == 0){
                        kill(jobs[i].pid, SIGCONT);
                        jobs[i].status = 1;
                    }
                    jobs[i].inForeGround = false;

                    return;
                }
            }
            char error_msg[60];
            sprintf(error_msg, "ERROR: no job %%%d\n", number);
            write(STDERR_FILENO, error_msg, strlen(error_msg));
            return;
        }
    }
    
    else if (strncmp(toks[1], "%", 1) != 0) {  // if refering to PID
      
        char *endptr;
        int pid = strtol(toks[1], &endptr, 10);
        if (*endptr != '\0') {
            // error: the string was not completely converted to a number
        } else {

            for(int i = 0; i < MAXJOBS; i++){
                if (jobs[i].pid == pid ){

                    if (jobs[i].status == 0){
                        kill(jobs[i].pid, SIGCONT);
                        jobs[i].status = 1;
                    }
                    jobs[i].inForeGround = false;
                    return;
                }
            }
            char error_msg[50];
            sprintf(error_msg, "ERROR: no PID %d\n", pid);
            write(STDERR_FILENO, error_msg, strlen(error_msg));
            return;
        }
    }
}

void cmd_nuke(const char **toks) {    
    
    if (toks[1] == NULL){

        sigset_t mask, prev_mask;
        sigfillset(&mask);
        sigprocmask(SIG_BLOCK, &mask, &prev_mask);
        int i = 31;
        while(i >=0 ){
            if (jobs[i].number > 0){
                kill(jobs[i].pid, SIGKILL);
                char buffer[136];
                sprintf(buffer, "[%d] (%d)  killed  %s\n", jobs[i].number, jobs[i].pid, jobs[i].command);

                write(STDOUT_FILENO, buffer, strlen(buffer));
                jobs[i].number = 0; 
            }
            i--;
        }
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        return;
    }

    else if (strncmp(toks[1], "%", 1) == 0) {   // if refering to job number
        char *endptr;
        int number = strtol(toks[1]+1, &endptr, 10);
        if (*endptr != '\0') {
            // error: the string was not completely converted to a number
        } else {
            
            sigset_t mask, prev_mask;
            sigfillset(&mask);
            sigprocmask(SIG_BLOCK, &mask, &prev_mask);
            int i = 0;
            while(i < MAXJOBS ){
                if (jobs[i].number  == number){
                    kill(jobs[i].pid, SIGKILL);
                    char buffer[136];
                    sprintf(buffer, "[%d] (%d) killed  %s\n", jobs[i].number, jobs[i].pid, jobs[i].command);

                    write(STDOUT_FILENO, buffer, strlen(buffer));
                    jobs[i].number = 0; 
                    sort_jobs();
                    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
                    return;
                }
                i++;
            }
            char error_msg[60];
            sprintf(error_msg, "ERROR: no job %%%d\n", number);
            write(STDERR_FILENO, error_msg, strlen(error_msg));
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);
            return;
        }
    }   

    else if (strncmp(toks[1], "%", 1) != 0) {  // if refering to PID
      
        char *endptr;
        int pid = strtol(toks[1], &endptr, 10);
        if (*endptr != '\0') {
            // error: the string was not completely converted to a number
        } else {
            
            sigset_t mask, prev_mask;
            sigfillset(&mask);
            sigprocmask(SIG_BLOCK, &mask, &prev_mask);
            int i = 0;
            while(i < MAXJOBS ){
                if (jobs[i].pid  == pid){
                    kill(jobs[i].pid, SIGKILL);
                    char buffer[136];
                    sprintf(buffer, "[%d] (%d)  killed  %s\n", jobs[i].number, jobs[i].pid, jobs[i].command);

                    write(STDOUT_FILENO, buffer, strlen(buffer));
                    jobs[i].number = 0; 
                    sort_jobs();
                    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
                    return;
                }
                i++;
            }
            char error_msg[50];
            sprintf(error_msg, "ERROR: no PID %d\n", pid);
            write(STDERR_FILENO, error_msg, strlen(error_msg));
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);
            return;
        }
    } 
}

void cmd_quit(const char **toks) {
    if (toks[1] != NULL) {
       
        const char* error_message = "ERROR: quit takes no arguments\n";
        write(STDERR_FILENO, error_message, strlen(error_message));

    } else {
        exit(0);
    }
}

void eval(const char **toks, bool bg) { // bg is true iff command ended with &
    assert(toks);
    if (*toks == NULL) return;
    if (strcmp(toks[0], "quit") == 0) {
        cmd_quit(toks);
    }//TODO: other commands
    else if (strcmp(toks[0], "jobs") == 0) {
        cmd_jobs(toks);    // check if right number of arguments
    }
    else if (strcmp(toks[0], "nuke") == 0) {
        cmd_nuke(toks);   // check if right number of arguments
    }
    else if (strcmp(toks[0], "fg") == 0) {
        cmd_fg(toks);   // check if right number of arguments
    }
    else if (strcmp(toks[0], "bg") == 0) {
        cmd_bg(toks);   // check if right number of arguments
    }
    else if (toks[0] == NULL) { // if Ctrl + D is pressed
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid == 0) {    // if there is a foreground process
            waitpid(-1, &status, 0);
        }               // goes to it????
        else {
            exit(0);        // if there isnt, exits
        }
    }  
    else {
        spawn(toks, bg);
    }
}

// you don't need to touch this unless you want to add debugging
void parse_and_eval(char *s) {
    assert(s);
    const char *toks[MAXLINE + 1];
    
    while (*s != '\0') {
        bool end = false;
        bool bg = false;
        int t = 0;

        while (*s != '\0' && !end) {
            while (*s == '\n' || *s == '\t' || *s == ' ') ++s;
            if (*s != ';' && *s != '&' && *s != '\0') toks[t++] = s;
            while (strchr("&;\n\t ", *s) == NULL) ++s;
            switch (*s) {
            case '&':
                bg = true;
                end = true;
                break;
            case ';':
                end = true;
                break;
            }
            if (*s) *s++ = '\0';
        }
        toks[t] = NULL;
        eval(toks, bg);
    }
}

// you don't need to touch this unless you want to add debugging
void prompt() {
    printf("crash> ");
    fflush(stdout);
}

// you don't need to touch this unless you want to add debugging
int repl() {
    char *buf = NULL;
    size_t len = 0;
    while (prompt(), getline(&buf, &len, stdin) != -1) {
        parse_and_eval(buf);
    }

    if (buf != NULL) free(buf);
    if (ferror(stdin)) {
        perror("ERROR");
        return 1;
    }
    return 0;
}

// you don't need to touch this unless you want to add debugging options
int main(int argc, char **argv) {
    install_signal_handlers();
    return repl();
}
