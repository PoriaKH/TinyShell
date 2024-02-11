#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>

#define FALSE 0
#define TRUE 1
#define INPUT_STRING_SIZE 80

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"

void handle_signals();
pid_t shell_pid;

pid_t running_pids[50]; //0 is for shell
int count = 0;
int signal_counter = 0;

pid_t bg_pids[50];//background process
int bg_counter = 0;

void set_signal_counter(){//CTRL Z Happend
   if(signal_counter > 0)
      signal_counter--;
}

int cmd_quit(tok_t arg[]) {
  printf("Bye\n");
  exit(0);
  return 1;
}
int cmd_pwd(tok_t arg[]);
int cmd_help(tok_t arg[]);
int cmd_cd(tok_t arg[]);
int cmd_wait(tok_t arg[]);

/* Command Lookup table */
typedef int cmd_fun_t (tok_t args[]); /* cmd functions take token array and return int */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_quit, "quit", "quit the command shell"},
  {cmd_cd, "cd", "change directory"},
  {cmd_pwd, "pwd","show current path"},
  {cmd_wait, "wait", "wait till end of each child process"},
};

int cmd_wait(tok_t arg[]){
   wait(NULL);
   return 1;
}
int cmd_pwd(tok_t arg[]){
  char cwd[500];
   if (getcwd(cwd, sizeof(cwd)) != NULL) {
       printf("%s\n", cwd);
   } else {
       perror("getcwd() error\n");
       return 0;
   }
   return 1;
}
int cmd_cd(tok_t arg[]){
   int result = chdir(arg[0]);
   if(result){
      printf("invalid path\n");
      return 0;
   }

   return 1;
}
int cmd_help(tok_t arg[]) {
  int i;
  for (i=0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
    printf("%s - %s\n",cmd_table[i].cmd, cmd_table[i].doc);
  }
  return 1;
}

int lookup(char cmd[]) {
  int i;
  for (i=0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
  }
  return -1;
}

void init_shell()
{
  /* Check if we are running interactively */
  shell_terminal = STDIN_FILENO;

  /** Note that we cannot take control of the terminal if the shell
      is not interactive */
  shell_is_interactive = isatty(shell_terminal);

  if(shell_is_interactive){

    /* force into foreground */
    while(tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp()))
      kill( - shell_pgid, SIGTTIN);

    shell_pgid = getpid();
    /* Put shell in its own process group */
    if(setpgid(shell_pgid, shell_pgid) < 0){
      perror("Couldn't put the shell in its own process group");
      exit(1);
    }

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);
    tcgetattr(shell_terminal, &shell_tmodes);
  }
  /** YOUR CODE HERE */
}

/**
 * Add a process to our process list
 */
void add_process(pid_t pid){
  
  /** YOUR CODE HERE */
  running_pids[count] = pid;
  count++;
  signal_counter++;
  
  tcsetpgrp(shell_terminal, pid);
  //printf("after tcset : %d\n",pid);
}
void remove_process(pid_t pid){
   int arg = -1;
   for(int i = 0; i < count; i++){
      if(running_pids[i] == pid){
         arg = i;
         break;
      }
   }
   if(arg == -1){
      printf("problem with removing the process\n");
      return;
   }
   for(int i = arg; i < count - 1; i++){
      running_pids[i] = running_pids[i + 1];
   }
  count--;
  signal_counter--;

  tcsetpgrp(shell_terminal, running_pids[signal_counter - 1]);
  //printf("after tcset : %d\n",running_pids[signal_counter - 1]);
}

void show_running (){
   for(int i = 0; i < count; i++)
      printf("PID = %d\n",running_pids[i]);
   printf("\n");
}

/**
 * Creates a process given the inputString from stdin
 */
process* create_process(char* inputString)
{
  /** YOUR CODE HERE */
  return NULL;
}


void findCommand(tok_t *t){
   //handle_signals();
   char* PATH = getenv("PATH");
   //printf("$PATH = %s\n", PATH);
   
   tok_t *p = getToks(PATH);
   
   for(int i = 0; i < getSize(p); i++){
      p[i] = append(p[i], t[0]);
   }
   
   int result;
   int len = getSize(p);

   for(int i = 0; i < len; i++){
      result = 1;
      result = execv(p[i],t);
      if(result != -1)
         break;
   }
   printf("invalid path\n");
}

void redirectIn(char *fileName)   //<
{
    int in = open(fileName, O_RDONLY);
    dup2(in, 0);
    close(in);
}

void redirectOut(char *fileName)  //>
{
    int out = open(fileName, O_WRONLY | O_TRUNC | O_CREAT, 0600);
    dup2(out, 1);
    close(out);
}

void ctrlz_func(){//CTRL Z Happend
   set_signal_counter;
   bg_pids[bg_counter] = tcgetpgrp(shell_terminal);
   bg_counter++;
   
   //make that process wait
   
   tcsetpgrp(shell_terminal, running_pids[signal_counter]);
   
}
void sigcont_func(){
   tcsetpgrp(shell_terminal, bg_pids[bg_counter-1]);
   bg_pids[bg_counter - 1] = NULL;
   bg_counter--;
}
void sig_handler(int sig){
   switch(sig) {
      case SIGTSTP: //CTRL Z
        // printf("used CTRL Z\n");
         ctrlz_func();
         //signal(SIGTSTP,SIG_IGN);
         break;
      case SIGINT:
           // printf("used CTRL C!\n");
         break;
      case SIGKILL:
         signal(SIGKILL,SIG_DFL);
         break;
      case SIGCONT:
         sigcont_func();
         signal(SIGCONT,SIG_DFL);
         break;
      default:
         break;
      
      
   }
   
//   fprintf(stderr, "were in sig_handler\n");
}

void handle_signals(){
    signal(SIGINT, sig_handler);
    signal(SIGTSTP, sig_handler);
    //signal(SIGINT, SIG_DFL);
   // signal(SIGTSTP, SIG_IGN);
}

int shell (int argc, char *argv[]) {
  char *s = malloc(INPUT_STRING_SIZE+1);			/* user input string */
  tok_t *t;			/* tokens parsed from input */
  int lineNum = 0;
  int fundex = -1;
  pid_t pid = getpid();		/* get current processes PID */
  pid_t ppid = getppid();	/* get parents PID */
  pid_t cpid, tcpid, cpgid;

  init_shell();
   
  shell_pid = getpid();
  // printf("%s running as PID %d under %d\n",argv[0],pid,ppid);
 // printf("shell pid = %d\n",shell_pid);
  add_process(shell_pid);

  //testing add process
  /*
  show_running();
  add_process(1234);
  add_process(111);
  add_process(555);
  show_running();
  remove_process(111);
  show_running();
  */
   
  lineNum=0;
  // fprintf(stdout, "%d: ", lineNum);
  handle_signals();
  printf("mercy$ ");
  while ((s = freadln(stdin))){
    int andFlag = 0;
    t = getToks(s); /* break the line into tokens */
    tok_t* args = malloc(MAXTOKS*sizeof(tok_t));
    int counter = 0;
    
    for(int i = 0; i < getSize(t); i++){
       
          
          if (strcmp(t[i], "<") == 0) {
             redirectIn(t[i + 1]);
             i++;
          } 
          else if (strcmp(t[i] , ">") == 0) {
             redirectOut(t[i + 1]);
             i++;
          }
          else if (strcmp(t[i], "&") == 0){
             andFlag = 1;
          }
          else {
             args[counter] = t[i];
             counter++;
          }
          
    } 
    args[counter] = NULL;
    t = args;

    fundex = lookup(t[0]); /* Is first token a shell literal */
    if(fundex >= 0) cmd_table[fundex].fun(&t[1]);
    else {
      int status;
      pid_t pid;
      //handle_signals();
      int res = 1;
      
      pid = fork();
      //add_process(pid);

      if(pid == 0){
         //child
         //t[0] = path, t[1] = arg
         pid_t child_pid = getpid();
        // printf("child_pid in child : %d\n",child_pid);
                
         int status2;
         pid_t pid2 = fork();
         
         if(pid2 != 0){//child
            waitpid(pid2,&status2,0);
            //now execv is finished ?
            //remove_process(child_pid);
            return 0;
         }
         
         //printf("fork pid = %d\n",child_pid);
         res = execv(t[0], t);
         if(res == -1){
            findCommand(t);
            //printf("invalid path\n");
         }
         return 0;
      }
      else{
         //parrent
         add_process(pid);
        // show_running();
        // printf("and flag = %d\n",andFlag); 
         if(andFlag != 1)
            wait(NULL);
         
         remove_process(pid);
         //printf("removing LIST:\n");
        // show_running();
      }
      
      redirectIn("/dev/tty");
      redirectOut("/dev/tty");
      
      //fprintf(stdout, "This shell only supports built-ins. Replace this to run programs as commands.\n");
    }
    // fprintf(stdout, "%d: ", lineNum);
    printf("mercy$ ");
  }
  return 0;
}
