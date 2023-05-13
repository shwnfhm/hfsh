//*********************************************************
//
// Shawn Fahimi
// Operating Systems
// Programming Project #2: hfsh
// February 14, 2023
// Instructor: Dr. Michael Scherger
//
//*********************************************************

//*********************************************************
// Includes and Defines
//*********************************************************
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

//*********************************************************
// Extern Declarations
//*********************************************************

//*********************************************************
// Buffer state. This is used to parse string in memory...
// Leave this alone.
//*********************************************************
extern "C"{
    extern char **gettoks();
    typedef struct yy_buffer_state * YY_BUFFER_STATE;
    extern YY_BUFFER_STATE yy_scan_string(const char * str);
    extern YY_BUFFER_STATE yy_scan_buffer(char *, size_t);
    extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
    extern void yy_switch_to_buffer(YY_BUFFER_STATE buffer);
}

//*********************************************************
// Namespace
//*********************************************************

//*********************************************************
// If you are new to C++, uncommment the `using namespace std`
// line. It will make your life a little easier.  However,
// all true C++ developers will leave this commented.  ;^)
//*********************************************************
using namespace std;

//*********************************************************
// Function Prototypes
//*********************************************************
int builtInCmd(char **cmd, char **paths, int *pids);
int execute(char **cmd, char **paths);
int preProcess(char **cmd, char **paths);
void waitpids(int *pids, int numPids);
void runShell(int mode, FILE *f);

//*********************************************************
//
// buildInCmd - Built In Command Handler
//
// Executes the built in command specified by 'cmd'
// (either 'path', 'exit', or 'cd') according to the
// instructions provided in the README. 'pids' is passed
// in solely for freeing in the case of 'exit'.
//
// Return Value
// ------------
// int      0 if command is successfully executed, 1 if not
//
// Function Parameters
// -------------------
// cmd      char **     reference   the command to be executed, with arguments stored as tokens (not accounting for redirection)
// paths    char **     reference   the search path 
// pids     int *       reference   the array of pids executing on a given run of the shell loop (for freeing in case of 'exit')
//
// Local Variables
// ---------------
// None
//
//*********************************************************
int builtInCmd(char **cmd, char **paths, int *pids){

    if(!strcmp(cmd[0], "path")){    //handle built in 'path' command

        for(int i = 0; paths[i] != NULL; i++){  //clear search path
            paths[i] = NULL;
        }

        for(int ii=1; cmd[ii] != NULL; ii++){   //add each path argument to the search path
            paths[ii - 1] = cmd[ii];
        }
        return 0;

    }
    else if(!strcmp(cmd[0], "exit")){   //handle built in 'exit' command

        if(cmd[1] != NULL){ //if 'exit' is sent in with additional arguments
            fprintf(stderr, "An error has occurred\n");
            return 1;
        }
        else{   //otherwise...free memory and exit
            cmd = NULL; 
            pids = NULL;
            free(pids);
            free(cmd);
            exit(0);
        }

    }
    else{   //handle built in 'cd' command

        if(cmd[1] == NULL || cmd[2] != NULL){   //if 0 or more than 1 directory is specified
            fprintf(stderr, "An error has occurred\n");
            return 1;
        }
        else{
            if(chdir(cmd[1]) < 0){   //change directory and check for errors
                fprintf(stderr, "An error has occurred\n");
                return 1;
            }
            return 0;
        }
    }
}

//*********************************************************
//
// execute - Execute Command Function
//
// Executes the command specified by 'cmd' by forking
// a child process and searching through the search path
// for the command, executing with a call to execvp if
// found. The child process exits with a return code 
// of 0 if successfully executed, 1 if not.
//
// Return Value
// ------------
// int      the pid of the child process (0 if child)
//
// Function Parameters
// -------------------
// cmd      char **     reference   the command to be executed, with arguments stored as tokens (minus redirection arguments)
// paths    char **     reference   the search path 
//
// Local Variables
// ---------------
// pid      int         the pid of the child process, to be returned
// path     char *      used to store the final path to the command
//
//*********************************************************
int execute(char **cmd, char **paths){

    int pid; //holds the pid of the child process
    char *path; //holds the path to the command

    if((pid = fork()) < 0){ //fork a child process and check for any errors
        fprintf(stderr, "An error has occurred\n");
        return pid;
    }

    else if((pid == 0) && paths[0] != NULL){    //if the child process, search for the command in the search path

        //iterate through the search path and check if the command exists in any of the paths
        for(int i = 0; paths[i] != NULL; i++){

            //create a path to the command by concatenating the current 
            //path with the command
            path = (char *)malloc(sizeof(char) * 256);
            strcpy(path, paths[i]);
            strcat(path, "/");
            strcat(path, cmd[0]);

            //if the command does not exist in the path and there are 
            //no more paths to check, exit with an error
            if(access(path, X_OK) != 0 && paths[i+1] == NULL){
                fprintf(stderr, "An error has occurred\n"); //print error message, free memory and exit
                path = NULL;
                free(path);
                exit(1);
            }

            //if the command does not exist in the path, 
            //free the path and continue to the next path
            else if(access(path, X_OK) != 0){
                path = NULL;    //reset path for next iteration
                free(path);
                continue;
            }

            //if the command exists in the path, we have found
            //a valid path and can break out of the loop
            else if(access(path, X_OK) == 0){
                break;
            }
        }

        //attempt to execute the path with execvp and check for any errors
        if(execvp(path, cmd) < 0){
            fprintf(stderr, "An error has occurred\n"); //print error message, free memory and exit
            path = NULL;
            free(path);
            exit(1);
        }
    }

    //if the search path is empty, exit with an error-
    //since a built in command will not find its way to
    //this function
    else if((pid == 0) && paths[0] == NULL){
        fprintf(stderr, "An error has occurred\n");
        exit(1);
    }
    return pid;
}

//*********************************************************
//
// preProcess - Command Preprocessing Function
//
// Prepares a command for execution by handling any 
// necessary output and error redirections before
// executing the command, and cleans up said
// redirections after the command has been executed.
//
// Return Value
// ------------
// int          the pid of the child process created in execute, or -1 if redirection is unsuccessful
//
// Function Parameters
// -------------------
// cmd          char **     reference   the command to be executed, arguments stored as tokens
// paths        char **     reference   the search path of the shell
//
// Local Variables
// -------------------
// cmd1         char **     the command to be executed, minus any redirection arguments, with arguments stored as tokens
// redirect     int         flag to check if redirection is needed, 0 if no, 1 if yes
// pid          int         the pid of the child process generated in the call to execute(cmd1, paths)
// out          int         file descriptor for stdout redirection
// error        int         file descriptor for stderr redirection
// save_out     int         duplicate file descriptor for stdout
// save_err     int         duplicate file descriptor for stderr
//
//*********************************************************
int preProcess(char **cmd, char **paths){

    int redirect = 0, pid = 0; //flag to check if redirection is needed and PID returned from execute call, respectively 
    int out, error, save_out, save_err; //file descriptors for redirection
    char **cmd1 = (char **)malloc(sizeof(char *) * 256);   //initialize cmd1 (holds the command to be executed minus any redirection arguments)

    for(int i = 0; cmd[i] != NULL; i++){    //check for redirection arguments and handle them if necessary

        if(!strcmp(cmd[i], ">")){   //if redirection symbol is found

            //if 0 or more than 1 file is provided for redirection,
            //or if multiple redirection tokens are given, exit
            //with an error
            if(i == 0 || cmd[i+1] == NULL || cmd[i+2] != NULL || !strcmp(cmd[i+1], ">")){
                fprintf(stderr, "An error has occurred\n");
                cmd1 = NULL;
                free(cmd1);
                return -1;
            }
            //otherwise, open necessary files for redirection
            //and redirect stdout and stderr to the provided file
            else{
                redirect = 1;

                //check for errors in opening files
                if((out = open(cmd[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0 || (error = open(cmd[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0){ 
                    fprintf(stderr, "An error has occurred\n");
                    cmd1 = NULL;
                    free(cmd1);
                    if(out > 0){
                        close(out);
                    }
                    if(error > 0){
                        close(error);
                    }
                    return -1;
                }
                //checking return value of dup system call
                if((save_out = dup(fileno(stdout))) < 0 || (save_err = dup(fileno(stderr))) < 0){
                    fprintf(stderr, "An error has occurred\n");
                    cmd1 = NULL;
                    free(cmd1);
                    close(out);
                    close(error);
                    if(save_out > 0){
                        close(save_out);
                    }
                    if(save_err > 0){
                        close(save_err);
                    }
                    return -1;
                }
                //checking return value of dup2 system call
                if(dup2(out, fileno(stdout)) < 0 || dup2(error, fileno(stderr)) < 0){
                    fprintf(stderr, "An error has occurred\n");
                    cmd1 = NULL;
                    free(cmd1);
                    close(out);
                    close(error);
                    close(save_out);
                    close(save_err);
                    return -1;
                }
            }
            break;
        }
        cmd1[i] = cmd[i];
    }

    pid = execute(cmd1, paths); //execute the command

    if(redirect){   //redirection cleanup performed if necessary
        fflush(stdout);
        close(out);
        fflush(stderr);
        close(error);
        //checking return value of dup2 system call
        if(dup2(save_out, fileno(stdout)) < 0 || dup2(save_err, fileno(stderr)) < 0){ 
            fprintf(stderr, "An error has occurred\n");
            pid = -1;   //just set pid to -1 since we're exiting anyway
        }
        close(save_out);
        close(save_err);
    }
    cmd1 = NULL;    //free dynamically allocated memory and return
    free(cmd1);
    return pid;
}

//*********************************************************
//
// waitPids - PID waiting function
//
// Waits on all child processes with PIDs specified in the
// array 'pids' to finish. The number of entries in the PID
// array (not necessarily equal to the number of running
// child processes) is specified by 'numPids'.
//
// Return Value
// ------------
// void     no return value
//
// Function Parameters
// -------------------
// pids     int *   reference   the array of PIDs to wait for
// numPids  int     value       the number of entries in the PID array
//
// Local Variables
// ---------------
// None
//
//*********************************************************
void waitpids(int *pids, int numPids){

    for(int i = 0; i < numPids; i++){
        if(pids[i] < 0){    //if this entry is not a child process (redirection/forking error), skip it
            continue;
        }
        else{   //otherwise, wait for the child process to finish
            waitpid(pids[i], NULL, 0);
        }
    }

}

//*********************************************************
//
// runShell - Shell Loop Function
//
// Runs the shell as a loop in the mode specificed 
// by the 'mode' parameter (1 = interactive,
// 2 = batch). If in interactive mode, a prompt will
// be given to the user to enter a command, which the
// loop will then parse and preprocess for execution. 
// If in batch mode, the loop will parse through each 
// line for commands to execute similarly. The loop will 
// continue until the user enters 'exit' or an EOF marker
// is reached.
//
// Return Value
// ------------
// void     no return value
//
// Function Parameters
// -------------------
// mode     int     value       the mode of the shell (1 = interactive, 2 = batch)
// f        FILE *  reference   file pointer to stdin or the batch file (if one is specified)
//
// Local Variables
// ---------------
// ii       int                 counter variable used to iterate through tokens of each line of input
// jj       int                 counter variable used to load tokens of each individual command into 'cmd'
// kk       int                 counter variable used to keep track of number of commands running in parallel per input line
// toks     char **             2D array of tokens
// linestr  char[]              holds current line of input
// buffer   YY_BUFFER_STATE     used to parse string in memory
// cmd      char **             used to store tokens of each individual command before sending for execution
// paths    char **             the search path of the shell, initially just '/bin'
// pids     int *               holds the pids of all executing processes during a given iteration of the shell loop
//
//*********************************************************
void runShell(int mode, FILE *f){

    int ii = 0, jj = 0, kk = 0; //counter variables used to iterate through tokens and individual commands, respectively
    char linestr[256];  //holds each line of input
    YY_BUFFER_STATE buffer; //used to parse string in memory
    char **toks, **cmd, **paths; //used to store the 2d array of tokens and tokens of each command and the search path of the shell, respectively
    int *pids; //holds the pids of all executing processes during a given iteration of the shell
    
    paths = (char **)malloc(sizeof(char *) * 256);  //initialize search path with /bin as the only path
    paths[0] = (char *)"/bin";

    if(mode == 1){  //print prompt if in interactive mode
        fprintf(stdout, "hfsh2> ");
    }

    while(fgets(linestr, 256, f)){

        pids = (int *)malloc(sizeof(int) * 256);     //initialize pid and cmd arrays
        cmd = (char **)malloc(sizeof(char *) * 256);

        if(!strstr(linestr, "\n")){     //make sure line has a '\n' at the end of it
            strcat(linestr, "\n");
        }

        buffer = yy_scan_string(linestr);   //get arguments
        yy_switch_to_buffer(buffer);
        toks = gettoks();
        yy_delete_buffer(buffer);

        if(toks[0] != NULL){

            for(ii=0; toks[ii] != NULL; ii++){  //loop through all tokens
                
                if(!strcmp(toks[ii], "&")){ //if we are at the end of a command, execute it
                                            //'&' implies this is the end of a command
                    if(cmd[0] != NULL){
                        //if the command is a built in command, execute it accordingly
                        if(!strcmp(cmd[0], "exit") || !strcmp(cmd[0], "path") || !strcmp(cmd[0], "cd")){
                            builtInCmd(cmd, paths, pids);
                        }
                        else{   //not a built in command, send for preprocessing and execution
                            pids[kk++] = preProcess(cmd, paths);
                        }
                    }

                    jj = 0; //reset counter
                    cmd = NULL; //reset cmd array
                    free(cmd);
                    cmd = (char **)malloc(sizeof(char *) * 256);
                }
                else{   //otherwise, we are still parsing an individual command...
                    cmd[jj++] = toks[ii];
                }
            }
           
            if(cmd[0] != NULL){
                //if the command is a built in command, execute it accordingly
                if(!strcmp(cmd[0], "exit") || !strcmp(cmd[0], "path") || !strcmp(cmd[0], "cd")){
                    builtInCmd(cmd, paths, pids);
                }
                else{   //not a built in command, send for preprocessing and execution
                    pids[kk++] = preProcess(cmd, paths);
                }
            }
        }

        waitpids(pids, kk);  //wait for all processes to finish
    
        kk = 0, jj = 0;     //reset cmd, pid, kk and jj
        cmd = NULL;
        pids = NULL;
        free(cmd);
        free(pids);

        if(mode == 1){   //reprint prompt if in interactive mode
            fprintf(stdout, "hfsh2> ");
        }
    }
}

//*********************************************************
//
// Main Function
//
// Initializes a file object pointer 'f' that stores either 
// stdin or a batch file if one is provided, then calls
// runShell(argc, f) with the appropriate parameters to 
// begin execution of the shell.
//
// Return Value
// ------------
// int      0 if successfully executed, 1 if not
//
// Function Parameters
// -------------------
// argc     int     value      number of arguments
// argv     char*[] reference  arguments
//
// Local Variables
// ---------------
// f        FILE *      pointer to either stdin or batch file (if provided)
//
//*********************************************************
int main(int argc, char *argv[]){
    
    FILE *f; //holds batch file (if provided)
    
    if(argc == 1){  //if we are running in interactive mode
        f = stdin;
    }
    else if(argc == 2){ //if we are running in batch mode
        f = fopen(argv[1], "r");

        if(f == NULL){  //check for any errors opening the file
            fprintf(stderr, "An error has occurred\n");
            exit(1);
        }
    }
    else{   //improper usage
        fprintf(stderr, "An error has occurred\n");
        exit(1);
    }

    runShell(argc, f);

    return 0;
}