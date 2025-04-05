/* $begin shellmain */

#include "myshell.h" /* 추가 */

#include <errno.h>

#include "csapp.h"

static job_t jobs[MAXJOBS];
static int njid = 1;

/* 초기화: 모든 job 슬롯 비움 */
void initjobs() {
    for (int i = 0; i < MAXJOBS; i++) {
        jobs[i].pgid = 0;
        jobs[i].jid = 0;
        jobs[i].state = UNDEF;
        jobs[i].cmdline[0] = '\0';
    }
    njid = 1;
}

/* 빈 슬롯 찾기 */
static int find_empty_slot() {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].state == UNDEF)
            return i;
    }
    return -1;  // 꽉 참
}

/* 새로운 Job 추가 */
int addjob(pid_t pid, pid_t pgid, job_state state, const char *cmdline) {
    int idx = find_empty_slot();
    if (idx < 0) {
        fprintf(stderr, "Too many jobs!\n");
        return 0;
    }
    jobs[idx].pgid = pgid;
    jobs[idx].pid = pid;
    jobs[idx].jid = njid++;
    jobs[idx].state = state;
    strncpy(jobs[idx].cmdline, cmdline, MAXCMDLINE - 1);
    jobs[idx].cmdline[MAXCMDLINE - 1] = '\0';

    if (njid > 999999)
        njid = 1;

    return jobs[idx].jid;  // 할당된 jid 반환
}

/* 해당 pgid를 가진 job 삭제 */
int deletejob(pid_t pgid) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pgid == pgid && jobs[i].state != UNDEF) {
            jobs[i].pgid = 0;
            jobs[i].jid = 0;
            jobs[i].state = UNDEF;
            jobs[i].cmdline[0] = '\0';
            return 1;
        }
    }
    return 0;
}

/* pgid로 job을 찾아서 반환 (없으면 NULL) */
job_t *getjobbypgid(pid_t pgid) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pgid == pgid && jobs[i].state != UNDEF) {
            return &jobs[i];
        }
    }
    return NULL;
}

job_t *getjobbypid(pid_t pid) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid && jobs[i].state != UNDEF)
            return &jobs[i];
    }
    return NULL;
}

/* jid로 job을 찾아서 반환 (없으면 NULL) */
job_t *getjobjid(int jid) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].jid == jid && jobs[i].state != UNDEF) {
            return &jobs[i];
        }
    }
    return NULL;
}

/* 현재 포그라운드(FG) job의 pgid 리턴 (없으면 0) */
pid_t fgpid() {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].state == FG)
            return jobs[i].pgid;
    }
    return 0;
}

/* 디버그용: job 리스트 출력 */
void listjobs() {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].state == FG) continue;
        if (jobs[i].state != UNDEF) {
            printf("[%d] %d ", jobs[i].jid, jobs[i].pgid);
            switch (jobs[i].state) {
                case BG:
                    printf("Running ");
                    break;
                case FG:
                    printf("Foreground ");
                    break;
                case ST:
                    printf("Stopped ");
                    break;
                default:
                    printf("??? ");
                    break;
            }
            printf("%s", jobs[i].cmdline);
        }
    }
}

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);

int main() {
    pid_t shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(STDIN_FILENO, shell_pgid);

    // 시그널 무시 설정 (중요!!)
    Signal(SIGTTOU, SIG_IGN);
    Signal(SIGTTIN, SIG_IGN);
    // Signal(SIGTSTP, SIG_IGN);

    Signal(SIGCHLD, sigchld_handler);
    Signal(SIGINT, sigint_handler);
    Signal(SIGTSTP, sigtstp_handler);

    initjobs();  // 초기화

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
    char buf1[MAXLINE];  /* for parsing & (background) */
    char buf2[MAXLINE];  /* for parsing pipe (|) */
    char *argv[MAXARGS]; /* 임시 인자 리스트 */
    char *cmds[MAXARGS]; /* 파이프로 분할된 명령들 */
    int bg;              /* Should the job run in bg or fg? */

    /* 1) 백그라운드(&) 처리 위해 한번 파싱 */
    strcpy(buf1, cmdline);
    bg = parseline(buf1, argv);
    // 이때 buf1 내부에서 공백/따옴표 처리 & 백그라운드인지 여부만 가져옴
    // argv는 builtin_command(예: cd, exit 등) 체크용으로 잠깐 사용

    /* 2) builtin 확인 */
    if (argv[0] == NULL) return;
    if (builtin_command(argv)) return;  // cd, quit, exit.. 인 경우

    /* 3) 파이프(|) 구분을 위해 별도 buf2를 이용 */
    strcpy(buf2, cmdline);
    int cmd_count = parseline_by_pipe(buf2, cmds);
    if (cmd_count == 0)
        return;

    /*
     * 4) 파이프 처리:
     *    부모가 (cmd_count - 1)개의 파이프를 먼저 만들고,
     *    cmd_count번 반복하며 자식 프로세스를 만든 뒤,
     *    각 자식이 필요한 stdin/stdout을 dup2로 연결.
     */

    // 파이프 개수 = cmd_count - 1
    // 예) cmd_count=3 (A, B, C) → 파이프 2개 필요
    int pipefds[2 * (MAXARGS + 1)];
    // int pipefds[2 * (cmd_count - 1)];
    for (int i = 0; i < cmd_count - 1; i++) {
        if (pipe(pipefds + 2 * i) < 0) {
            unix_error("pipe error");
        }
    }

    // 자식 pid들을 모아두는 배열
    pid_t pids[MAXARGS];

    // 명령어 개수만큼 fork
    for (int i = 0; i < cmd_count; i++) {
        // (1) 각 cmds[i]를 공백 기준으로 argv 파싱
        char tmp_buf[MAXLINE];
        strcpy(tmp_buf, cmds[i]);
        parseline(tmp_buf, argv);
        // 예) cmds[i]가 "ls -l"이면, argv[0]="ls", argv[1]="-l", argv[2]=NULL

        pids[i] = Fork();
        if (pids[i] == 0) {
            // 자식 프로세스 쪽
            setpgid(0, 0);
            // (a) 이전 파이프가 있다면 => 읽기(파이프의 fd[0]을 STDIN)
            if (i > 0) {
                dup2(pipefds[2 * (i - 1)], STDIN_FILENO);
            }

            // (b) 다음 파이프가 있다면 => 쓰기(파이프의 fd[1]을 STDOUT)
            if (i < cmd_count - 1) {
                dup2(pipefds[2 * i + 1], STDOUT_FILENO);
            }

            // (c) 부모가 만든 모든 pipe fd 닫기
            for (int k = 0; k < 2 * (cmd_count - 1); k++) {
                Close(pipefds[k]);
            }

            // (d) 명령 실행
            Execvp(argv[0], argv);
            // execvp 실패하면 아래로 떨어지지만, 보통은 프로세스 교체됨
            exit(0);  // 안전빵
        }
        // 부모는 그냥 다음 i로 이동
    }

    // 부모: 사용 다한 파이프 fd 닫기
    for (int k = 0; k < 2 * (cmd_count - 1); k++) {
        Close(pipefds[k]);
    }

    setpgid(pids[0], pids[0]);  // 첫 자식 기준으로 프로세스 그룹 설정 (혹시 자식이 먼저 못했을 경우)
    if (!bg) {
        // 포그라운드 실행
        addjob(pids[0], pids[0], FG, cmdline);
        tcsetpgrp(STDIN_FILENO, pids[0]);  // 제어권 넘김

        int status;
        waitpid(-pids[0], &status, WUNTRACED);  // 그룹 전체 대기

        tcsetpgrp(STDIN_FILENO, getpgrp());  // 제어권 복구

        if (WIFSTOPPED(status)) {
            job_t *job = getjobbypid(pids[0]);
            if (job) {
                job->state = ST;
                printf("\n[%d]+  Stopped\t\t%s", job->jid, job->cmdline);
            }
        }
        else if (WIFSIGNALED(status)) {
            printf("\n");  // Ctrl+C로 죽었을 때 줄바꿈
            deletejob(pids[0]);
        }
        else {
            deletejob(pids[0]);
        }

    }
    else {
        // 백그라운드 실행
        // addjob(pids[0], pids[0], BG, cmdline);
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

    if (!strcmp(argv[0], "jobs")) {
        listjobs();
        return 1;
    }

    if (!strcmp(argv[0], "fg")) {
        if (argv[1] == NULL || argv[1][0] != '%') {
            printf("Usage: fg %%<jid>\n");
            return 1;
        }
        int jid = atoi(&argv[1][1]);
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
            printf("\n"); 
            deletejob(job->pgid);
        }
        else if (WIFEXITED(status)) {
            deletejob(job->pgid);
        }

        return 1;
    }

    if (!strcmp(argv[0], "bg")) {
        if (argv[1] == NULL || argv[1][0] != '%') {
            printf("Usage: bg %%<jid>\n");
            return 1;
        }
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
        if (argv[1] == NULL || argv[1][0] != '%') {
            printf("Usage: kill %%<jid>\n");
            return 1;
        }
        int jid = atoi(&argv[1][1]);
        job_t *job = getjobjid(jid);
        if (!job) {
            printf("No such job\n");
            return 1;
        }

        Kill(-job->pgid, SIGKILL);
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

    char *last = argv[argc - 1];
    int len = strlen(last);

    if (len > 0 && last[len - 1] == '&') {
        bg = 1;

        if (len == 1) {
            // 인자가 그냥 "&"인 경우 (예: "ls &")
            argv[--argc] = NULL;
        }
        else {
            // 인자 끝에 붙어있는 &만 제거 (예: "ls&" → "ls")
            last[len - 1] = '\0';
        }
    }

    return bg;
}
/* $end parseline */

void directory_error(char *path) {
    // 디렉토리가 없을 때 오류 출력
    printf("bash: cd: %s: No such file or directory\n", path);
    return;
}

int Cd(char *path) {
    char *home = getenv("HOME");  // 환경변수 HOME 찾기

    if (path == NULL || strlen(path) == 0 || strcmp(path, "~") == 0) {
        // Home directory로 이동하는 경우들
        if (home == NULL || chdir(home) == -1) directory_error(path);  // 만약 HOME이 등록되어 있지 않거나 이동 실패한 경우 오류출력
    }
    else if (path[0] == '~') {
        if (home == NULL) {  // 만약 HOME이 등록되어 있지 않으면 오류
            directory_error(path);
            return 1;
        }

        char target_path[MAXLINE];
        snprintf(target_path, sizeof(target_path), "%s%s", home, path + 1);

        if (chdir(target_path) == -1) directory_error(path);  // 경로 없는 경우
    }
    else {                                             // 일반 경로
        if (chdir(path) == -1) directory_error(path);  // 경로 없는 경우
    }
    return 1;
}

void Execvp(const char *filename, char *const argv[]) {
    // 경로가 주어지지 않았을 때도 사용 할 수 있는 execvp를 사용
    if (execvp(filename, argv) < 0)
        unix_error("Execvp error");
}

char *delete_space(char *s) {  // 앞 뒤 공백 제거
    while (isspace(*s)) s++;   // 앞 공백 제거

    // 뒤 공백 제거
    char *r = s + strlen(s) - 1;
    while (s < r && isspace(*r)) r--;
    *(r + 1) = '\0';

    return s;
}

int parseline_by_pipe(char *buf, char **cmds) {
    int cnt = 0;
    char *cmd = strtok(buf, "|");
    while (cmd != NULL && cnt < MAXARGS) {
        cmds[cnt++] = delete_space(cmd);
        cmd = strtok(NULL, "|");
    }
    cmds[cnt] = NULL;
    return cnt;
}

void sigchld_handler(int sig) {
    int old_errno = errno;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        job_t *job = getjobbypid(pid);
        if (!job) continue;

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            if (job->state != FG) deletejob(job->pgid);
        }
        else if (WIFSTOPPED(status)) {
            job->state = ST;
            printf("\n[%d] Stopped %s\n", job->jid, job->cmdline);
        }
        else if (WIFCONTINUED(status)) {
            job->state = BG;
        }
    }

    errno = old_errno;
}

void sigint_handler(int sig) {
    pid_t fg = fgpid();
    if (fg > 0) {
        Kill(-fg, SIGINT);  // 포그라운드 그룹 전체에 SIGINT 전달
    }

}

void sigtstp_handler(int sig) {
    pid_t fg = fgpid();
    if (fg > 0) {
        Kill(-fg, SIGTSTP);  // 포그라운드 그룹 전체에 SIGTSTP 전달
    }
    printf("\n");
    fflush(stdout);
}