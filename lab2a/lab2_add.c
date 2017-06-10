#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <getopt.h>
#include <string.h>

pthread_t *threadIDs;
int numIterations, numThreads;

long long counter;

pthread_mutex_t m_lock;
int s_lock;

int opt_yield;
char opt_sync;

void acquireSpinLock(int *lock){
  while(__sync_lock_test_and_set(lock, 1) == 1); //spinwait
}
void releaseSpinLock(int *lock){
  __sync_lock_release(lock);;
}

void add(long long *pointer, long long value){
  long long sum = *pointer + value;
  if(opt_yield == 1)
    sched_yield();
  *pointer = sum;
}

void add_m(long long *pointer, long long value){
  pthread_mutex_lock(&m_lock);
  long long sum = *pointer + value;
  if(opt_yield == 1)
    sched_yield();
  *pointer = sum;
  pthread_mutex_unlock(&m_lock);
}

void add_s(long long *pointer, long long value){
  acquireSpinLock(&s_lock);
  long long sum = *pointer + value;
  if(opt_yield == 1)
    sched_yield();
  *pointer = sum;
  releaseSpinLock(&s_lock);
}

void add_c(long long *pointer, long long value){
  long long old = *pointer;
  if(opt_yield == 1)
    sched_yield();
  do{
    old = *pointer;
  } while(__sync_val_compare_and_swap(pointer, old, old+value) != old);
}

void *threadFunction(void* arg){
  void (*addFunc) (long long *, long long) = (void (*) (long long *, long long))arg;
  for(int i = 0; i < numIterations; i++)
    addFunc(&counter, 1);
  for(int i = 0; i < numIterations; i++)
    addFunc(&counter, -1);
}

//clean things up
void exitFunction(){
  free(threadIDs);
}

int main(int argc, char **argv){

  atexit(exitFunction);

  struct timespec start, end;
  void (*addFunc) (long long *, long long) = add;
  numThreads = 1,
  numIterations = 1;
  opt_yield = 0;
  opt_sync = '\0';
  counter = 0;    
  
  //process arguments
  const struct option longopts[] =
    {
      {"threads", required_argument, 0, 't'},
      {"iterations", required_argument, 0, 'i'},
      {"yield", no_argument, 0, 'y'},
      {"sync", required_argument, 0, 's'},
      {0, 0, 0, 0}
    };
  int a;
  while((a = getopt_long(argc, argv, "t:i:ys:", longopts, 0)) >= 0){
    switch(a){
    case 't':
      numThreads = atoi(optarg);
      break;
    case 'i':
      numIterations = atoi(optarg);
      break;
    case 'y':
      opt_yield = 1;
      break;
    case 's':
      opt_sync = optarg[0];

      //initialize appropriate locks & catch bad sync argument
      switch(opt_sync){
      case 'm':
	pthread_mutex_init(&m_lock, NULL);
	addFunc = add_m;
	break;
      case 's':
	s_lock = 0;
	addFunc = add_s;
	break;
      case 'c':
	addFunc = add_c;
	break;
      default:
	fprintf(stderr, "--sync must equal either m, s, or c");
	exit(1);
	break;
      }
      
      break;
    case '?':
      fprintf(stderr, "Unrecognized option\n");
      break;
    default:
      break;
    }
  }
  
  //dynamically allocate array of threadIDs
  threadIDs = (pthread_t *)malloc(numThreads*sizeof(pthread_t));

  //record start time
  if(clock_gettime(CLOCK_MONOTONIC, &start) < 0){
    fprintf(stderr, "Failed to get time");
    exit(1);
  }

  //create all threads
  for(int i = 0; i < numThreads; i++){
    pthread_create(&(threadIDs[i]), NULL, threadFunction, (void*)addFunc);
  }
  
  //wait for threads to finish
  for(int i = 0; i < numThreads; i++){
    pthread_join(threadIDs[i], NULL);
  }

  //record end time
  if(clock_gettime(CLOCK_MONOTONIC, &end) < 0){
    fprintf(stderr, "Failed to get end time");
    exit(1);
  }

  //print info about this run
  int numOps = numThreads * numIterations * 2;
  long long totalRunTime = 1000000000 * ((long long)end.tv_sec - (long long)start.tv_sec)
    + (long long)end.tv_nsec - (long long)start.tv_nsec;
  char *runName;
  if(opt_yield == 1){
    switch(opt_sync){
    case 'm':
      runName = "add-yield-m";
      break;
    case 's':
      runName = "add-yield-s";
      break;
    case 'c':
      runName = "add-yield-c";
      break;
    default:
      runName = "add-yield-none";
      break;
    }
  }
  else{
    switch(opt_sync){
    case 'm':
      runName = "add-m";
      break;
    case 's':
      runName = "add-s";
      break;
    case 'c':
      runName = "add-c";
      break;
    default:
      runName = "add-none";
      break;
    }
  }
  
  fprintf(stdout, "%s,%d,%d,%d,%lld,%d,%lld\n", runName, numThreads, numIterations,
	  numOps, totalRunTime, totalRunTime/numOps, counter);
  
  exit(0);
}
