#ifndef AUX_FUNCTIONS_H
#define AUX_FUNCTIONS_H

#include <string.h>
#include <dirent.h>
#include "eventlist.h"

int writeFile(int fd, char* buffer);
char* pathingOut(const char *directoryPath, struct dirent *entry);
char *pathingJobs(char *directoryPath, struct dirent *entry);
void freeMutexes(struct Event* event, size_t num_rows, size_t num_cols);
int sortVectors(size_t num_seats, size_t* xs, size_t* ys);
void switchPositions(size_t* xs, size_t* ys, size_t i, size_t j);

#endif
