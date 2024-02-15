#include "rw_aux.h"

#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

int writeFile(int fd, const char* buffer, size_t bufferSize) {
    size_t len = bufferSize;
    size_t done = 0;

    while (len > 0) {
        ssize_t bytes_written = write(fd, buffer + done, len);

        if (bytes_written < 0) {
            fprintf(stderr, "Write error: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        len -= (size_t)bytes_written;
        done += (size_t)bytes_written;
    }

    return 0;
}

int readBuffer(int fd, char *buffer, size_t bufferSize) {

   memset(buffer, 0, bufferSize);

   ssize_t bytes_read = read(fd, buffer, bufferSize);
   if (bytes_read < 0) {
    fprintf(stderr, "Read error: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
   }

   return 0;
}