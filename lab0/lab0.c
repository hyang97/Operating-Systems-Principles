#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

//argc is argument count, argv is a pointer to an array of arguments provided
int main(int argc, char *argv[]){

  //use getopt_long
  //int getopt_long(int argc, char * const argv[], const char *optstring,
  //                const struct option *longopts, int *longindex); 

  int segfaultFlag = 0;
  int catchFlag = 0;

  char *inputFile = "";
  char *outputFile = "";
  
  //char *optstring = ""; //since all options are long
  const struct option longopts[] =
    {
      {"input", required_argument, 0, 'i'},
      {"output", required_argument, 0, 'o'},
      {"segfault", no_argument, 0, 's'},
      {"catch", no_argument, 0, 'c'},
      {0, 0, 0, 0}
    };

  int c;
  while((c = getopt_long(argc, argv, "i:o:sc", longopts, 0)) >= 0){
    switch(c){
    case 'i':
      inputFile = optarg;
      break;
    case 'o':
      outputFile = optarg;
      break;
    case 's':
      segfaultFlag = 1;
      break;
    case 'c':
      catchFlag = 1;
      break;
    case '?':
      fprintf(stderr, "Unrecognized option\n");
      break;
    default:
      //perror("Invalid argument");
      //_exit(4);
      break;
    }
  }

  if(catchFlag == 1){
    typedef void (*sighandler_t)(int);
    sighandler_t handler;
    void segHandler(int signo){
      if(signo == SIGSEGV){
        fprintf(stderr, "Caught segmentation fault!\n");
        _exit(3);
      }
    }
    //set segmentation fault to be caught by handler                                                                  
    if(signal(SIGSEGV, segHandler) == SIG_ERR){
      fprintf(stderr, "Unable to catch segfault\n");
    }
  }

  if(segfaultFlag == 1){
    char *badptr = NULL;
    *badptr = 1; // should force segmentation fault                                                                  
  }

  
  //input file provided, make the fd for the file 0, replacing stdin
  if(strlen(inputFile) != 0){
    int inputfd = open(inputFile, O_RDONLY);
    if(inputfd >= 0) {
      close(0); // closes the 0 fd, which referenced stdin
      dup(inputfd); //creates another fd to point at the inputfile, which is 0 in this case
      close(inputfd); //no longer need inputfd since 0 now points to inputfile
    }
    else{
      //unable to open inputfile
      fprintf(stderr, "Failed to open %s\n", inputFile);
      perror("Error");
      _exit(1);
    }
  }

  //output file provided, make the fd for the file 1, replacing stdout
  if(strlen(outputFile) != 0){
    int outputfd = creat(outputFile, 0666); //0666 gives everyone read and write permissions
    if(outputfd >= 0) {
      close(1);
      dup(outputfd);
      close(outputfd);
    }
    else {
      //unable to open/create output file
      fprintf(stderr, "Failed to open/create %s\n", outputFile);
      perror("Error");
      _exit(2);
    }
  }
  
  //read everything from input to buffer
  char buf[513];
  int charsRead;
  //sizeof is ok to use because chars are 1 byte each
  //-1 to account for terminating byte
  while((charsRead = read(0, buf, sizeof(buf) - 1)) > 0){
    //fprintf(stdout, "chars read: %d\n", charsRead); //debugging
    int charsWritten = write(1, buf, charsRead);
    if(charsWritten == -1){
      //error writing to file
      fprintf(stderr, "Error writing %s\n", outputFile);
      perror("Error");
      _exit(2);
    }
  }
  if(charsRead == -1){
    //error reading file
    fprintf(stderr, "Error reading %s\n", inputFile);
    perror("Error");
    _exit(1);
  }

   _exit(0);
}
