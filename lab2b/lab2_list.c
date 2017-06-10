#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <getopt.h>
#include <string.h>

#include "SortedList.h"

int numThreads, numIterations, numLists;
int opt_yield;
char *yieldopts;
char *syncopts;
int syncMutex = 0;
int syncSpin = 0;
pthread_mutex_t *lock;
int *spinlock;

char **keys;
SortedListElement_t *elements;
SortedList_t *list;

pthread_t *threadIDs;

typedef struct{
  int calls;
  long long time;
} mutex_wait;

mutex_wait *m_timings;

int hash(char *key){
  return key[0] + key[1]/2 + key[2]/3;
}

void exitFunction(){
  for(int i = 0; i < numThreads*numIterations; i++)
    free(keys[i]);
  free(keys);
  free(threadIDs);
  free(m_timings);
  free(list);
  if(syncMutex == 1)
    free(lock);
  else if(syncMutex == 1)
    free(spinlock);
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

  list = (SortedList_t *)malloc(numLists*sizeof(SortedList_t));
  for(int i = 0; i < numLists; i++){
    list[i].key = NULL;
    list[i].next = NULL;
    list[i].prev = NULL;
  }
}


void initializeLocks(){
  if(syncMutex == 1){
    lock = (pthread_mutex_t *)malloc(numLists*sizeof(pthread_mutex_t));
    for(int i = 0; i < numLists; i++){
      pthread_mutex_init(&(lock[i]), NULL);
    }
  }
  else if(syncSpin == 1){
    spinlock = (int *)malloc(numLists*sizeof(int));
    for(int i = 0; i < numLists; i++){
      spinlock[i] = 0;
    }
  }
}



void *threadFunction(void *args){
  int startIndex = *((int *)args)*numIterations;
  int endIndex = startIndex + numIterations;

  int threadNum = *((int *)args);
  m_timings[threadNum].calls = 0;
  m_timings[threadNum].time = 0;

  struct timespec start, end;

  // -------------------- INSERTION -----------------------
  //insert elements into linked list
  for(int i = startIndex; i < endIndex; i++){

    int listNum = hash(keys[i]) % numLists;
    
    if(syncMutex == 1){

      //attempt to acquire lock and record acquisition time
      clock_gettime(CLOCK_MONOTONIC, &start);
      pthread_mutex_lock(&(lock[listNum])); //acquire lock
      clock_gettime(CLOCK_MONOTONIC, &end);
      m_timings[threadNum].calls++;
      m_timings[threadNum].time += 1000000000 * ((long long)end.tv_sec - (long long)start.tv_sec)
	+ (long long)end.tv_nsec - (long long)start.tv_nsec;
      
      SortedList_insert(&(list[listNum]), &(elements[i]));
      pthread_mutex_unlock(&(lock[listNum]));
    }
    else if(syncSpin == 1){
      while(__sync_lock_test_and_set(&(spinlock[listNum]), 1) == 1);
      SortedList_insert(&(list[listNum]), &(elements[i]));
      __sync_lock_release(&(spinlock[listNum]));
    }
    else
      SortedList_insert(&(list[listNum]), &(elements[i]));
  }
  // ----------------------------------------------------


  
  // --------------------- LENGTH -----------------------
  //get the length of the linked list
  int length = 0;
  if(syncMutex == 1){

    //attempt to acquire lock and record acquisition time
    clock_gettime(CLOCK_MONOTONIC, &start);

    //acquire ALL locks
    for(int i = 0; i < numLists; i++)
      pthread_mutex_lock(&(lock[i])); //acquire lock
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    m_timings[threadNum].calls += numLists;
    m_timings[threadNum].time += 1000000000 * ((long long)end.tv_sec - (long long)start.tv_sec)
      + (long long)end.tv_nsec - (long long)start.tv_nsec;

    //sum lengths of all sublists
    for(int i = 0; i < numLists; i++)
      length += SortedList_length(&(list[i]));

    //release ALL locks
    for(int i = 0; i < numLists; i++)
      pthread_mutex_unlock(&(lock[i]));
  }
  else if(syncSpin == 1){
    
    //acquire ALL locks
    for(int i = 0; i < numLists; i++)
      while(__sync_lock_test_and_set(&(spinlock[i]), 1) == 1);

    //sum lengths of all sublists
    for(int i = 0; i < numLists; i++)
      length += SortedList_length(&(list[i]));

    //release ALL locks
    for(int i = 0; i < numLists; i++)
      __sync_lock_release(&(spinlock[i]));
    
  }
  else{
    for(int i = 0; i < numLists; i++)
      length += SortedList_length(&(list[i]));
  }
  if(length < 0)
    exit(10);
  // ----------------------------------------------------


  
  // ------------------- DELETION -----------------------
  //remove all elements from linked list
  for(int i = startIndex; i < endIndex; i++){

    int listNum = hash(keys[i]) % numLists;
    
    if(syncMutex == 1){

      //attempt to acquire lock and record acquisition time
      clock_gettime(CLOCK_MONOTONIC, &start);
      pthread_mutex_lock(&(lock[listNum])); //acquire lock
      clock_gettime(CLOCK_MONOTONIC, &end);
      m_timings[threadNum].calls++;
      m_timings[threadNum].time += 1000000000 * ((long long)end.tv_sec - (long long)start.tv_sec)
        + (long long)end.tv_nsec - (long long)start.tv_nsec;

      SortedListElement_t *temp = SortedList_lookup(&(list[listNum]), elements[i].key);
      if(temp == NULL)
        exit(11);
      if(SortedList_delete(temp) == 1)
        exit(1);
      pthread_mutex_unlock(&(lock[listNum]));
    }
    else if(syncSpin == 1){
      while(__sync_lock_test_and_set(&(spinlock[listNum]), 1) == 1);
      SortedListElement_t *temp = SortedList_lookup(&(list[listNum]), elements[i].key);
      if(temp == NULL)
        exit(13);
      if(SortedList_delete(temp) == 1)
        exit(14);
      __sync_lock_release(&(spinlock[listNum]));
    }
    else{
      SortedListElement_t *temp = SortedList_lookup(&(list[listNum]), elements[i].key);
      if(temp == NULL)
	exit(15);
      if(SortedList_delete(temp) == 1)
	exit(16);
    }
  }

  return (void*)(long)length;
}

int main(int argc, char **argv){

  atexit(exitFunction);

  struct timespec start, end;
  numThreads = 1;
  numIterations = 1;
  numLists = 1;
  syncopts = "none";
  yieldopts = "none";

  //process arguments
  const struct option longopts[] =
    {
      {"threads", required_argument, 0, 't'},
      {"iterations", required_argument, 0, 'i'},
      {"yield", required_argument, 0, 'y'},
      {"sync", required_argument, 0, 's'},
      {"lists", required_argument, 0, 'l'},
      {0, 0, 0, 0,}
    };
  int a;
  while((a = getopt_long(argc, argv, "t:i:y:s:l:", longopts, 0)) >= 0){
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
	//pthread_mutex_init(&lock, NULL);
      }
      else if(strchr(syncopts, 's') != NULL){
	syncSpin = 1;
	//spinlock = 0;
      }
      else{
	fprintf(stderr, "Bad argument for --sync");
	exit(1);
      }
      break;
    case 'l':
      numLists = atoi(optarg);
      break;
    case '?':
      fprintf(stderr, "Unrecognized option\n");
      break;
    default:
      break;
    }
  }

  //initialize locks
  initializeLocks();
  
  //initialize random keys -- stored in char ** keys
  generateKeys();

  //initialize list elements and list head
  //also initializes each of the sublists
  initializeList();

  //dynamically allocate array of threadIDs
  threadIDs = (pthread_t *)malloc(numThreads*sizeof(pthread_t));

  //dynamically allocate array for keeping track of mutex wait timings
  m_timings = (mutex_wait *)malloc(numThreads*sizeof(mutex_wait));
  
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

  int endLength = 0;
  for(int i = 0; i < numLists; i++)
    endLength += SortedList_length(&(list[i]));

  if(endLength != 0)
    exit(endLength);
  
  //print out data
  int numOps = numThreads*numIterations*3;
  long long totalRunTime = 1000000000 * ((long long)end.tv_sec - (long long)start.tv_sec)
    + (long long)end.tv_nsec - (long long)start.tv_nsec;
  long long mutex_time_total = 0;
  int mutex_calls_total = 0;
  
  if(syncMutex == 1){
    for(int i = 0; i < numThreads; i++){
      mutex_time_total += m_timings[i].time;
      mutex_calls_total += m_timings[i].calls;
    }
    
    fprintf(stdout, "list-%s-%s,%d,%d,%d,%d,%lld,%d,%d\n", yieldopts, syncopts, numThreads,
	    numIterations, numLists, numOps, totalRunTime, totalRunTime/numOps,
	    mutex_time_total/mutex_calls_total);
  }
  else{
    fprintf(stdout, "list-%s-%s,%d,%d,%d,%d,%lld,%d\n", yieldopts, syncopts, numThreads,
	    numIterations, numLists, numOps, totalRunTime, totalRunTime/numOps);
  }
  exit(0);
}
