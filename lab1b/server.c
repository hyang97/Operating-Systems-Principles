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

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>

#include <mcrypt.h>

//store the process id for the child process
pid_t childPID = -1;

//need two pipes between server and bash
int toChildPipe[2];
int fromChildPipe[2];

int sockfd, newsockfd;

MCRYPT td;
int encryptFlag;

void *secondThread(void *arg){

  //create signal handler for SIGPIPE
  void handleSIGPIPE(int signum){
    close(sockfd);
    close(newsockfd);
    kill(childPID, SIGKILL);
    exit(2);
  }

  signal(SIGPIPE, handleSIGPIPE);
  
  //read from shell and write to socket
  char c[4096];
  while(true){
    memset(c, 0, 16);
    int charsRead = read(fromChildPipe[0], &(c[16]), sizeof(c)-17);
    if(charsRead > 0){
      if(encryptFlag == 1){ //encrypt!
        mcrypt_generic(td, c, charsRead+16);
      }
      write(STDOUT_FILENO, c, charsRead+16);
    }
    else{ //received EOF or some other error
      close(sockfd);
      close(newsockfd);
      kill(childPID, SIGKILL);
      exit(2);
    }
  }
}

int main(int argc, char **argv){

  //handle arguments
  const struct option longopts[] =
    {
      {"port", required_argument, 0, 'p'},
      {"encrypt", no_argument, 0, 'e'},
      {0, 0, 0, 0}
    };

  //for --port
  int portno = -1;

  //for --encrypt
  encryptFlag = 0;

  int a;
  while((a = getopt_long(argc, argv, "p:e", longopts, 0)) >= 0){
    switch(a){
    case 'p':
      portno = atoi(optarg);
      break;
    case 'e':
      encryptFlag = 1;
      break;
    case '?':
      fprintf(stderr, "Unrecognized argument\n");
      break;
    default:
      break;
    }
  }

  //setup server
  int clilen;
  char buffer[256];
  struct sockaddr_in serv_addr, cli_addr;
  int n;

  //create socket (first call to the socket() function)
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0){
    perror("Error opening socket");
    exit(1);
  }

  //initialize socket structure
  memset((char *)&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  //bind the host address using bind()
  if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
    perror("Error on binding the host address");
    exit(1);
  }

  //start listening for clients (process sleeps and waits for incoming connection)
  listen(sockfd, 5); //who knows what 5 means (maybe number of connections?)
  clilen = sizeof(cli_addr);

  //accept connection from the client
  newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
  if(newsockfd < 0){
    perror("Error on accepting client connection");
    exit(1);
  }

  //initialize encryption module
  char *key;
  char password[20];
  char *IV;
  int keysize=16; //128 bits
  key = calloc(1, keysize);
  int keyfd = open("my.key", O_RDONLY);
  if(keyfd < 0){
    fprintf(stderr, "Failed to open my.key");
    exit(1);
  }

  int rsize = read(keyfd, password, 20*sizeof(char));
  password[rsize] = 0;
  memmove(key, password, strlen(password));
  close(keyfd);

  td = mcrypt_module_open("twofish", NULL, "cfb", NULL);
  if(td == MCRYPT_FAILED){
    exit(1);
  }
  
  IV = malloc(mcrypt_enc_get_iv_size(td));

  srand(1919); //use the same seed value as client

  for(int i = 0; i < mcrypt_enc_get_iv_size(td); i++){
    IV[i] = rand();
  }

  int k = mcrypt_generic_init(td, key, keysize, IV);
  if (k<0) {
     mcrypt_perror(k);
     exit(1);
  }

  //accepted client connection, start doing work

  if(pipe(toChildPipe) == -1){
    fprintf(stderr, "pipe() failed!\n");
    exit(1);
  }

  if(pipe(fromChildPipe) == -1){
    fprintf(stderr, "pipe() failed!\n");
    exit(1);
  }

  close(STDIN_FILENO);
  dup2(newsockfd, STDIN_FILENO);
  close(STDOUT_FILENO);
  dup2(newsockfd, STDOUT_FILENO);
  close(STDERR_FILENO);
  dup2(newsockfd, STDERR_FILENO);
  
  childPID = fork();

  if(childPID > 0){ //parent process
    close(toChildPipe[0]);
    close(fromChildPipe[1]);

    pthread_t thread2id;
    pthread_create(&thread2id, NULL, secondThread, NULL);
    
    char c[4096];
    while(true){
      int charsRead = read(STDIN_FILENO, c, sizeof(c)-1);
      if(charsRead > 0){
	if(encryptFlag == 1){ //decrypt!
	  mdecrypt_generic(td, c, charsRead*sizeof(char));
	}
	//write characters read from socket to shell
	write(toChildPipe[1], c, charsRead);
      }
      else{ //received read error or EOF
	close(sockfd);
	close(newsockfd);
	kill(childPID, SIGKILL);
	exit(1);
      }
    }
  }
  else if(childPID == 0){ //child process
    close(toChildPipe[1]);
    close(fromChildPipe[0]);
    dup2(toChildPipe[0], STDIN_FILENO);
    dup2(fromChildPipe[1], STDOUT_FILENO);
    close(toChildPipe[0]);
    close(fromChildPipe[1]);

    char *execvp_argv[2];
    char execvp_filename[] = "/bin/bash";
    execvp_argv[0] = execvp_filename;
    execvp_argv[1] = NULL;
    execvp(execvp_filename, execvp_argv);
  }
  else{ //fork failed
    fprintf(stderr, "fork() failed!\n");
    exit(1);
  }
  
  return 0;
}
