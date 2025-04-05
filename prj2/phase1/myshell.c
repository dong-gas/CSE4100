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

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) {
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if (argv[0] == NULL)
        return;                                    /* Ignore empty lines */
    if (!builtin_command(argv)) {                  // quit -> exit(0), & -> ignore, other -> run

        // buitlin_command가 아닌 경우
        pid = Fork();        
        
        if (pid == 0) {  // 자식
            Execvp(argv[0], argv);
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
    if (!strcmp(argv[0], "cd")) { /* cd 구현 */
        Cd(argv[1]);
        return 1;
    }
    if (!strcmp(argv[0], "&")) /* Ignore singleton & */
        return 1;

    return 0; /* Not a builtin c-	Phase2 (pipelining)
    	Pipeline( ‘|’ )을 구현한 부분에 대해서 간략히 설명 (design & implementation)
    	Pipeline 개수에 따라 어떻게 handling했는지에 대한 설명
    ommand */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) {
    char *delim; /* Points to first space delimiter */
    int argc;    /* Number of args */
    int bg;      /* Background job? */

    buf[strlen(buf) - 1] = ' ';   /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;

    if (argc == 0) /* Ignore blank line */
        return 1;

    /* Should the job run in the background? */
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

void Cd(char *path) {
    char *home = getenv("HOME");  // 환경변수 HOME 찾기

    if (path == NULL || strlen(path) == 0 || strcmp(path, "~") == 0) {
        // Home directory로 이동하는 경우들
        if (home == NULL || chdir(home) == -1) directory_error(path);  // 만약 HOME이 등록되어 있지 않거나 이동 실패한 경우 오류출력
    }
    else if (path[0] == '~') {
        if (home == NULL) {  // 만약 HOME이 등록되어 있지 않으면 오류
            directory_error(path);
            return;
        }

        char target_path[MAXLINE];
        snprintf(target_path, sizeof(target_path), "%s%s", home, path + 1);

        if (chdir(target_path) == -1) directory_error(path);  // 경로 없는 경우
    }
    else {                                             // 일반 경로
        if (chdir(path) == -1) directory_error(path);  // 경로 없는 경우
    }
}

void Execvp(const char *filename, char *const argv[]) {
    // 경로가 주어지지 않았을 때도 사용 할 수 있는 execvp를 사용
    if (execvp(filename, argv) < 0)
        unix_error("Execvp error");
}