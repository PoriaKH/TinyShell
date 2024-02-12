#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>


#define INPUT_STRING_SIZE 80

#include "../io/io.h"
#include "../parser/parser.h"
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
int cmd_help(tok_t arg[]) {     // for "?" command
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
}

/**
 * Add a process to our process list
 */
void add_process(pid_t pid){
  
  running_pids[count] = pid;
  count++;
  signal_counter++;
  
  tcsetpgrp(shell_terminal, pid);
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
}

void show_running (){       //shows running PIDs.
   for(int i = 0; i < count; i++)
      printf("PID = %d\n",running_pids[i]);
   printf("\n");
}

/**
 * Creates a process given the inputString from stdin
 */

void findCommand(tok_t *t){     //find command in the system
   char* PATH = getenv("PATH");

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

void redirectIn(char *fileName)   //for "<" annotation
{
    int in = open(fileName, O_RDONLY);
    dup2(in, 0);
    close(in);
}

void redirectOut(char *fileName)  //for ">" annotation
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
   bg_pids[bg_counter - 1] = (pid_t) NULL;
   bg_counter--;
}
void sig_handler(int sig){      // catching signals
   switch(sig) {
      case SIGTSTP: //CTRL Z
         ctrlz_func();
         break;
      case SIGINT:
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
   
}

void handle_signals(){
    signal(SIGINT, sig_handler);
    signal(SIGTSTP, sig_handler);
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
  add_process(shell_pid);

   
  lineNum=0;
  handle_signals();
  printf("PoriaKH$ ");
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
      int res = 1;
      
      pid = fork();

      if(pid == 0){
         pid_t child_pid = getpid();

         int status2;
         pid_t pid2 = fork();
         
         if(pid2 != 0){//child
            waitpid(pid2,&status2,0);
            return 0;
         }
         
         res = execv(t[0], t);
         if(res == -1){
            findCommand(t);
         }
         return 0;
      }
      else{
         add_process(pid);
         if(andFlag != 1)
            wait(NULL);
         
         remove_process(pid);
      }
      
      redirectIn("/dev/tty");
      redirectOut("/dev/tty");
      
    }
    printf("PoriaKH$ ");
  }
  return 0;
}
