#include "operations.h"
#include "parser.h"
#include "main.h"
#include "auxFunctions.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <pthread.h>

#define ERROR -1

int global_num_proc = 0;
int global_num_threads = 0;


/* Main function that processes the arguments and calls the functions that process the files */
int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

  //If the number of arguments is different from 4 or 5, the input is invalid
  if (argc != 4 && argc != 5) {
    fprintf(stderr, "Invalid arguments. See HELP for usage\n");
    return 1;
  }

  //If the number of processes is invalid, the input is invalid
  char *endptr_proc;
  long int num_proc = strtol(argv[2], &endptr_proc, 10);
  if (*endptr_proc != '\0' || num_proc > INT_MAX || num_proc < 1) {
    fprintf(stderr, "Invalid processes value or value too large\n");
    return 1;
  }
  global_num_proc = (int)num_proc;

  //If the number of threads is invalid, the input is invalid
  char *endptr_threads;
  long int num_threads = strtol(argv[3], &endptr_threads, 10);
  if (*endptr_threads != '\0' || num_threads > INT_MAX || num_threads < 1) {
    fprintf(stderr, "Invalid threads value or value too large\n");
    return 1;
  }
  global_num_threads = (int)num_threads;

  // If there is a fifth argument, the delay value is assigned
  if (argc == 5) {
    char *endptr;
    unsigned long int delay = strtoul(argv[4], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  iterateFiles(argv[1]);

  return 0;
}

/* Function that iterates over the files of a directory */
int iterateFiles(char* directoryPath) {
  DIR *dir;
  struct dirent *entry;

  // Open directory
  if ((dir = opendir(directoryPath)) == NULL) {
      perror("Error opening directory");
      return ERROR;
  }

  int activeProcesses = 0;
  int status;

  // Iterate over the files of the directory
  while ((entry = readdir(dir)) != NULL) {
    if (strstr(entry->d_name, ".job") != NULL) {
      if (activeProcesses == global_num_proc) {
        if (wait(&status) == ERROR)
          return ERROR;
        printf("The process %d has finished because of the WAIT.\n", status);
        activeProcesses--;
      }
      pid_t pid = fork();
      if (pid < 0) {
        perror("Error forking process");
        closedir(dir);
        return ERROR;

      } else if (pid == 0) { // Child process
        char *pathJobs = pathingJobs(directoryPath, entry);
        char *pathOut = pathingOut(directoryPath, entry);
        if (process_file(pathJobs, pathOut) != 0) {
          fprintf(stderr, "Error processing file: %s\n", pathJobs);
          closedir(dir);
          return ERROR;
        }
        free(pathJobs);
        free(pathOut);
        exit(0);

      } else { // Parent process
        ++activeProcesses;
        printf("Number of active processes: %d.\n", activeProcesses);
      }
    }
  }
  printf("THE FATHER WILL START WAITING FOR THE CONCLUSION.\n Number of active processes:%d \n", activeProcesses);
  // Wait for all remaining child processes to finish
  while (activeProcesses > 0) {
      wait(&status);
      printf("The process %d has finished because the father waited for its conclusion.\n", status);
      activeProcesses--;
  }

  closedir(dir);

  return 0;
}

/* Function that processes the input file and calls the functions that do the operations */
int process_file(char* pathJobs, char* pathOut) {

  // Open input file
  int fdRead = open(pathJobs, O_RDONLY);
  if (fdRead == ERROR) {
      perror("Error opening input file");
      return ERROR;
  }

  // Open output file
  int fdWrite = open(pathOut, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
  if (fdWrite == ERROR) {
      perror("Error opening output file");
      close(fdRead);
      return ERROR;
  }

  size_t xs[global_num_threads][MAX_RESERVATION_SIZE];
  size_t ys[global_num_threads][MAX_RESERVATION_SIZE];
  pthread_mutex_t mutex;

  if (pthread_mutex_init(&mutex, NULL) != 0) {
    perror("Error initializing mutex");
    close(fdRead);
    close(fdWrite);
    return ERROR;
  }

  int barrierFlag = 0;
  unsigned int waitingThread = 0;
  unsigned int delayWait = 0;

  // Initialize waitFlags
  int waitFlags[global_num_threads + 1];
  for (int i = 0; i <= global_num_threads; i++)
    waitFlags[i] = 0;
  
  pthread_t threads[global_num_threads];
  ThreadParameters threadParameters[global_num_threads];

  int resultadoThread;
  while (1) {
    int flagBarrier = 0;
    for (int i = 0; i < global_num_threads; i++) {
      threadParameters[i].barrierFlag = &barrierFlag;
      threadParameters[i].mutex = &mutex;
      threadParameters[i].waitingThread = &waitingThread;
      threadParameters[i].delayWait = &delayWait;
      threadParameters[i].fdRead = fdRead;
      threadParameters[i].fdWrite = fdWrite;
      threadParameters[i].xs = xs[i];
      threadParameters[i].ys = ys[i];
      threadParameters[i].thread_id = i+1;
      threadParameters[i].waitFlags = waitFlags;
      if (pthread_create(&threads[i], NULL, thread_execute, &threadParameters[i]) != 0) {
        perror("Error creating thread");
        // Deal with the failure to create the thread, freeing resources and terminating previous threads
        if (close(fdRead) == ERROR) {
          perror("Error closing input file.");
          return ERROR;
        }
        if (close(fdWrite) == ERROR) {
          perror("Error closing output file.");
          return ERROR;
        }
        if (pthread_mutex_destroy(&mutex) != 0) {
          fprintf(stderr, "Error: Failed to destroy mutex.\n");
        }
        return ERROR;
      }
    }
    for (int i = 0; i < global_num_threads; i++) {
      if (pthread_join(threads[i], (void*)&resultadoThread) != 0) {
        perror("Error waiting for thread to finish.");
        // Deal with the failure to join the thread, freeing resources and terminating previous threads
        if (close(fdRead) == ERROR) {
          perror("Error closing input file.");
          return ERROR;
        }
        if (close(fdWrite) == ERROR) {
          perror("Error closing output file.");
          return ERROR;
        }
        if (pthread_mutex_destroy(&mutex) != 0) {
          fprintf(stderr, "Error: Failed to destroy mutex.\n");
        }
        return ERROR;
      }
      if (resultadoThread == BARRIER) {
        flagBarrier = 1;
      } else if (resultadoThread == ERROR){
        return ERROR;
      }
    }
    // If there was a barrier, the threads are restarted
    if (!flagBarrier) break;
    else barrierFlag = 0;
    
  }

  if (close(fdRead) == ERROR) {
    perror("Error closing input file.");
    return ERROR;
  }
  if (close(fdWrite) == ERROR) {
    perror("Error closing output file.");
    return ERROR;
  }
  if (pthread_mutex_destroy(&mutex) != 0) {
    fprintf(stderr, "Error: Failed to destroy mutex.\n");
    return ERROR;
  }

  ems_terminate();
  return 0;
}

/* Function that executes the commands */
void* thread_execute(void* args) {
  ThreadParameters *parameters = (ThreadParameters*)args;
  int fdRead = (parameters)->fdRead;
  int fdWrite = (parameters)->fdWrite;
  pthread_mutex_t * mutex = (parameters)->mutex;
  size_t *xs = (parameters)->xs;
  size_t *ys = (parameters)->ys;

  while (1) {
    unsigned int event_id;
    size_t num_rows, num_columns, num_coords;

    if (pthread_mutex_lock(mutex) != 0) {
      fprintf(stderr, "Error: Failed to lock mutex.\n");
      return (void*)ERROR;
    }
    if (parameters->waitFlags[parameters->thread_id] == 1) {
      if (pthread_mutex_unlock(mutex) != 0) {
        fprintf(stderr, "Error: Failed to unlock mutex.\n");
        return (void*)ERROR;
      }
      ems_wait(*parameters->delayWait);
      if (pthread_mutex_lock(mutex) != 0) {
        fprintf(stderr, "Error: Failed to lock mutex.\n");
        return (void*)ERROR;
      }
      parameters->waitFlags[parameters->thread_id] = 0;
    }

    if (*parameters->barrierFlag) {
      if (pthread_mutex_unlock(mutex) != 0) {
        fprintf(stderr, "Error: Failed to unlock mutex.\n");
        return (void*)ERROR;
      }

      return (void*)BARRIER;
    }

    // Read command
    int command = (int)get_next(fdRead);
    switch (command) {
        case CMD_CREATE:
          if (parse_create(fdRead, &event_id, &num_rows, &num_columns) != 0) {
            fprintf(stderr, "Invalid command. See HELP for usage.\n");
            continue;
          }

          if (pthread_mutex_unlock(mutex) != 0) {
            fprintf(stderr, "Error: Failed to unlock mutex.\n");
            return (void*)ERROR;
          }

          if (ems_create(event_id, num_rows, num_columns)) {
            fprintf(stderr, "Failed to create event.\n");
          }

          break;

        case CMD_RESERVE:
          num_coords = parse_reserve(fdRead, MAX_RESERVATION_SIZE, &event_id, xs, ys);

          if (pthread_mutex_unlock(mutex) != 0) {
            fprintf(stderr, "Error: Failed to unlock mutex.\n");
            return (void*)ERROR;
          }

          if (num_coords == 0) {
            fprintf(stderr, "Invalid command. See HELP for usage.\n");
            continue;
          }

          if (ems_reserve(event_id, num_coords, xs, ys)) {
            fprintf(stderr, "Failed to reserve seats.\n");
          }

          break;

        case CMD_SHOW:
          if (parse_show(fdRead, &event_id) != 0) {
            fprintf(stderr, "Invalid command. See HELP for usage.\n");
            continue;
          }

          if (pthread_mutex_unlock(mutex) != 0) {
            fprintf(stderr, "Error: Failed to unlock mutex.\n");
            return (void*)ERROR;
          }

          if (ems_show(event_id, fdWrite)) {
            fprintf(stderr, "Failed to show event.\n");
          }

          break;

        case CMD_LIST_EVENTS:
          if (pthread_mutex_unlock(mutex) != 0) {
            fprintf(stderr, "Error: Failed to unlock mutex.\n");
            return (void*)ERROR;
          }

          if (ems_list_events(fdWrite)) {
            fprintf(stderr, "Failed to list events.\n");
          }

          break;

        case CMD_WAIT:
          if (parse_wait(fdRead, &(*parameters->delayWait), &(*parameters->waitingThread))) {
            fprintf(stderr, "Invalid command. See HELP for usage.\n");
            continue;
          }

          if (*parameters->delayWait > 0 && *parameters->waitingThread == 0) {
            ems_wait(*parameters->delayWait);
            if (pthread_mutex_unlock(mutex) != 0) {
              fprintf(stderr, "Error: Failed to unlock mutex.\n");
              return (void*)ERROR;
            }
            break;
          }
          else if ((int)*parameters->waitingThread <= global_num_threads) {
            parameters->waitFlags[*parameters->waitingThread] = 1;
          }
          
          if (pthread_mutex_unlock(mutex) != 0) {
            fprintf(stderr, "Error: Failed to unlock mutex.\n");
            return (void*)ERROR;
          }

          break;

        case CMD_INVALID:
          if (pthread_mutex_unlock(mutex) != 0) {
            fprintf(stderr,"Error: Failed to unlock mutex.\n");
            return (void*)ERROR;
          }
          fprintf(stderr, "Invalid command. See HELP for usage.\n");
          break;

        case CMD_HELP:
          if (pthread_mutex_unlock(mutex) != 0) {
            fprintf(stderr, "Error: Failed to unlock mutex.\n");
            return (void*)ERROR;
          }
          printf(
              "Available commands:\n"
              "  CREATE <event_id> <num_rows> <num_columns>\n"
              "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
              "  SHOW <event_id>\n"
              "  LIST\n"
              "  WAIT <delay_ms> [thread_id]\n"
              "  BARRIER\n"
              "  HELP\n");

          break;

        case CMD_BARRIER:
          *parameters->barrierFlag = BARRIER;
          if (pthread_mutex_unlock(mutex) != 0) {
            fprintf(stderr, "Error: Failed to unlock mutex.\n");
            return (void*)ERROR;
          }
          return (void*)BARRIER;
        case CMD_EMPTY:
          if (pthread_mutex_unlock(mutex) != 0) {
            fprintf(stderr, "Error: Failed to unlock mutex.\n");
            return (void*)ERROR;
          }
          break;

        case EOC:
          if (pthread_mutex_unlock(mutex) != 0) {
            fprintf(stderr, "Error: Failed to unlock mutex.\n");
            return (void*)ERROR;
          }
          return (void*)0;
      }
    }
  }
