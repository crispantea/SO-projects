#ifndef EMS_MAIN_H
#define EMS_MAIN_H

#include "constants.h"

#include <stdio.h>
#include <pthread.h>
#include <dirent.h>

#define BARRIER 1

typedef struct {
  int fdRead;
  int fdWrite;
  int *barrierFlag;
  unsigned int *delayWait;
  unsigned int *waitingThread;
  int thread_id;
  int *waitFlags;
  pthread_mutex_t *mutex;
  size_t *xs; 
  size_t *ys;
} ThreadParameters;

int iterateFiles(char* directoryPath);
int process_file(char* pathJobs, char* pathOut);
void* thread_execute(void* args);

#endif