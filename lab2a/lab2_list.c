#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <getopt.h>
#include <string.h>

#include "SortedList.h"

int numThreads, numIterations;
int opt_yield;
char *yieldopts;
char *syncopts;
int syncMutex = 0;
int syncSpin = 0;
pthread_mutex_t lock;
int spinlock;

char **keys;
SortedListElement_t *elements;
SortedList_t list;

pthread_t *threadIDs;

void exitFunction(){
  for(int i = 0; i < numThreads*numIterations; i++)
    free(keys[i]);
  free(keys);
  free(threadIDs);
}

void generateKeys(){
  char charset[] = "0123456789"
                   "abcdefghijklmnopqrstuvwxyz"
                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  keys = (char **)malloc(numThreads*numIterations*sizeof(char *));
  for(int i = 0; i < numThreads*numIterations; i++){
    keys[i] = (char *)malloc(4*sizeof(char));
    for(int j = 0; j < 3; j++){
      int randIndex = rand() * (sizeof(charset)-1) / RAND_MAX;
      keys[i][j] = charset[randIndex];
    }
    keys[i][3] = 0;
  }
}

void initializeList(){
  elements = (SortedListElement_t *)malloc(numThreads*numIterations*sizeof(SortedListElement_t));
  for(int i = 0; i < numThreads*numIterations; i++){
    elements[i].key = keys[i];
  }
  list.key = NULL;
  list.next = NULL;
  list.prev = NULL;
}

void *threadFunction(void *args){
  int startIndex = *((int *)args)*numIterations;
  int endIndex = startIndex + numIterations;

  //fprintf(stdout, "startIndex:%d, endIndex:%d\n", startIndex, endIndex);
  
  //insert elements into linked list
  for(int i = startIndex; i < endIndex; i++){
    if(syncMutex == 1){
      pthread_mutex_lock(&lock);
      SortedList_insert(&list, &(elements[i]));
      pthread_mutex_unlock(&lock);
    }
    else if(syncSpin == 1){
      while(__sync_lock_test_and_set(&spinlock, 1) == 1);
      SortedList_insert(&list, &(elements[i]));
      __sync_lock_release(&spinlock);
    }
    else
      SortedList_insert(&list, &(elements[i]));
  }

  //get the length of the linked list
  int length = 0;
  if(syncMutex == 1){
    pthread_mutex_lock(&lock);
    length = SortedList_length(&list);
    pthread_mutex_unlock(&lock);
  }
  else if(syncSpin == 1){
    while(__sync_lock_test_and_set(&spinlock, 1) == 1);
    length = SortedList_length(&list);
    __sync_lock_release(&spinlock);
  }
  else
    length = SortedList_length(&list);
  if(length < 0)
    exit(10);

  //remove all elements from linked list
  for(int i = startIndex; i < endIndex; i++){
    if(syncMutex == 1){
      pthread_mutex_lock(&lock);
      SortedListElement_t *temp = SortedList_lookup(&list, elements[i].key);
      if(temp == NULL)
        exit(11);
      if(SortedList_delete(temp) == 1)
        exit(1);
      pthread_mutex_unlock(&lock);
    }
    else if(syncSpin == 1){
      while(__sync_lock_test_and_set(&spinlock, 1) == 1);
      SortedListElement_t *temp = SortedList_lookup(&list, elements[i].key);
      if(temp == NULL)
        exit(13);
      if(SortedList_delete(temp) == 1)
        exit(14);
      __sync_lock_release(&spinlock);
    }
    else{
      SortedListElement_t *temp = SortedList_lookup(&list, elements[i].key);
      if(temp == NULL)
	exit(15);
      if(SortedList_delete(temp) == 1)
	exit(16);
    }
  }

  //fprintf(stdout, "help!");

  return (void*)(long)length;
}

int main(int argc, char **argv){

  atexit(exitFunction);

  struct timespec start, end;
  numThreads = 1;
  numIterations = 1;
  syncopts = "none";
  yieldopts = "none";

  //process arguments
  const struct option longopts[] =
    {
      {"threads", required_argument, 0, 't'},
      {"iterations", required_argument, 0, 'i'},
      {"yield", required_argument, 0, 'y'},
      {"sync", required_argument, 0, 's'},
      {0, 0, 0, 0,}
    };
  int a;
  while((a = getopt_long(argc, argv, "t:i:y:s:", longopts, 0)) >= 0){
    switch(a){
    case 't':
      numThreads = atoi(optarg);
      break;
    case 'i':
      numIterations = atoi(optarg);
      break;
    case 'y':
      yieldopts = optarg;
      opt_yield = -1;
      if(strchr(optarg, 'i') != NULL)
	opt_yield &= INSERT_YIELD;
      if(strchr(optarg, 'd') != NULL)
	opt_yield &= DELETE_YIELD;
      if(strchr(optarg, 'l') != NULL)
	opt_yield &= LOOKUP_YIELD;
      break;
    case 's':
      syncopts = optarg;
      if(strchr(syncopts, 'm') != NULL){
	syncMutex = 1;
	pthread_mutex_init(&lock, NULL);
      }
      else if(strchr(syncopts, 's') != NULL){
	syncSpin = 1;
	spinlock = 0;
      }
      else{
	fprintf(stderr, "Bad argument for --sync");
	exit(1);
      }
      break;
    case '?':
      fprintf(stderr, "Unrecognized option\n");
      break;
    default:
      break;
    }
  }
  
  //initialize random keys -- stored in char ** keys
  generateKeys();

  //initialize list elements and list head
  initializeList();

  //dynamically allocate array of threadIDs
  threadIDs = (pthread_t *)malloc(numThreads*sizeof(pthread_t));

  //record start time
  if(clock_gettime(CLOCK_MONOTONIC, &start) < 0){
    fprintf(stderr, "Failed to get time");
    exit(1);
  }

  //create all threads
  int threadNum[numThreads];
  for(int i = 0; i < numThreads; i++){
    threadNum[i] = i;
    pthread_create(&(threadIDs[i]), NULL, threadFunction, (void *)(&threadNum[i]));
    //fprintf(stdout, "thread number: %d\n", threadIDs[i]);
  }
    
  //wait for threads to finish
  void **retval = NULL;
  for(int i = 0; i < numThreads; i++){
    //fprintf(stdout, "join thread number: %d\n", threadIDs[i]);
    if(pthread_join(threadIDs[i], retval) != 0)
      printf("sos");
  }

  //record end time
  if(clock_gettime(CLOCK_MONOTONIC, &end) < 0){
    fprintf(stderr, "Failed to get time");
    exit(1);
  }
  
  //print out data
  int numOps = numThreads*numIterations*3;
  long long totalRunTime = 1000000000 * ((long long)end.tv_sec - (long long)start.tv_sec)
    + (long long)end.tv_nsec - (long long)start.tv_nsec;

  fprintf(stdout, "list-%s-%s,%d,%d,%d,%d,%lld,%d\n", yieldopts, syncopts, numThreads,
	  numIterations, 1, numOps, totalRunTime, totalRunTime/numOps);

  if(SortedList_length(&list) != 0)
    exit(SortedList_length(&list));

  exit(0);
}
