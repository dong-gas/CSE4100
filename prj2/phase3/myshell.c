/* $begin shellmain */

#include "myshell.h" /* 추가 */

#include <errno.h>

#include "csapp.h"
#define MAXARGS 128

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);

job job_list[MAXJOBS];
int jid = 0;

int main() {
    /* signal handler 등록 */
    Signal(SIGCHLD, sigchld_handler);
    Signal(SIGINT, sigint_handler);
    Signal(SIGTSTP, sigtstp_handler);

    char cmdline[MAXLINE]; /* Command line */
    init_job();            // init joblist

    while (1) {
        /* Read */
        printf("CSE4100-SP-P2> ");
        fgets(cmdline, MAXLINE, stdin);
        // cmdline[strcspn(cmdline, "\n")] = '\0';
        if (feof(stdin))
            exit(0);

        /* Evaluate */
        eval(cmdline);
    }
}
/* $end shellmain */

/* $begin run pipe */
/* 재귀적으로 pipe를 실행 */
void run_pipe(char **cmds, int i, int cnt, int bg) {
    // fprintf(stderr, "[debug] running command: '%s'\n", cmds[i]);
    if (i + 1 == cnt) {
        // 마지막 명령어. 재귀 종료
        char *argv[MAXARGS];
        parseline(cmds[i], argv);
        Execvp(argv[0], argv);
        return;
    }
    int fd[2];  // pipe file descriptor
    pipe(fd);
    pid_t pid = Fork();

    if (pid == 0) {                  // 자식
        Dup2(fd[1], STDOUT_FILENO);  // 파이프로 넘기기
        Close(fd[0]), Close(fd[1]);  // 닫기

        char *argv[MAXARGS];
        parseline(cmds[i], argv);
        Execvp(argv[0], argv);
    }

    Close(fd[1]);
    Dup2(fd[0], STDIN_FILENO);   // 파이프로 넘기기
    Close(fd[0]);  // 닫기
    run_pipe(cmds, i + 1, cnt, bg);
}
/* $end run pipe */

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) {
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf1[MAXLINE];  /* Holds modified command line */
    char buf2[MAXLINE];  /* 예비 버퍼 1개 더 */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */

    /* buf1: 기존 파싱 저장  */
    /* buf2: 파이프 파싱 저장 */
    strcpy(buf1, cmdline), strcpy(buf2, cmdline);
    bg = parseline(buf1, argv);

    char *cmds[MAXARGS];  // pipe기준 파싱 저장용
    int cnt = parseline_by_pipe(buf2, cmds);

    if (argv[0] == NULL)
        return;                    /* Ignore empty lines */
    if (!builtin_command(argv)) {  // quit -> exit(0), & -> ignore, other -> run
        // buitlin_command(즉, exit, quit, cd)가 아닌 경우

        pid = Fork();
        if (pid == 0) run_pipe(cmds, 0, cnt, bg);  // 자식
        else if (pid > 0) {                        // 부모
            if (!bg) {
                int status;
                Waitpid(pid, &status, 0);
            }
            else {
                // printf("[%d] %d\n", 1, pid);
                add_job(pid, 1, cmdline);
            }
        }

        // /* Parent waits for foreground job to terminate */
        // if (!bg) {
        //     int status;
        // }
        // else  // when there is backgrount process!
        //     printf("%d %s", pid, cmdline);
    }
    return;
}

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
    if (!strcmp(argv[0], "jobs"))
        return Jobs();
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

    int len = strlen(argv[argc - 1]);
    if (argv[argc - 1][len - 1] == '&') {  // 맨 끝이 &인 경우 분리 필요
        bg = 1;
        if (len == 1) argv[argc - 1] = NULL;  // &만 있는 경우 argc--;
        else argv[argc - 1][len - 1] = '\0';  // 앞에 붙어있던 경우
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

int Jobs() {
    for (int i = 0; i < MAXJOBS; i++) {
        if (!job_list[i].pid) continue;
        printf("[%d] %s %s", job_list[i].job_id, job_list[i].state ? "Running" : "Stopped", job_list[i].cmd);
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

void init_job() {
    memset(job_list, 0, sizeof(job_list));
}

void add_job(pid_t pid, int state, char *cmdline) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid) continue;
        job_list[i].pid = pid;
        job_list[i].job_id = ++jid;
        job_list[i].state = state;
        strcpy(job_list[i].cmd, cmdline);
        return;
    }
}

void sigchld_handler(int sig) {  // 자식 프로세스 종료 감지
    int bef = errno, status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < MAXJOBS; i++) {
            if (job_list[i].pid != pid) continue;
            job_list[i].pid = job_list[i].job_id = job_list[i].state = 0;
            job_list[i].cmd[0] = '\0';
            break;
        }
    }
    errno = bef;
    return;
}

void sigint_handler(int sig) {  // ctrl + c : 종료
    for (int i = 0; i < MAXJOBS; i++) {
        if (!job_list[i].pid || !job_list[i].state) continue;
        Kill(-job_list[i].pid, SIGINT);
        break;
    }
    return;
}

void sigtstp_handler(int sig) {  // ctrl + z : 중지
    for (int i = 0; i < MAXJOBS; i++) {
        if (!job_list[i].pid || !job_list[i].state) continue;
        Kill(-job_list[i].pid, SIGTSTP);
        job_list[i].state = 0;
        break;
    }
    return;
}