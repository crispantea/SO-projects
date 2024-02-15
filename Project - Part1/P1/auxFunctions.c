#include "auxFunctions.h"
#include "main.h"
#include "eventlist.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

/* Function that creates the path to the input file */
char *pathingJobs(char *directoryPath, struct dirent *entry) {

  size_t length = strlen(directoryPath) + strlen(entry->d_name) + 2;
  char *pathJobs = (char *)malloc(length);

  if (pathJobs == NULL) {
    fprintf(stderr, "Error: Failed to allocate memory\n");
    return NULL;
  }

  snprintf(pathJobs, length, "%s/%s", directoryPath, entry->d_name);

  return pathJobs;
}

/* Function that creates the path to the output file */
char* pathingOut(const char *directoryPath, struct dirent *entry) {
  const char *extension_to_remove = ".jobs";
  const char *new_extension = ".out";

  // Find the position of the ".jobs" extension in the string
  const char *extension_position = strstr(entry->d_name, extension_to_remove);

  // Calculate the length of the part of the filename before the ".jobs" extension
  size_t directory_length = strlen(directoryPath);
  size_t prefix_length = strlen(entry->d_name) - strlen(extension_position); //entry->d_name é um apontador para o inicio da string e extension_position é um apontador para onde o .jobs começa

  // Dinamically allocate memory for the new string
  char *pathFileOut = (char *)malloc(directory_length + prefix_length + strlen(new_extension) + 2);

  if (pathFileOut == NULL) {
      fprintf(stderr, "Error: Failed to allocate memory\n");
      return NULL;
  }

  // Copy the directory path and the filename prefix to the new string
  strcpy(pathFileOut, directoryPath);
  strcat(pathFileOut, "/");
  strncat(pathFileOut, entry->d_name, prefix_length);
  strcat(pathFileOut, new_extension);

  return pathFileOut;
}

/* Functtion that writes to the output file */
int writeFile(int fd, char* buffer) {
    size_t len = strlen(buffer);
    size_t done = 0;

    while (len > 0) {
        ssize_t bytes_written = write(fd, buffer + done, len);

        if (bytes_written < 0) {
            fprintf(stderr, "Write error: %s\n", strerror(errno));
            return -1;
        }

        // might not have managed to write all, len becomes what remains
        len -= (size_t)bytes_written;
        done += (size_t)bytes_written;
    }

    return 0;
}

void freeMutexes(struct Event* event, size_t num_rows, size_t num_cols) {
    int num_seats = (int)(num_rows * num_cols);

    for (int i = 0; i < num_seats; i++) {
        if (pthread_mutex_destroy(&event->seatsLock[i]) != 0) {
            fprintf(stderr, "Error destroying mutex %d\n", i);
        }
    }
    free(event->seatsLock);
}

void switchPositions(size_t* firstVector, size_t* secondVector, size_t i, size_t j) {
  size_t temp = firstVector[i];
  firstVector[i] = firstVector[j];
  firstVector[j] = temp;

  temp = secondVector[i];
  secondVector[i] = secondVector[j];
  secondVector[j] = temp;
}

int sortVectors(size_t num_seats, size_t* firstVector, size_t* secondVector) {
  for (size_t i = 0; i < num_seats - 1; i++) {
    for (size_t j = 0; j < num_seats - i - 1; j++) {
      if (firstVector[j] > firstVector[j + 1]) {
          switchPositions(firstVector, secondVector, j, j + 1);
      }
      else if (firstVector[j] == firstVector[j + 1] && secondVector[j] == secondVector[j + 1]) {
        return 1;
      }
      else if (firstVector[j] == firstVector[j + 1] && secondVector[j] > secondVector[j + 1]) {
          switchPositions(firstVector, secondVector, j, j + 1);
      }
    }
  }
  return 0;
}
