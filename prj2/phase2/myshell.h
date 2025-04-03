#include <unistd.h>
#include <stdio.h>

void directory_error(char *path);
int Cd(char* path);
void Execvp(const char *filename, char *const argv[]);
int parseline_by_pipe(char* buf, char ** argv);
char* delete_space(char* s);
void run_pipe(char** cmds, int i, int cnt);