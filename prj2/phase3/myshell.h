#include <unistd.h>
#include <stdio.h>

#define MAXJOBS 128

typedef struct {
    int job_id, state;    // state 0(stop) 1(run)
    pid_t pid;  // 양수
    char cmd[128];
} job;

void init_job();
void add_job(pid_t pid, int state, char *cmdline);
int Jobs();
void directory_error(char *path);
int Cd(char* path);
void Execvp(const char *filename, char *const argv[]);
int parseline_by_pipe(char* buf, char ** argv);
char* delete_space(char* s);
// void run_pipe(char** cmds, int i, int cnt, int bg);
void run_pipe(char **cmds, int cnt);

void sigchld_handler(int sig);
void sigint_handler(int sig);
void sigtstp_handler(int sig);