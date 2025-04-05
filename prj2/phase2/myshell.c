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

/* $begin run pipe */
/* 재귀적으로 pipe를 실행 */
void run_pipe(char **cmds, int i, int cnt) {
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
    else if (pid > 0) {              // 부모
        Dup2(fd[0], STDIN_FILENO);   // 파이프로 넘기기
        Close(fd[0]), Close(fd[1]);  // 닫기
        run_pipe(cmds, i + 1, cnt);
        int status;
        Waitpid(pid, &status, 0);
    }
}
/* $end run pipe */

/* $begin eval */
/* eval - Evaluate a command line */
// void eval(char *cmdline) {
//     char *argv[MAXARGS]; /* Argument list execve() */
//     char buf1[MAXLINE];  /* Holds modified command line */
//     char buf2[MAXLINE];  /* 예비 버퍼 1개 더 */
//     int bg;              /* Should the job run in bg or fg? */
//     pid_t pid;           /* Process id */

//     /* buf1: 기존 파싱 저장  */
//     /* buf2: 파이프 파싱 저장 */
//     strcpy(buf1, cmdline), strcpy(buf2, cmdline);
//     bg = parseline(buf1, argv);

//     char* cmds[MAXARGS]; // pipe기준 파싱 저장용
//     int cnt = parseline_by_pipe(buf2, cmds);

//     if (argv[0] == NULL)
//         return;                                    /* Ignore empty lines */
//     if (!builtin_command(argv)) {                  // quit -> exit(0), & -> ignore, other -> run
//         // buitlin_command(즉, exit, quit, cd)가 아닌 경우

//         pid = Fork();
//         if (pid == 0) run_pipe(cmds, 0, cnt); // 자식
//         else if (pid > 0) {  // 부모
//             int status;
//             Waitpid(pid, &status, 0);
//         }

//         /* Parent waits for foreground job to terminate */
//         if (!bg) {
//             int status;
//         }
//         else  // when there is backgrount process!
//             printf("%d %s", pid, cmdline);
//     }
//     return;
// }

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

    // (5) 백그라운드가 아니면 부모가 자식들 모두 기다림
    if (!bg) {
        int status;
        for (int i = 0; i < cmd_count; i++) {
            Waitpid(pids[i], &status, 0);
        }
    }
    else {
        // 백그라운드면 기다리지 않음
        // 보통은 "job id"나 첫 번째 pid 등을 출력
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