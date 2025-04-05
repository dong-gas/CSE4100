/* $begin shellmain */

#include "myshell.h" /* 추가 */

#include <errno.h>

#include "csapp.h"
#define MAXARGS 128

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);

int main() {
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
/* eval - Evaluate a command line */
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


    // 명령어 개수만큼 fork
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

    // 부모) 사용 다한 파이프 fd 닫기
    for (int k = 0; k < 2 * (cmd_count - 1); k++) Close(pipefds[k]);

    if (!bg) {
        int status;
        for (int i = 0; i < cmd_count; i++) Waitpid(pids[i], &status, 0);
    }
    else {
        // 백그라운드면 기다리지 않음
        printf("[%d] %s", pids[0], cmdline);
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

    if ((bg = (*argv[argc - 1] == '&')) != 0)
        argv[--argc] = NULL;
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