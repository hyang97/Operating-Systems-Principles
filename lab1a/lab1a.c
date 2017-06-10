#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/wait.h>

//store old terminal settings
struct termios oldtio; 

//store the process id for the child process
pid_t child_pid = -1;

//need two pipes as a pipe is unidirectional                                                                      
//parent to child running bash
int to_child_pipe[2];
//child running bash to parent
int from_child_pipe[2];

void waitForChildProcess(){
  int child_status;
  waitpid(child_pid, &child_status, 0);
  printf("shell exit status: %d\n", WEXITSTATUS(child_status));
}

void reset_terminal_settings(){
  //STDIN_FILENO gives the file descriptor (stdin gives a FILE*)
  //TCSANOW makes the change instantaneous 
  tcsetattr(STDIN_FILENO, TCSANOW, &oldtio);
}

void *secondThread(void *arg){

  //create signal handler for SIGPIPE
  void handleSIGPIPE(int signum){
    waitForChildProcess();
    exit(1);
  }

  signal(SIGPIPE, handleSIGPIPE);
  
  //read input from shell pipe, write it to stdout
  char buf[65];
  while(true){
    int charsRead = read(from_child_pipe[0], buf, sizeof(buf)-1);
    if(charsRead > 0){
      for(int i = 0; i < charsRead; i++){
	if(buf[i] == 4){ //received ^D (EOF)
	  waitForChildProcess();
	  exit(1);
	}
	else{
	  write(STDOUT_FILENO, &(buf[i]), sizeof(char));
	}
      }
    }
  }
}

int main(int argc, char **argv){

  int shellFlag = 0;
  const struct option longopts[] =
    {
      {"shell", no_argument, 0, 's'},
      {0, 0, 0, 0}
    };

  int a;
  while((a = getopt_long(argc, argv, "s", longopts, 0)) >= 0){
    switch(a){
    case 's':
      shellFlag = 1;
      break;
    case '?':
      fprintf(stderr, "Try using --shell instead!\n");
      break;
    default:
      break;
    }
  }
  
  struct termios newtio;
  
  //make sure stdin is a terminal (unsure if this check is strictly necessary)
  if(isatty(STDIN_FILENO) == false){
    fprintf(stderr, "Stdin is not a terminal");
    exit(1);
  }

  //save old settings
  tcgetattr(STDIN_FILENO, &oldtio);

  //restore old terminal settings on exit
  atexit(reset_terminal_settings);

  tcgetattr(STDIN_FILENO, &newtio);
  //set to noncanonical mode and turn off echo
  newtio.c_lflag &= ~(ICANON|ECHO); //preserves the other setting while turning
				    //off specific bits
  //newtio.c_oflag &= (ONLCR|OCRNL); //translates CR to NL and NL to CR-NL
  
  tcsetattr(STDIN_FILENO, TCSANOW, &newtio);

  if(shellFlag == 0){
    //continuously reading and writing characters until receive ^D
    char c[65];
    char cr = 13;
    char lf = 10;
    while(true){
      int charsRead = read(STDIN_FILENO, c, sizeof(c)-1);
      if(charsRead > 0){
        for(int i = 0; i < charsRead; i++){
          if(c[i] == 4){
            exit(0);
          }
          else if(c[i] == 10 || c[i] == 13){
            write(STDOUT_FILENO, &cr, sizeof(char));
            write(STDOUT_FILENO, &lf, sizeof(char));
          }
          else{
            write(STDOUT_FILENO, &(c[i]), sizeof(char));
          }
        }
      }
    }
  }
  else{ //--shell option

    if(pipe(to_child_pipe) == -1){
      fprintf(stderr, "pipe() failed!\n");
      exit(1);
    }

    if(pipe(from_child_pipe) == -1){
      fprintf(stderr, "pipe() failed!\n");
      exit(1);
    }

    child_pid = fork();

    if(child_pid > 0){ // parent process
      close(to_child_pipe[0]);
      close(from_child_pipe[1]);

      pthread_t thread1id, thread2id;
      //pthread_create(&thread1id, NULL, firstThread, NULL);
      pthread_create(&thread2id, NULL, secondThread, NULL);

      //read input from keyboard, echo to stdout, forward to shell
      //echo to stdout only necessary because of noncanonical no-echo mode
      char c[65];
      char cr = 13;
      char lf = 10;
      while(true){
        int charsRead = read(STDIN_FILENO, c, sizeof(c)-1);
        if(charsRead > 0){
          for(int i = 0; i < charsRead; i++){
            if(c[i] == 3){ //received ^C
              //send SIGINT to shell
              kill(child_pid, SIGINT);
            }
            else if(c[i] == 4){ //received ^D
              //close pipe
              close(to_child_pipe[1]);
              close(from_child_pipe[0]);
              //send SIGHUP to shell
              kill(child_pid, SIGHUP);
	      waitForChildProcess();
	      exit(0);
            }
            else if(c[i] == 10 || c[i] == 13){ //received a cr or lf
              write(STDOUT_FILENO, &cr, sizeof(char));
              write(STDOUT_FILENO, &lf, sizeof(char));
              write(to_child_pipe[1], &lf, sizeof(char)); //write to shell                                           
            }
            else{
              write(STDOUT_FILENO, &(c[i]), sizeof(char));
              write(to_child_pipe[1], &(c[i]), sizeof(char)); //write to shell                                       
            }
          }
        }
      }
    }
    else if(child_pid == 0){ //child process

      close(to_child_pipe[1]);
      close(from_child_pipe[0]);
      dup2(to_child_pipe[0], STDIN_FILENO);
      dup2(from_child_pipe[1], STDOUT_FILENO);
      close(to_child_pipe[0]);
      close(from_child_pipe[1]);

      char *execvp_argv[2];
      char execvp_filename[] = "/bin/bash";
      execvp_argv[0] = execvp_filename;
      execvp_argv[1] = NULL;
      execvp(execvp_filename, execvp_argv);
      /*
      if(execvp(execvp_filename, execvp_argv) == -1){
	fprintf(stderr, "execvp() failed!\n");
	exit(1);
      }
      */
    }
    else{ //fork failed
      fprintf(stderr, "fork() failed!\n");
      exit(1);
    }
  }  
  exit(0);
}
