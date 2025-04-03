/* $begin shellmain */


#include <errno.h>

#include "csapp.h"
#include "myshell.h" /* 추가 */
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
void run_pipe(char** cmds, int i, int cnt) {
    // fprintf(stderr, "[debug] running command: '%s'\n", cmds[i]);
    if (i + 1 == cnt) {
        // 마지막 명령어. 재귀 종료
        char* argv[MAXARGS];
        parseline(cmds[i], argv);
        Execvp(argv[0], argv);
        return;
    }
    int fd[2]; // pipe file descriptor
    pipe(fd);
    pid_t pid = Fork();
    

    if (pid == 0) { // 자식
        Dup2(fd[1], STDOUT_FILENO);  // 파이프로 넘기기 
        Close(fd[0]), Close(fd[1]);  // 닫기

        char* argv[MAXARGS];
        parseline(cmds[i], argv);
        Execvp(argv[0], argv);
    }
    else if (pid > 0) { // 부모
        Dup2(fd[0], STDIN_FILENO);  // 파이프로 넘기기
        Close(fd[0]), Close(fd[1]); // 닫기
        run_pipe(cmds, i + 1, cnt);
        int status;
        Waitpid(pid, &status, 0);

    }
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

    char* cmds[MAXARGS]; // pipe기준 파싱 저장용
    int cnt = parseline_by_pipe(buf2, cmds);

    if (argv[0] == NULL)
        return;                                    /* Ignore empty lines */
    if (!builtin_command(argv)) {                  // quit -> exit(0), & -> ignore, other -> run

        // buitlin_command(즉, exit, quit, cd)가 아닌 경우                    
        pid = Fork();            
        if (pid == 0) {      // 자식
            if(cnt > 1) run_pipe(cmds, 0, cnt); // 파이프가 있으면 재귀적으로 실행       
            else {
                // for (int k = 0; argv[k] != NULL; k++) fprintf(stderr, "[debug argv[%d]] = %s\n", k, argv[k]); // 디버깅용 출력
                Execvp(argv[0], argv);         // 단일 명령이라면 그냥 실행
            }
        }
        else if (pid > 0) {  // 부모
            int status;
            Waitpid(pid, &status, 0);
        }
        

        /* Parent waits for foreground job to terminate */
        if (!bg) {
            int status;
        }
        else  // when there is backgrount process!
            printf("%d %s", pid, cmdline);
    }
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) {
    if (!strcmp(argv[0], "quit")) /* quit command */
        exit(0);
    if (!strcmp(argv[0], "exit")) /* exit 구현 */
        exit(0);
    if (!strcmp(argv[0], "cd"))   /* cd 구현 */
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

    char* ptr = buf;
    while (*ptr) {
        while (isspace(*ptr)) ptr++; // 앞 빈칸 생략
        if (*ptr == '\0') break;

        if (*ptr == '"' || *ptr == '\'') {
            char q = *ptr;
            argv[argc++] = ++ptr;
            while (*ptr && *ptr != q) ptr++; // 닫는 괄호 전까지
            if (*ptr) *ptr++ = '\0';
        }
        else {
            argv[argc++] = ptr;
            while (*ptr && !isspace(*ptr)) ptr++; // 공백 전까지
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

char* delete_space(char* s) { // 앞 뒤 공백 제거
    while(isspace(*s)) s++; // 앞 공백 제거

    // 뒤 공백 제거
    char* r = s + strlen(s) -1;
    while(s < r && isspace(*r)) r--;
    *(r + 1) = '\0';

    return s;
}

int parseline_by_pipe(char* buf, char ** cmds) {
    int cnt = 0;
    char* cmd = strtok(buf, "|");
    while(cmd != NULL && cnt < MAXARGS) {
        cmds[cnt++] = delete_space(cmd);
        cmd = strtok(NULL, "|");
    }
    cmds[cnt] = NULL;
    return cnt;
}