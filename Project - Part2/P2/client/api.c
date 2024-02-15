#include "api.h"
#include "common/rw_aux.h"
#include "common/constants.h"
#include "common/io.h"
#include "server/operations.h"

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

const char* request_pipe;
const char* response_pipe;

int fd_req_pipe = -1;
int fd_resp_pipe = -1;

int session_id;

/* 
 * Initializes the connection with the server.
 * 
 * @param req_pipe_path - path to the request pipe
 * @param resp_pipe_path - path to the response pipe
 * @param server_pipe_path - path to the server pipe
 * 
 * @return 0 on success, 1 otherwise */
int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  
  char *buffer;

  // remove pipe if it does exist
  if (unlink(req_pipe_path) != 0 && errno != ENOENT) {
    fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", req_pipe_path, strerror(errno));
    exit(EXIT_FAILURE);
  }

  // remove pipe if it does exist
  if (unlink(resp_pipe_path) != 0 && errno != ENOENT) {
    fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", resp_pipe_path, strerror(errno));
    exit(EXIT_FAILURE);
  }

  // create pipe
  if (mkfifo(req_pipe_path, 0640) != 0) {
    fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // create pipe
  if (mkfifo(resp_pipe_path, 0640) != 0) {
    fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  request_pipe = req_pipe_path;
  response_pipe = resp_pipe_path;

  int tx = open(server_pipe_path, O_WRONLY);
  if (tx == -1) {
    fprintf(stderr, "Error opening the server pipe.\n");
    return 1;
  }

  char ch = '1';
  buffer = malloc(sizeof(char));
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  memcpy(buffer, &ch, sizeof(char));
  writeFile(tx, buffer, sizeof(char));
  free(buffer);

  buffer = malloc(sizeof(char) * MAX_PIPE_NAME * 2);
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  memset(buffer, '\0', MAX_PIPE_NAME * 2);
  strncpy(buffer, request_pipe, MAX_PIPE_NAME);
  strncpy(buffer + MAX_PIPE_NAME, response_pipe, MAX_PIPE_NAME);

  writeFile(tx, buffer, sizeof(char) * MAX_PIPE_NAME * 2);

  free(buffer);
  if (close(tx) == -1) {
    perror("Error closing server pipe.\n");
    return 1;
  }

  fd_req_pipe = open(req_pipe_path, O_WRONLY);
  if (fd_req_pipe == -1) {
    perror("Error opening request pipe.\n");
    return 1;
  }

  fd_resp_pipe = open(resp_pipe_path, O_RDONLY);
  if (fd_resp_pipe == -1) {
    perror("Error opening response pipe.\n");
    return 1;
  }

  buffer = malloc(sizeof(int));
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  readBuffer(fd_resp_pipe, buffer, sizeof(int));
  memcpy(&session_id, buffer, sizeof(int));
  free(buffer);
  
  return 0;
}

/* 
 * Closes the connection with the server.
 * 
 * @return 0 on success, 1 otherwise */
int ems_quit(void) { 

  char *buffer;
  char ch = '2';
  buffer = malloc(sizeof(char));
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  memcpy(buffer, &ch, sizeof(char));
  writeFile(fd_req_pipe, buffer, sizeof(char));
  free(buffer);

  if (close(fd_req_pipe) == -1) {
    perror("Error closing request pipe.\n");
    exit(EXIT_FAILURE);
  }

  if (close(fd_resp_pipe) == -1) {
    perror("Error closing response pipe.\n");
    exit(EXIT_FAILURE);
  }

  return 0;
}

/* 
 * Creates a new event.
 * 
 * @param event_id - event identifier
 * @param num_rows - number of rows
 * @param num_cols - number of columns
 * 
 * @return 0 on success, 1 otherwise */
int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {

  int status;
  char *buffer, *ptr;

  char ch = '3';

  buffer = malloc(sizeof(char));
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  memcpy(buffer, &ch, sizeof(char));
  writeFile(fd_req_pipe, buffer, sizeof(char));
  free(buffer);

  buffer = malloc(sizeof(unsigned int) + (sizeof(size_t) * 2));
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  ptr = buffer;
  memcpy(ptr, &event_id, sizeof(unsigned int));
  ptr += sizeof(unsigned int);
  memcpy(ptr, &num_rows, sizeof(size_t));
  ptr += sizeof(size_t);
  memcpy(ptr, &num_cols, sizeof(size_t));

  writeFile(fd_req_pipe, buffer, sizeof(unsigned int) + (sizeof(size_t) * 2));
  free(buffer);

  buffer = malloc(sizeof(int));
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  readBuffer(fd_resp_pipe, buffer, sizeof(int));
  memcpy(&status, buffer, sizeof(int));
  free(buffer);

  return status;
}

/* 
 * Reserves seats for an event.
 * 
 * @param event_id - event identifier
 * @param num_seats - number of seats to reserve
 * @param xs - array with the row numbers
 * @param ys - array with the column numbers
 * 
 * @return 0 on success, 1 otherwise */
int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  
  int status;
  char *buffer, *ptr;
  char ch = '4';

  buffer = malloc(sizeof(char));
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  memcpy(buffer, &ch, sizeof(char));
  writeFile(fd_req_pipe, buffer, sizeof(char));
  free(buffer);

  size_t size_event_id = sizeof(unsigned int);
  size_t size_num_seats = sizeof(size_t);
  size_t size_reservation_seat = sizeof(size_t);
  size_t size_xs = sizeof(size_t) * MAX_RESERVATION_SIZE;
  size_t size_ys = sizeof(size_t) * MAX_RESERVATION_SIZE;

  size_t total_size = size_event_id + size_num_seats + size_xs + size_ys;

  buffer = malloc(total_size);
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }

  ptr = buffer;
  memcpy(ptr, &event_id, size_event_id);
  ptr += size_event_id;
  memcpy(ptr, &num_seats, size_num_seats);
  ptr += size_num_seats;

  for (int i = 0; i < (int)num_seats; i++) {
    memcpy(ptr, &xs[i], size_reservation_seat);
    ptr += size_reservation_seat;
    memcpy(ptr, &ys[i], size_reservation_seat);
    ptr += size_reservation_seat;
  }

  writeFile(fd_req_pipe, buffer, total_size);
  free(buffer);

  buffer = malloc(sizeof(int));
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  readBuffer(fd_resp_pipe, buffer, sizeof(int));
  memcpy(&status, buffer, sizeof(int));
  free(buffer);

  return status;
}

/* 
 * Shows the seats of an event.
 * 
 * @param out_fd - file descriptor to write the seats
 * @param event_id - event identifier
 * 
 * @return 0 on success, 1 otherwise */
int ems_show(int out_fd, unsigned int event_id) {

  int status;
  char *buffer, *ptr;
  size_t num_rows, num_cols, num_seats;
  char ch = '5';

  buffer = malloc(sizeof(char));
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  memcpy(buffer, &ch, sizeof(char));
  writeFile(fd_req_pipe, buffer, sizeof(char));
  free(buffer);

  buffer = malloc(sizeof(unsigned int) + sizeof(int));
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  memcpy(buffer, &event_id, sizeof(unsigned int));
  writeFile(fd_req_pipe, buffer, sizeof(unsigned int));
  free(buffer);


  buffer = malloc(sizeof(int));
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  readBuffer(fd_resp_pipe, buffer, sizeof(int));
  memcpy(&status, buffer, sizeof(int));
  free(buffer);
  if (status) return 1;

  buffer = malloc(sizeof(size_t) * 2);
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  readBuffer(fd_resp_pipe, buffer, sizeof(size_t) * 2);
  ptr = buffer;
  memcpy(&num_rows, ptr, sizeof(size_t));
  ptr += sizeof(size_t);
  memcpy(&num_cols, ptr, sizeof(size_t));
  free(buffer);

  num_seats = num_rows * num_cols;
  unsigned int show_seats[num_seats];
  memset(show_seats, 0, num_seats * sizeof(unsigned int));

  buffer = malloc(sizeof(unsigned int) * num_seats);
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  readBuffer(fd_resp_pipe, buffer, sizeof(unsigned int) * num_seats);
  ptr = buffer;

  for (size_t i = 0; i < num_seats; i++) {
    memcpy(&show_seats[i], ptr, sizeof(unsigned int));
    ptr += sizeof(unsigned int);
  }
  free(buffer);

  buffer = malloc((sizeof(char) * num_seats * 2) + 1);
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  ptr = buffer;
  int k = 0;
  for (size_t i = 1; i <= num_rows; i++) {
    for (size_t j = 1; j <= num_cols; j++) {
      int written = snprintf(ptr, 2, "%u", show_seats[k]);
      ptr += written;
      if (j < num_cols) {
          int space_written = snprintf(ptr, 2, " ");
          ptr += space_written;
      }
      k++;
    }
    int newline_written = snprintf(ptr, 2, "\n");
    ptr += newline_written;
  }
  *ptr = '\n';
  ptr++;
  writeFile(out_fd, buffer, (sizeof(char) * num_seats * 2) + 1);
  free(buffer);

  return status;
}

/* 
 * Lists the events.
 * 
 * @param out_fd - file descriptor to write the events
 * 
 * @return 0 on success, 1 otherwise */
int ems_list_events(int out_fd) {

  int status;
  char *buffer, *ptr;
  size_t num_events;
  char ch = '6';

  buffer = malloc(sizeof(char));
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  memcpy(buffer, &ch, sizeof(char));
  writeFile(fd_req_pipe, buffer, sizeof(char));
  free(buffer);

  buffer = malloc(sizeof(int));
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  readBuffer(fd_resp_pipe, buffer, sizeof(int));
  memcpy(&status, buffer, sizeof(int));
  free(buffer);
  if (status) return 1;


  buffer = malloc(sizeof(size_t));
  if (buffer == NULL) {
    perror("Error allocating memory.\n");
    exit(EXIT_FAILURE);
  }
  readBuffer(fd_resp_pipe, buffer, sizeof(size_t));
  memcpy(&num_events, buffer, sizeof(size_t));
  free(buffer);

  if (num_events == 0) {
    buffer = malloc(sizeof(char) * 11);
    if (buffer == NULL) {
      perror("Error allocating memory.\n");
      exit(EXIT_FAILURE);
    }
    buffer[0] = '\0';
    strcat(buffer, "No events\n");
    strcat(buffer, "\0");
    writeFile(out_fd, buffer, sizeof(char) * 10);
    free(buffer);
  } else {
    buffer = malloc(sizeof(unsigned int) * num_events);
    if (buffer == NULL) {
      perror("Error allocating memory.\n");
      exit(EXIT_FAILURE);
    }
    readBuffer(fd_resp_pipe, buffer, sizeof(unsigned int) * num_events);
    ptr = buffer;

    unsigned int list_ids[num_events];
    memset(list_ids, 0, num_events * sizeof(unsigned int));

    for (size_t i = 0; i < num_events; i++) {
      memcpy(&list_ids[i], ptr, sizeof(unsigned int));
      ptr += sizeof(unsigned int);
    }
    free(buffer);

    buffer = malloc(20 * num_events * sizeof(char));
    if (buffer == NULL) {
      perror("Error allocating memory.\n");
      exit(EXIT_FAILURE);
    }
    ptr = buffer;
    memset(buffer, '\0', 20 * num_events * sizeof(char));
    int numCharacters = 0;

    // 20 * num_events * sizeof(char):
    // 8 caracteres vindos do Event:\n" e outros 12 de tamanho máx que um event_id pode ter
    // não é dito por isso colocamos que um event_id tem no máximo 12 bytes de tamanho
    for (size_t i = 0; i < num_events; i++) {
      strcat(buffer, "Event: ");
      numCharacters+= 7;
      ptr += 7;
      int written = snprintf(ptr, 12, "%u\n", list_ids[i]);
      numCharacters += written;
      ptr += written;
    }

    writeFile(out_fd, buffer, sizeof(char) * (size_t)numCharacters);
    free(buffer);
  }

  return 0;
}
