#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "list.h"

#define PERM        (0644)        /* default permission rw-r--r-- */
#define MAXBUF        (512)        /* max length of input line. */
#define MAX_ARG        (100)        /* max number of cmd line arguments. */

typedef enum {
    AMPERSAND,             /* & */
    NEWLINE,            /* end of line reached. */
    NORMAL,                /* file name or command option. */
    INPUT,                /* input redirection (< file) */
    OUTPUT,                /* output redirection (> file) */
    PIPE,                /* | for instance: ls *.c | wc -l */
    SEMICOLON            /* ; */
} token_type_t;

static char*    progname;        /* name of this shell program. */
static char    input_buf[MAXBUF];    /* input is placed here. */
static char    token_buf[2 * MAXBUF];    /* tokens are placed here. */
static char*    input_char;        /* next character to check. */
static char*    token;            /* a token such as /bin/ls */
static char*    prev_wd;        /* previous working directory */

static list_t*    path_dir_list;        /* list of directories in PATH. */
static int    input_fd;        /* for i/o redirection or pipe. */
static int    output_fd;        /* for i/o redirection or pipe */

/* fetch_line: read one line from user and put it in input_buf. */
int fetch_line(char* prompt)
{
    int    c;
    int    count;

    input_char = input_buf;
    token = token_buf;

    printf("%s", prompt);
    fflush(stdout);

    count = 0;

    for (;;) {

        c = getchar();

        if (c == EOF)
            return EOF;

        if (count < MAXBUF)
            input_buf[count++] = c;

        if (c == '\n' && count < MAXBUF) {
            input_buf[count] = 0;
            return count;
        }

        if (c == '\n') {
            printf("too long input line\n");
            return fetch_line(prompt);
        }

    }
}

/* end_of_token: true if character c is not part of previous token. */
static bool end_of_token(char c)
{
    switch (c) {
    case 0:
    case ' ':
    case '\t':
    case '\n':
    case ';':
    case '|':
    case '&':
    case '<':
    case '>':
        return true;

    default:
        return false;
    }
}

/* gettoken: read one token and let *outptr point to it. */
int gettoken(char** outptr)
{
    token_type_t    type;

    *outptr = token;

    while (*input_char == ' '|| *input_char == '\t')
        input_char++;

    *token++ = *input_char;

    switch (*input_char++) {
    case '\n':
        type = NEWLINE;
        break;

    case '<':
        type = INPUT;
        break;

    case '>':
        type = OUTPUT;
        break;

    case '&':
        type = AMPERSAND;
        break;

    case '|':
        type = PIPE;
        break;

    default:
        type = NORMAL;

        while (!end_of_token(*input_char))
            *token++ = *input_char++;
    }

    *token++ = 0; /* null-terminate the string. */


    return type;
}

/* error: print error message using formatting string similar to printf. */
void error(char *fmt, ...)
{
    va_list        ap;

    fprintf(stderr, "%s: error: ", progname);

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    /* print system error code if errno is not zero. */
    if (errno != 0) {
        fprintf(stderr, ": ");
        perror(0);
    } else
        fputc('\n', stderr);

}

/* run_program: fork and exec a program. */
void run_program(char** argv, int argc, bool foreground, bool doing_pipe)
{

    pid_t   child_pid;
    int     child_status;
    char    current_path[1024];

    unsigned len        = length(path_dir_list);
    list_t *current_dir = path_dir_list;

    /* Special built-in case when funciton is cd */

    if(strcmp(argv[0], "cd") == 0){

        char temp_wd[MAXBUF];

        if(prev_wd != NULL){
            strcpy(temp_wd, prev_wd);
        }

        prev_wd = getcwd(prev_wd, MAXBUF);

        if(argc != 1 && (strcmp(argv[1], "-") == 0)){
            if(prev_wd == NULL){
                error("No previous directory.\n");
                fflush(stdout);
                return;
            }
            chdir(temp_wd);

        }else{

            if(argc == 1){
                chdir("/Users/erikleffler1");
            }

            else if(chdir(argv[1]) == 0){
            }
            else{
                error("Argument is not a directory in path: %s\n", argv[1]);
                fflush(stdout);
            }
        }


        return;
    }

    /* Find path to function if accessible and store in current_path */

    for(int i = 0; i < len; i++){

        strcpy(current_path, current_dir->data);
        strcat(current_path, "/");
        strcat(current_path, argv[0]);

        if(access(current_path, X_OK) == 0){
            break;
        }

        else if(i == len - 1){
            error("Could not find %s in path.\n", argv[0]);
            return;
        }

        current_dir = current_dir->succ;
    }

    /* Fork and execute function */

    child_pid = fork();
    switch(child_pid) {

        case -1 :
            error("Failed fork, returned pid: %i", child_pid);
            break;

        case 0 :
            if(input_fd > 0){
                dup2(input_fd, 0);
                close(input_fd);
            }

            if(output_fd > 0){
                dup2(output_fd, 1);
            }

            execv(current_path, argv);
            break;

        default :
            if(foreground && !doing_pipe){
                const int es = WEXITSTATUS(child_status);
                if(waitpid(child_pid, &child_status, 0) == -1){
                    error("Failed fork, returned pid: %i", child_pid);
                    printf("waitpid failed");
                } else {
                    printf("Exit status %d\n", es);
                }
            }
    }
}

void parse_line(void)
{

    char*    argv[MAX_ARG + 1];
    int        argc;
    int        pipe_fd[2];    /* 1 for producer and 0 for consumer. */
    token_type_t    type;
    bool        foreground;
    bool        doing_pipe;

    input_fd    = 0;
    output_fd    = 0;
    argc        = 0;

    for (;;) {

        foreground    = true;
        doing_pipe    = false;

        type = gettoken(&argv[argc]);

        switch (type) {
        case NORMAL:
            argc += 1;
            break;

        case INPUT:
            type = gettoken(&argv[argc]);
            if (type != NORMAL) {
                error("expected file name: but found %s",
                    argv[argc]);
                return;
            }

            input_fd = open(argv[argc], O_RDONLY);

            if (input_fd < 0)
                error("cannot read from %s", argv[argc]);

            break;

        case OUTPUT:
            type = gettoken(&argv[argc]);
            if (type != NORMAL) {
                error("expected file name: but found %s",
                    argv[argc]);
                return;
            }

            output_fd = open(argv[argc], O_CREAT | O_WRONLY, PERM);

            if (output_fd < 0)
                error("cannot write to %s", argv[argc]);
            break;

        case PIPE:

            type = gettoken(&argv[argc]);
            if (type != NORMAL) {
                error("expected file name: but found %s",
                      argv[argc]);
                return;
            }

            doing_pipe = true;

            /* Pipe and set up file descriptors accordingly */

            if(pipe(pipe_fd) == -1){
                error("pipe failure.");
            }

            output_fd = pipe_fd[1];

            /* Split argument vector on the pipe-token */

            char* current_argv[MAX_ARG + 1];
            for(int i = 0; i < argc; i++){
                current_argv[i] = argv[i];
            }
            current_argv[argc] = NULL;

            argv[0] = argv[argc];
            for(int i = 1; i < argc; i++){
                argv[i] = NULL;
            }

            /* Run the program with first part of argv */

            run_program(current_argv, argc, foreground, doing_pipe);

            /* Manage file descriptors appropriatley */

            input_fd = pipe_fd[0];
            close(output_fd);
            output_fd = 0;

            argc = 1;
            break;


        case AMPERSAND:
            foreground = false;

        case NEWLINE:
        case SEMICOLON:
            if (argc == 0)
                return;

            argv[argc] = NULL;

            run_program(argv, argc, foreground, doing_pipe);

            input_fd    = 0;
            output_fd    = 0;
            argc        = 0;

            if (type == NEWLINE)
                return;

            break;
        }
    }
}

/* init_search_path: make a list of directories to look for programs in. */
static void init_search_path(void)
{
    char*        dir_start;
    char*        path;
    char*        s;
    list_t*        p;
    bool        proceed;

    path = getenv("PATH");

    /* path may look like "/bin:/usr/bin:/usr/local/bin"
     * and this function makes a list with strings
     * "/bin" "usr/bin" "usr/local/bin"
      *
     */

    dir_start = malloc(1+strlen(path));
    if (dir_start == NULL) {
        error("out of memory.");
        exit(1);
    }

    strcpy(dir_start, path);

    path_dir_list = NULL;

    if (path == NULL || *path == 0) {
        path_dir_list = new_list("");
        return;
    }

    proceed = true;

    while (proceed) {
        s = dir_start;
        while (*s != ':' && *s != 0)
            s++;
        if (*s == ':')
            *s = 0;
        else
            proceed = false;

        insert_last(&path_dir_list, dir_start);

        dir_start = s + 1;
    }

    p = path_dir_list;

    if (p == NULL)
        return;

#if 0
    do {
        printf("%s\n", (char*)p->data);
        p = p->succ;
    } while (p != path_dir_list);
#endif
}

/* main: main program of simple shell. */
int main(int argc, char** argv)
{
    progname = argv[0];

    init_search_path();
    while (fetch_line("% ") != EOF){
        parse_line();
    }
    return 0;
}





