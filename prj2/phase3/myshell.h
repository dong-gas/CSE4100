#include <stdio.h>
#include <unistd.h>

#define MAXARGS 128
#define MAXJOBS 128
#define MAXCMDLINE 1024

/* job_state를 나타낼 enum 또는 매크로 */
typedef enum {
    UNDEF = 0,
    FG,
    BG,
    ST
} job_state;

/* 하나의 Job 정보를 담는 구조체 */
typedef struct {
    pid_t pid;
    pid_t pgid;              /* 프로세스 그룹 ID */
    int jid;                 /* Job ID (1부터 할당) */
    job_state state;         /* FG/BG/ST 등 */
    char cmdline[MAXCMDLINE];/* 전체 명령어 문자열 */
} job_t;

void directory_error(char* path);
int Cd(char* path);
void Execvp(const char* filename, char* const argv[]);
int parseline_by_pipe(char* buf, char** argv);
char* delete_space(char* s);
void run_pipe(char** cmds, int i, int cnt);

/* 초기화: 모든 job 슬롯 비움 */
void initjobs();

/* 빈 슬롯 찾기 */
static int find_empty_slot();

/* 새로운 Job 추가 */
int addjob(pid_t pid, pid_t pgid, job_state state, const char* cmdline);

/* 해당 pgid를 가진 job 삭제 */
int deletejob(pid_t pgid);

/* pgid로 job을 찾아서 반환 (없으면 NULL) */
job_t* getjobbypgid(pid_t pgid);
/* jid로 job을 찾아서 반환 (없으면 NULL) */
job_t* getjobjid(int jid);

job_t *getjobbypid(pid_t pid);

/* 현재 포그라운드(FG) job의 pgid 리턴 (없으면 0) */
pid_t fgpid();
/* 디버그용: job 리스트 출력 */
void listjobs();

void sigchld_handler(int sig);

void sigint_handler(int sig);

void sigtstp_handler(int sig);