#include <stdio.h>
#include <unistd.h>

#define MAXARGS 128
#define MAXJOBS 128
#define MAXCMDLINE 1024

typedef enum {
    UNDEF = 0,
    FG,
    BG,
    ST
} job_state;

/* 하나의 Job 정보를 담는 구조체 */
typedef struct {
    pid_t pid;  // pid..
    pid_t pgid; // pgid (그룹)
    int jid; // job id (1 base)
    job_state state;
    char cmdline[MAXCMDLINE]; // 명령어
} job_t;

int addjob(pid_t pid, pid_t pgid, job_state state, const char* cmdline);
int deletejob(pid_t pgid);
job_t* getjobbypgid(pid_t pgid);
job_t* getjobjid(int jid);
job_t *getjobbypid(pid_t pid);
pid_t fgpid();
int Jobs();

void directory_error(char* path);
int Cd(char* path);
void Execvp(const char* filename, char* const argv[]);
int parseline_by_pipe(char* buf, char** argv);
char* delete_space(char* s);
void run_pipe(char** cmds, int i, int cnt);

void sigchld_handler(int sig);
void sigint_handler(int sig);
void sigtstp_handler(int sig);

void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);