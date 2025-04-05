/* $begin shellmain */
#include "myshell.h" /* 추가 */

#include <errno.h>

#include "csapp.h"

static job_t jobs[MAXJOBS];
static int njid = 1;  // job id 번호로 쓸 거

int main() {
    pid_t shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(STDIN_FILENO, shell_pgid);

    Signal(SIGTTOU, SIG_IGN);
    Signal(SIGTTIN, SIG_IGN);

    // SIG 등록
    Signal(SIGCHLD, sigchld_handler);
    Signal(SIGINT, sigint_handler);
    Signal(SIGTSTP, sigtstp_handler);

    for (int i = 0; i < MAXJOBS; i++) {  // jobs 초기화
        jobs[i].pgid = jobs[i].jid = 0;
        jobs[i].state = UNDEF;
        jobs[i].cmdline[0] = '\0';
    }

    char cmdline[MAXLINE]; /* Command line */

    while (1) {
        /* Read */
        printf("CSE4100-SP-P2> ");
        fgets(cmdline, MAXLINE, stdin);
        if (feof(stdin))
            exit(0);

        /* Evaluate */
        eval(cmdline);
    }
}
/* $end shellmain */

/* $begin eval */
void eval(char *cmdline) {
    char buf1[MAXLINE], buf2[MAXLINE]; /* pipe용으로 하나 더.. */
    char *argv[MAXARGS];               // parse..
    char *cmds[MAXARGS];               // 파이프 기준..

    int bg; /* Should the job run in bg or fg? */

    strcpy(buf1, cmdline);
    bg = parseline(buf1, argv);

    if (argv[0] == NULL) return;
    if (builtin_command(argv)) return;  // cd, quit, exit.. 인 경우

    strcpy(buf2, cmdline);
    int cmd_count = parseline_by_pipe(buf2, cmds);  // 파이프 개수
    if (cmd_count == 0) return;

    int pipefds[2 * (MAXARGS + 1)];
    // pipe 미리 만들기..
    for (int i = 0; i < cmd_count - 1; i++) 
        if (pipe(pipefds + 2 * i) < 0) unix_error("pipe error");

    // 자식 pid들을 모아두는 배열
    pid_t pids[MAXARGS];


    for (int i = 0; i < cmd_count; i++) {
        char tmp_buf[MAXLINE];
        strcpy(tmp_buf, cmds[i]); // pipe 기준인 거 복사해서 파싱..
        parseline(tmp_buf, argv);

        pids[i] = Fork();
        if (pids[i] == 0) { //자식
            setpgid(0, 0); // group id 
            if (i > 0) dup2(pipefds[2 * (i - 1)], STDIN_FILENO);
            if (i < cmd_count - 1) dup2(pipefds[2 * i + 1], STDOUT_FILENO);
            for (int j = 0; j < 2 * (cmd_count - 1); j++) Close(pipefds[j]); // 부모가 만든 거 닫
            Execvp(argv[0], argv);
            exit(0);
        }
    }

    // 부모: 사용 다한 파이프 fd 닫기
    for (int k = 0; k < 2 * (cmd_count - 1); k++) Close(pipefds[k]);

    setpgid(pids[0], pids[0]);
    if (!bg) {  // 포그라운드
        addjob(pids[0], pids[0], FG, cmdline);
        tcsetpgrp(STDIN_FILENO, pids[0]);  // 제어권 포기하기..

        int status;
        waitpid(-pids[0], &status, WUNTRACED);  // 그룹 전체 대기하기..

        tcsetpgrp(STDIN_FILENO, getpgrp());  // 제어권 다시 받기..

        if (WIFSTOPPED(status)) {  // ctrl z
            job_t *job = getjobbypid(pids[0]);
            if (job) {
                job->state = ST;
                printf("\n[%d]+  Stopped\t\t%s", job->jid, job->cmdline);
            }
        }
        else if (WIFSIGNALED(status)) {  // ctrl c
            job_t *fgjob = getjobbypid(pids[0]);
            printf("\n[%d] Terminated %s", fgjob->jid, fgjob->cmdline);
            deletejob(pids[0]);
        }
        else deletejob(pids[0]);
    }
    else {  // 백그라운드
        addjob(pids[cmd_count - 1], pids[0], BG, cmdline);
        printf("[%d] (%d) %s", getjobbypgid(pids[0])->jid, pids[0], cmdline);
    }

    return;
}
/* $end eval */

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) {
    if (!strcmp(argv[0], "quit")) /* quit command */
        exit(0);
    if (!strcmp(argv[0], "exit")) /* exit 구현 */
        exit(0);
    if (!strcmp(argv[0], "cd")) /* cd 구현 */
        return Cd(argv[1]);
    if (!strcmp(argv[0], "&")) /* Ignore singleton & */
        return 1;

    if (!strcmp(argv[0], "jobs")) /* jobs 구현 */
        return Jobs();

    if (!strcmp(argv[0], "fg")) {
        int jid = atoi(&argv[1][1]);  // argv[1]: %124\0 니까 argv[1][1]부터 넘기면 그 뒤 정수 받을 수 있음...
        job_t *job = getjobjid(jid);
        if (!job) {
            printf("No such job\n");
            return 1;
        }

        job->state = FG;
        tcsetpgrp(STDIN_FILENO, job->pgid);
        Kill(-job->pgid, SIGCONT);

        int status;
        waitpid(-job->pgid, &status, WUNTRACED);
        tcsetpgrp(STDIN_FILENO, getpgrp());

        if (WIFSTOPPED(status)) {
            job->state = ST;
            printf("\n[%d]+  Stopped\t\t%s", job->jid, job->cmdline);
        }
        else if (WIFSIGNALED(status)) {
            printf("\n[%d] Terminated %s", job->jid, job->cmdline);
            deletejob(job->pgid);
        }
        else if (WIFEXITED(status)) deletejob(job->pgid);

        return 1;
    }

    if (!strcmp(argv[0], "bg")) {
        int jid = atoi(&argv[1][1]);
        job_t *job = getjobjid(jid);
        if (!job) {
            printf("No such job\n");
            return 1;
        }
        job->state = BG;
        Kill(-job->pgid, SIGCONT);
        printf("[%d] (%d) %s", job->jid, job->pgid, job->cmdline);
        return 1;
    }

    if (!strcmp(argv[0], "kill")) {
        int jid = atoi(&argv[1][1]);
        job_t *job = getjobjid(jid);
        if (!job) {
            printf("No such job\n");
            return 1;
        }

        Kill(-job->pgid, SIGKILL);
        sleep(1);
        return 1;
    }
    return 0; /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) {
    int argc = 0, bg = 0;
    if (buf[strlen(buf) - 1] == '\n') buf[strlen(buf) - 1] = '\0';

    char *ptr = buf;
    while (*ptr) {
        while (isspace(*ptr)) ptr++;  // 앞 빈칸 생략
        if (*ptr == '\0') break;

        if (*ptr == '"' || *ptr == '\'') {
            char q = *ptr;
            argv[argc++] = ++ptr;
            while (*ptr && *ptr != q) ptr++;  // 닫는 괄호 전까지
            if (*ptr) *ptr++ = '\0';
        }
        else {
            argv[argc++] = ptr;
            while (*ptr && !isspace(*ptr)) ptr++;  // 공백 전까지
            if (*ptr) *ptr++ = '\0';
        }
    }

    // 하단은 기존 코드 그대로..

    argv[argc] = NULL;
    if (argc == 0) return 1;

    // bg 여부 판단하기.
    int len = strlen(argv[argc - 1]);
    if (!len) return 0;

    if (argv[argc - 1][len - 1] == '&') {
        bg = 1;
        if (len == 1) argv[--argc] = NULL;    // & 하나인 거
        else argv[argc - 1][len - 1] = '\0';  // 명령어에 붙어 있는 경우
    }

    return bg;
}
/* $end parseline */

void directory_error(char *path) {
    // 디렉토리가 없을 때 오류 출력
    printf("bash: cd: %s: No such file or directory\n", path);
    return;
}

// cd 명령어
int Cd(char *path) {
    char *home = getenv("HOME");  // 환경변수 HOME 찾기

    if (path == NULL || strlen(path) == 0 || strcmp(path, "~") == 0) {  // Home directory로 이동하는 경우들
        if (home == NULL || chdir(home) == -1) directory_error(path);   // 만약 HOME이 등록되어 있지 않거나 이동 실패한 경우 오류출력
    }
    else if (path[0] == '~') {
        if (home == NULL) {  // 만약 HOME이 등록되어 있지 않으면 오류
            directory_error(path);
            return 1;
        }
        char target_path[MAXLINE];
        snprintf(target_path, sizeof(target_path), "%s%s", home, path + 1);  // ~빼고 복사
        if (chdir(target_path) == -1) directory_error(path);                 // 경로 없는 경우
    }
    else {                                             // 일반 경로
        if (chdir(path) == -1) directory_error(path);  // 경로 없는 경우
    }
    return 1;
}

// jobs 명령어
int Jobs() {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].state == UNDEF || jobs[i].state == FG) continue;
        printf("[%d] %d %s %s", jobs[i].jid, jobs[i].pgid, (jobs[i].state == BG) ? "Running" : "Stopped", jobs[i].cmdline);
    }
    return 1;
}

void Execvp(const char *filename, char *const argv[]) {
    // 경로가 주어지지 않았을 때도 사용 할 수 있는 execvp를 사용
    if (execvp(filename, argv) < 0)
        unix_error("Execvp error");
}

char *delete_space(char *s) {  // 앞 뒤 공백 제거하는 함수..

    while (isspace(*s)) s++;  // 앞 공백 제거

    // 뒤 공백 제거
    char *r = s + strlen(s) - 1;
    while (s < r && isspace(*r)) r--;
    *(r + 1) = '\0';

    return s;
}

int parseline_by_pipe(char *buf, char **cmds) {  // 파이프 단위로 짜르고
    int cnt = 0;
    char *cmd = strtok(buf, "|");
    while (cmd != NULL && cnt < MAXARGS) {
        cmds[cnt++] = delete_space(cmd);
        cmd = strtok(NULL, "|");
    }
    cmds[cnt] = NULL;
    return cnt;
}

int addjob(pid_t pid, pid_t pgid, job_state state, const char *cmdline) {  // job 추가하기 (인덱스 리턴..)
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].state == UNDEF) {
            jobs[i].pgid = pgid;
            jobs[i].pid = pid;
            jobs[i].jid = njid++;
            jobs[i].state = state;
            strncpy(jobs[i].cmdline, cmdline, MAXCMDLINE - 1);
            jobs[i].cmdline[MAXCMDLINE - 1] = '\0';
            return jobs[i].jid;
        }
    }
    return -1;
}

int deletejob(pid_t pgid) {  // pgid로 삭제..
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pgid == pgid && jobs[i].state != UNDEF) {
            jobs[i].pgid = jobs[i].jid = 0;
            jobs[i].state = UNDEF;
            jobs[i].cmdline[0] = '\0';
            return 1;
        }
    }
    return 0;
}

// pgid, pid, jid로 찾기
job_t *getjobbypgid(pid_t pgid) {
    for (int i = 0; i < MAXJOBS; i++)
        if (jobs[i].pgid == pgid && jobs[i].state != UNDEF) return &jobs[i];
    return NULL;
}

job_t *getjobbypid(pid_t pid) {
    for (int i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid && jobs[i].state != UNDEF) return &jobs[i];
    return NULL;
}

job_t *getjobjid(int jid) {
    for (int i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid == jid && jobs[i].state != UNDEF) return &jobs[i];
    return NULL;
}

void sigchld_handler(int sig) {
    int bef = errno;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        job_t *job = getjobbypid(pid);
        if (!job) continue;

        if (WIFEXITED(status)) {                         // 정상
            if (job->state != FG) deletejob(job->pgid);  // fg아니면 삭제
        }
        else if (WIFSIGNALED(status)) {
            if (job->state == BG) printf("[%d] Terminated %s", job->jid, job->cmdline);
            if (job->state != FG) deletejob(job->pgid);  // Fg 아니면 삭제
        }
        else if (WIFSTOPPED(status)) {  // ctrl z
            job->state = ST;
            printf("\n[%d] Stopped %s\n", job->jid, job->cmdline);
        }
        else if (WIFCONTINUED(status)) job->state = BG;
    }

    errno = bef;
}

pid_t fgpid() {  // foreground job pgid 반환..
    for (int i = 0; i < MAXJOBS; i++)
        if (jobs[i].state == FG) return jobs[i].pgid;
    return -1;
}

void sigint_handler(int sig) {  // ctrl c
    pid_t fg = fgpid();
    if (fg > 0) Kill(-fg, SIGINT);
}

void sigtstp_handler(int sig) {  // ctrl z
    pid_t fg = fgpid();
    if (fg > 0) Kill(-fg, SIGTSTP);
}