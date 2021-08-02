////////////////////////////////////////////////////////////////////////
// COMP1521 21t2 -- Assignment 2 -- shuck, A Simple Shell
// <https://www.cse.unsw.edu.au/~cs1521/21T2/assignments/ass2/index.html>
//
// Written by YOUR-NAME-HERE (z5555555) on INSERT-DATE-HERE.
//
// 2021-07-12    v1.0    Team COMP1521 <cs1521@cse.unsw.edu.au>
// 2021-07-21    v1.1    Team COMP1521 <cs1521@cse.unsw.edu.au>
//     * Adjust qualifiers and attributes in provided code,
//       to make `dcc -Werror' happy.
//

#include <sys/types.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// [[ TODO: put any extra `#include's here ]]
#include <spawn.h>
#include <glob.h>

// [[ TODO: put any `#define's here ]]
#define MAX_HISTORY 2048

//
// Interactive prompt:
//     The default prompt displayed in `interactive' mode --- when both
//     standard input and standard output are connected to a TTY device.
//
static const char *const INTERACTIVE_PROMPT = "shuck& ";

//
// Default path:
//     If no `$PATH' variable is set in Shuck's environment, we fall
//     back to these directories as the `$PATH'.
//
static const char *const DEFAULT_PATH = "/bin:/usr/bin";

//
// Default history shown:
//     The number of history items shown by default; overridden by the
//     first argument to the `history' builtin command.
//     Remove the `unused' marker once you have implemented history.
//
static const int DEFAULT_HISTORY_SHOWN __attribute__((unused)) = 10;

//
// Input line length:
//     The length of the longest line of input we can read.
//
static const size_t MAX_LINE_CHARS = 1024;

//
// Special characters:
//     Characters that `tokenize' will return as words by themselves.
//
static const char *const SPECIAL_CHARS = "!><|";

//
// Word separators:
//     Characters that `tokenize' will use to delimit words.
//
static const char *const WORD_SEPARATORS = " \t\r\n";

// [[ TODO: put any extra constants here ]]

static const char * history = "..shuck_history";

// [[ TODO: put any type definitions (i.e., `typedef', `struct', etc.) here ]]


static void execute_command(char **words, char **path, char **environment);
static void do_exit(char **words);
static int is_executable(char *pathname);
static char **tokenize(char *s, char *separators, char *special_chars);
static void free_tokens(char **tokens);

// [[ TODO: put any extra function prototypes here ]]

static void do_pwd(char **words);
static void do_cd(char **words);

static char* search_program(char* program,char**path);
static void run_command(char* program,char** words,char** environment);

static char* history_path();
static int read_history();
static void do_history(char **words);
static void do_last_command(char **words,char**path,char**environment);
static void write_to_history(char**command_words);

static char ** do_glob(char** words);

int main (void)
{
    // Ensure `stdout' is line-buffered for autotesting.

    setlinebuf(stdout);
    // Environment variables are pointed to by `environ', an array of
    // strings terminated by a NULL value -- something like:
    //     { "VAR1=value", "VAR2=value", NULL }
    extern char **environ;

    // Grab the `PATH' environment variable for our path.
    // If it isn't set, use the default path defined above.
    char *pathp;
    if ((pathp = getenv("PATH")) == NULL) {
        pathp = (char *) DEFAULT_PATH;
    }
    char **path = tokenize(pathp, ":", "");

    // Should this shell be interactive?
    bool interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);

    // Main loop: print prompt, read line, execute command
    while (1) {
        // If `stdout' is a terminal (i.e., we're an interactive shell),
        // print a prompt before reading a line of input.
        if (interactive) {
            fputs(INTERACTIVE_PROMPT, stdout);
            fflush(stdout);
        }

        char line[MAX_LINE_CHARS];
        if (fgets(line, MAX_LINE_CHARS, stdin) == NULL)
            break;

        // Tokenise and execute the input line.
        char **command_words =
            tokenize(line, (char *) WORD_SEPARATORS, (char *) SPECIAL_CHARS);
        execute_command(command_words, path, environ);
        free_tokens(command_words);
    }
    free_tokens(path);
    return 0;
}


//
// Execute a command, and wait until it finishes.
//
//  * `words': a NULL-terminated array of words from the input command line
//  * `path': a NULL-terminated array of directories to search in;
//  * `environment': a NULL-terminated array of environment variables.
//
static void execute_command(char **words, char **path, char **environment)
{
    assert(words != NULL);
    assert(path != NULL);
    assert(environment != NULL);
    char *program = words[0];
    if (program == NULL) {
        // nothing to do
        return;
    }
    if (strcmp(program, "exit") == 0) {
        do_exit(words);
        return;
    }

    // [[ TODO: add code here to implement subset 0 ]]
    if(strcmp(program,"pwd")==0){
        do_pwd(words);
        write_to_history(words);
        return ;
    }
    if(strcmp(program,"cd")==0){
        do_cd(words);
        write_to_history(words);
        return ;
    }
    if(strcmp(program,"history")==0){
        do_history(words);
        write_to_history(words);
        return ;
    }
    if(strcmp(program,"!")==0){
        do_last_command(words,path,environment);
        return;
    }

    // [[ TODO: change code below here to implement subset 1 ]]
    char** command_argv = do_glob(words);

    if(strrchr(program,'/')==NULL){
        program = search_program(program,path);
    }
    if (is_executable(program)) {
        run_command(program,command_argv,environment);
        write_to_history(words);
    } else {
        fprintf(stderr,"%s: command not found\n",words[0]);
        write_to_history(words);
    }
    if(strcmp(program,words[0])!=0){
        free(program);
    }
    free_tokens(command_argv);
}


//
// Implement the `exit' shell built-in, which exits the shell.
//
// Synopsis: exit [exit-status]
// Examples:
//     % exit
//     % exit 1
//
static void do_exit(char **words)
{
    assert(words != NULL);
    assert(strcmp(words[0], "exit") == 0);

    int exit_status = 0;

    if (words[1] != NULL && words[2] != NULL) {
        // { "exit", "word", "word", ... }
        fprintf(stderr, "exit: too many arguments\n");

    } else if (words[1] != NULL) {
        // { "exit", something, NULL }
        char *endptr;
        exit_status = (int) strtol(words[1], &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "exit: %s: numeric argument required\n", words[1]);
        }
    }

    exit(exit_status);
}

static void do_pwd(char **words){
    assert(words != NULL);
    assert(strcmp(words[0], "pwd") == 0);
    char pathname[MAX_LINE_CHARS];
    if (words[1] != NULL ) {
        fprintf(stderr, "pwd: too many arguments\n");
     }else if (getcwd(pathname, sizeof pathname) == NULL) {
        perror("getcwd");
    }else{
        printf("current directory is \'%s\'\n",pathname);
    }
    return;
}

static void do_cd(char **words){
    assert(words != NULL);
    assert(strcmp(words[0], "cd") == 0);
   if (words[1] != NULL && words[2] != NULL) {
        fprintf(stderr, "cd: too many arguments\n");
    }else if(words[1]==NULL){
        char *HOMEPATH  = getenv("HOME");
        if(chdir(HOMEPATH)!=0){
            perror("chdir");
        }
    }else if(chdir(words[1]) != 0){
        perror("chdir");
    }
    return ;
}

static char* search_program(char* program,char**path){
    char *result = NULL;
    result = malloc(sizeof(char)*MAX_LINE_CHARS);
    for(int i=0;path[i]!=NULL;i++){
        int len = strlen(path[i])+strlen(program)+1;
        strcpy(result,path[i]);
        strcat(result,"/");
        strcat(result,program);
        result[len] = '\0';
        if (is_executable(result)){
            return result;
        }
    }
    free(result);
    result = NULL;
    return NULL;
}

static void run_command(char *program,char**words,char**environment){
    pid_t pid;
    if (posix_spawn(&pid,program, NULL, NULL, words, environment) != 0) {
        perror("spawn");
        exit(1);
    }
    int exit_status;
    if (waitpid(pid, &exit_status, 0) == -1) {
        perror("waitpid");
        exit(1);
    }
    printf("%s exit status = %d\n",program,exit_status);
}

static char* history_path(){
    char *HOMEPATH  = getenv("HOME");
    char* history_file = malloc(sizeof(HOMEPATH)+sizeof(history)+2);
    strcpy(history_file,HOMEPATH);
    strcpy(history_file,"/");
    strcpy(history_file,history);
    return history_file;
}


static int read_history(){
    char *history_file = history_path();
    FILE *f = fopen(history_file,"r");
    if(!f){
        free(history_file);
        return 0;
    }
    int number = 0;
    char buf[MAX_LINE_CHARS];
    while (fgets(buf, sizeof buf,f) != NULL){
        int len = strlen(buf);
        buf[len] = '\0'; 
        number++;
    }
    free(history_file);
    return number;
}

static void write_to_history(char**command_words){ 
    char *history_file = history_path();
    int number_history = read_history(history_file);
    FILE *f = NULL;
    if(number_history==0){
        f = fopen(history_file,"w");
    }else{
        f = fopen(history_file,"a");
    }
    for(int i=0;command_words[i]!=NULL;i++){
        fprintf(f,"%s ",command_words[i]);
    }
    fprintf(f,"\n");
    fclose(f);
    free(history_file);
}

static void do_history(char **words){
    assert(words != NULL);
    assert(strcmp(words[0], "history") == 0);
    int history_line = 10;
    if (words[1] != NULL && words[2] != NULL) {
        fprintf(stderr, "history: too many arguments\n");
    } else if (words[1] != NULL) {
        char *endptr;
        history_line = (int) strtol(words[1], &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "history: %s: numeric argument required\n", words[1]);
        }
        if(history_line<0){
            fprintf(stderr, "history: %s: unsigned numeric argument required \n", words[1]);
        }
    }
    char *history_file = history_path();
    int number_history = read_history(history_file);
    if(number_history==0) return ;
    FILE* f = fopen(history_file,"r");
    char buf[MAX_LINE_CHARS];
    int skip_line = number_history-history_line;
    if(skip_line>0){
        for(int i=0;i<skip_line;i++){
            fscanf(f,"%*[^\n]%*c");
        }
    }
    int i = skip_line>0?skip_line:0;
    while (fgets(buf, sizeof buf,f) != NULL)
    {
        int len = strlen(buf);
        buf[len] = '\0'; 
        printf("%d: %s",i,buf);
        i++;
    }
    fclose(f);
    free(history_file);
}

static void do_last_command(char **words,char**path,char**environment){
    int last_command = 0;
    if (words[1] != NULL && words[2] != NULL) {
        fprintf(stderr, "!: too many arguments\n");
    } else if (words[1] != NULL) {
        char *endptr;
        last_command = (int) strtol(words[1], &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "!: %s: numeric argument required\n", words[1]);
            return ;
        }
        if(last_command<0){
            fprintf(stderr, "!: %s: unsigned numeric argument required \n", words[1]);
            return ;
        }
    }
    char* history_file = history_path();
    int number_history = read_history(history_file);
    if(number_history==0) return ;
    FILE* f = fopen(history_file,"r");
    int skip_line = last_command==0?number_history-1:last_command;
    char buf[MAX_LINE_CHARS];
    if(skip_line>0){
        for(int i=0;i<skip_line;i++){
            fscanf(f,"%*[^\n]%*c");
        }
    }
    fgets(buf, sizeof buf,f);
    int len = strlen(buf);
    buf[len] = '\0';
    fclose(f);
    printf("%s",buf);
    char** command_words=tokenize(buf, (char *) WORD_SEPARATORS, (char *) SPECIAL_CHARS);
    execute_command(command_words, path, environment);
    free(history_file);
    free_tokens(command_words);
}


static char ** do_glob(char** words){
    glob_t matches; 
    char** result = malloc(sizeof(words)+MAX_LINE_CHARS*sizeof(char));
    int n = 0;
    for(int i=0;words[i]!=NULL;i++){
        if(strchr(words[i],'*')!=NULL||
        strchr(words[i],'[')!=NULL||
        strchr(words[i],'~')!=NULL||
        strchr(words[i],'?')!=NULL){
            int resul = glob(words[i], GLOB_NOCHECK|GLOB_TILDE, NULL, &matches);
            if (resul != 0) {
                printf("glob returns %d\n", result);
            } else {
                for (int j = 0; j < matches.gl_pathc; j++) {
                    result[n] = malloc((strlen(matches.gl_pathv[j])+1)*sizeof(char));
                    strcpy(result[n], matches.gl_pathv[j]);
                    n++;
                }
                globfree(&matches);
            }
        }else{
            result[n] = malloc((strlen(words[i])+1)*sizeof(char));
            strcpy(result[n],words[i]);
            n++;
        }
    }
    result[n] = NULL;
    result = realloc(result, (n + 1) * sizeof *result);
    return result;
}

//
// Check whether this process can execute a file.  This function will be
// useful while searching through the list of directories in the path to
// find an executable file.
//
static int is_executable(char *pathname)
{
    struct stat s;
    return
        // does the file exist?
        stat(pathname, &s) == 0 &&
        // is the file a regular file?
        S_ISREG(s.st_mode) &&
        // can we execute it?
        faccessat(AT_FDCWD, pathname, X_OK, AT_EACCESS) == 0;
}


//
// Split a string 's' into pieces by any one of a set of separators.
//
// Returns an array of strings, with the last element being `NULL'.
// The array itself, and the strings, are allocated with `malloc(3)';
// the provided `free_token' function can deallocate this.
//
static char **tokenize(char *s, char *separators, char *special_chars)
{
    size_t n_tokens = 0;

    // Allocate space for tokens.  We don't know how many tokens there
    // are yet --- pessimistically assume that every single character
    // will turn into a token.  (We fix this later.)
    char **tokens = calloc((strlen(s) + 1), sizeof *tokens);
    assert(tokens != NULL);

    while (*s != '\0') {
        // We are pointing at zero or more of any of the separators.
        // Skip all leading instances of the separators.
        s += strspn(s, separators);

        // Trailing separators after the last token mean that, at this
        // point, we are looking at the end of the string, so:
        if (*s == '\0') {
            break;
        }

        // Now, `s' points at one or more characters we want to keep.
        // The number of non-separator characters is the token length.
        size_t length = strcspn(s, separators);
        size_t length_without_specials = strcspn(s, special_chars);
        if (length_without_specials == 0) {
            length_without_specials = 1;
        }
        if (length_without_specials < length) {
            length = length_without_specials;
        }

        // Allocate a copy of the token.
        char *token = strndup(s, length);
        assert(token != NULL);
        s += length;

        // Add this token.
        tokens[n_tokens] = token;
        n_tokens++;
    }

    // Add the final `NULL'.
    tokens[n_tokens] = NULL;

    // Finally, shrink our array back down to the correct size.
    tokens = realloc(tokens, (n_tokens + 1) * sizeof *tokens);
    assert(tokens != NULL);

    return tokens;
}


//
// Free an array of strings as returned by `tokenize'.
//
static void free_tokens(char **tokens)
{
    for (int i = 0; tokens[i] != NULL; i++) {
        free(tokens[i]);
    }
    free(tokens);
}
