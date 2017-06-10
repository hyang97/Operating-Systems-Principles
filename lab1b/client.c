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
int sockfd;
int logfd = -1;

struct termios oldtio;

char cr = 13;
char lf = 10;
char *sent = "SENT 1 byte: ";
char *received = "RECEIVED ";
char *bytes = " bytes: ";

MCRYPT td;
int encryptFlag;

void exitFunction(){
  tcsetattr(STDIN_FILENO, TCSANOW, &oldtio);
}

void *secondThread(void *arg){
  //this thread will read from the socket
  char buf[4096];
  while(true){
    int charsRead = read(sockfd, buf, sizeof(buf)-1);
    if(charsRead > 0){

      
      //char temp[4096];
      //strcpy(temp, buf);
      
      if(logfd >= 0){
	write(logfd, received, strlen(received));
	char *num;
	sprintf(num, "%d\0", charsRead-16);
	write(logfd, num, strlen(num));
	write(logfd, bytes, strlen(bytes));
	write(logfd, &(buf[16]), charsRead-16);
	write(logfd, &lf, sizeof(char));
      }

      if(encryptFlag == 1){ //decrypt input
	mdecrypt_generic(td, buf, charsRead*sizeof(char));
      }

      write(STDOUT_FILENO, &(buf[16]), charsRead-16);
    }
    else{ //error reading from socket or encountered EOF
      close(sockfd);
      exit(1);
    }
  }
}

int main(int argc, char **argv){

  //handle arguments
  const struct option longopts[] =
    {
      {"port", required_argument, 0, 'p'},
      {"log", required_argument, 0, 'l'},
      {"encrypt", no_argument, 0, 'e'},
      {0, 0, 0, 0}
    };

  //for --port
  int portno = -1;
  
  //for --log
  char *logFile = "";

  //for --encrypt
  encryptFlag = 0;
  
  int a;
  while((a = getopt_long(argc, argv, "p:l:e", longopts,0)) >= 0){
    switch(a){
    case 'p':
      portno = atoi(optarg);
      break;
    case 'l':
      logFile = optarg;
      break;
    case 'e':
      encryptFlag = 1;
      break;
    case '?':
      fprintf(stderr, "Unrecognized option\n");
      break;
    default:
      break;
    }
  }

  //setup client
  int n;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  //create socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0){
    perror("Error opening socket");
    exit(1);
  }

  //use localhost since client and server run on same machine
  server = gethostbyname("localhost");    
  if(server == NULL){
    fprintf(stderr, "Error, no such host\n");
    exit(0);
  }

  //initialize socket structure
  memset((char *)&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
  serv_addr.sin_port = htons(portno);

  //connect to server
  if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
    perror("Error connecting, possibly bad portno");
    exit(1);
  }

  
  char buffer[256];
  
  struct termios newtio;
  
  if(strlen(logFile) != 0){
    logfd = creat(logFile, 0666);
    if(logfd < 0){
      fprintf(stderr, "Error creating logfile");
      exit(1);
    }
  }

  //check that stdin is a proper terminal
  if(isatty(STDIN_FILENO) == false){
    fprintf(stderr, "STDIN is not a terminal");
    exit(1);
  }

  //save old terminal settings
  tcgetattr(STDIN_FILENO, &oldtio);
  tcgetattr(STDIN_FILENO, &newtio);

  //restore old terminal settings and free memory on exit
  atexit(exitFunction);

  //set to noncanonical mode and turn off echo
  newtio.c_lflag &= ~(ICANON|ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newtio);

  //create second thread to read from socket
  pthread_t thread2id;
  pthread_create(&thread2id, NULL, secondThread, NULL);

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

  srand(1919); //use the same seed value for server as well

  for(int i = 0; i < mcrypt_enc_get_iv_size(td); i++){
    IV[i] = rand();
  }

  int k = mcrypt_generic_init(td, key, keysize, IV);
  if (k<0) {
     mcrypt_perror(k);
     exit(1);
  }

  //current thread will echo to stdout and write to socket
  while(true){
    int charsRead = read(STDIN_FILENO, buffer, sizeof(buffer)-1);
    if(charsRead > 0){
      for(int i = 0; i < charsRead; i++){
	if(buffer[i] == 3){ //received ^C
	  //do nothing
	}
	else if(buffer[i] == 4){ //received ^D
	  close(sockfd);
	  exit(0);
	}
	else if(buffer[i] == 10 || buffer[i] == 13){ //received a cr or lf
	  //echo out cr and lf
	  write(STDOUT_FILENO, &cr, sizeof(char));
	  write(STDOUT_FILENO, &lf, sizeof(char));

	  if(encryptFlag == 1){ //encrypt!
	    char encryptedLF = 10;
	    mcrypt_generic(td, &encryptedLF, sizeof(char));
	    mcrypt_generic(td, &(buffer[i]), sizeof(char));
	    
	    //send lf to socket
	    write(sockfd, &encryptedLF, sizeof(char));

	    //write the character into log file (if it exists)
	    if(logfd >= 0){
	      write(logfd, sent, strlen(sent));
	      write(logfd, &(buffer[i]), sizeof(char));
	      write(logfd, &lf, sizeof(char));
	    }
	  }
	  else{
	    //send lf to socket
	    write(sockfd, &lf, sizeof(char));

	    //write the character into log file (if it exists)
	    if(logfd >= 0){
	      write(logfd, sent, strlen(sent));
	      write(logfd, &(buffer[i]), sizeof(char));
	      write(logfd, &lf, sizeof(char));
	    }
	  }
	}
	else{ //received normal character
	  //write character to socket
	  write(STDOUT_FILENO, &(buffer[i]), sizeof(char));

	  if(encryptFlag == 1){ //encrypt!
	    mcrypt_generic(td, &(buffer[i]), sizeof(char));
	  }
	  write(sockfd, &(buffer[i]), sizeof(char));

	  if(logfd >= 0){
	    write(logfd, sent, strlen(sent));
	    write(logfd, &(buffer[i]), sizeof(char));
	    write(logfd, &lf, sizeof(char));
	  }
	  
	}
      }
    }
  }
  return 0;
}

