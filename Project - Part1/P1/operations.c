#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>

#include "eventlist.h"
#include "auxFunctions.h"
#include "main.h"



#define BUFFERSIZE 1024
#define IDMAX 128

static struct EventList* event_list = NULL;
static unsigned int state_access_delay_ms = 0;
pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_rwlock_t global_rwlock = PTHREAD_RWLOCK_INITIALIZER;

/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event_id The ID of the event to get.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event* get_event_with_delay(unsigned int event_id) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed

  return get_event(event_list, event_id);
}

/// Gets the seat with the given index from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event Event to get the seat from.
/// @param index Index of the seat to get.
/// @return Pointer to the seat.
static unsigned int* get_seat_with_delay(struct Event* event, size_t index) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed

  return &event->data[index];
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event* event, size_t row, size_t col) { return (row - 1) * event->cols + col - 1; }

int ems_init(unsigned int delay_ms) {
  if (event_list != NULL) {
    fprintf(stderr, "EMS state has already been initialized\n");
    return 1;
  }

  event_list = create_list();
  state_access_delay_ms = delay_ms;

  return event_list == NULL;
}

int ems_terminate() {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  free_list(event_list);
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_wrlock(&global_rwlock) != 0) {
    fprintf(stderr, "Error locking write lock\n.");
    return 1;
  }
  
  if (get_event_with_delay(event_id) != NULL) {
    fprintf(stderr, "Event already exists\n");
    if(pthread_rwlock_unlock(&global_rwlock) != 0) {
      fprintf(stderr, "Error unloking write lock.\n");
    }
    return 1;
  }

  struct Event* event = malloc(sizeof(struct Event));
  if (event == NULL) {
    fprintf(stderr, "Error allocating memory for event.\n");
    if (pthread_rwlock_unlock(&global_rwlock) != 0) {
      fprintf(stderr, "Error unlocking write lock.\n");
      return 1;
    }
    return 1;
  }

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  if (pthread_mutex_init(&event->mutex, NULL) != 0) {
    fprintf(stderr, "Error initializing event mutex\n");
    return 1;
  }
  if (pthread_rwlock_init(&event->rwlock, NULL) != 0) {
    fprintf(stderr, "Error initializing event read-write lock\n");
    return 1;
  }
  event->data = malloc(num_rows * num_cols * sizeof(unsigned int));
  if (event->data == NULL) {
    fprintf(stderr, "Error allocating memory for event data\n");
    free(event);
    if (pthread_rwlock_unlock(&global_rwlock) != 0)
      fprintf(stderr, "Error unlocking write lock.\n");
    return 1;
  }

  event->seatsLock = malloc(event->rows * event->cols * sizeof(pthread_mutex_t));
  if (event->seatsLock == NULL) {
    fprintf(stderr, "Error allocating memory for seatsLock");
    return 1;
  }

  int num_seats = (int)(num_rows * num_cols);
  for (int i = 0; i < num_seats; i++){
    if (pthread_mutex_init(&event->seatsLock[i], NULL) != 0) {
      fprintf(stderr, "Error initializing mutex");
      return 1;
    }
  }

  for (size_t i = 0; i < num_rows * num_cols; i++) {
    event->data[i] = 0;
  }

  if (append_to_list(event_list, event) != 0) {
    fprintf(stderr, "Error appending event to list\n");
    free(event->data);

    for (int i = 0; i < num_seats; i++){
      if (pthread_mutex_destroy(&event->seatsLock[i]) != 0) {
        fprintf(stderr, "Error destroying mutex");
        return 1;
      }
    }
    free(event->seatsLock);

    if (pthread_rwlock_unlock(&global_rwlock) != 0)
      fprintf(stderr, "Error unlocking write lock.\n");
    if (pthread_rwlock_destroy(&event->rwlock) != 0)
        fprintf(stderr, "Error destroying event read-write lock\n");
    if (pthread_mutex_destroy(&event->mutex) != 0)
      fprintf(stderr, "Error destroying event mutex\n");
    free(event);
    return 1;
  }
  if (pthread_rwlock_unlock(&global_rwlock) != 0) {
    fprintf(stderr, "Error unlocking write lock.\n");
    return 1;
  }

  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized.\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&global_rwlock) != 0) {
    fprintf(stderr, "Error locking read lock.\n");
    return 1;
  }
  struct Event* event = get_event_with_delay(event_id);
  if (pthread_rwlock_unlock(&global_rwlock) != 0) {
    fprintf(stderr, "Error unlocking read lock.\n");
    return 1;
  }

  if (event == NULL) {
    fprintf(stderr, "Event not found.\n");
    return 1;
  }

  if (sortVectors(num_seats, xs, ys)) {
    fprintf(stderr, "Invalid Seats to Sort\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&event->rwlock) != 0) {
    fprintf(stderr, "Error locking read lock.\n");
    return 1;
  }

  unsigned int myValue = atomic_load(&event->reservations);
  unsigned int newId = myValue + 1;
  atomic_store(&event->reservations, newId);
  size_t i = 0;
  for (; i < num_seats; i++) {
    size_t row = xs[i];
    size_t col = ys[i];


    if (row <= 0 || row > event->rows || col <= 0 || col > event->cols) {
      fprintf(stderr, "Invalid seat\n");
      break;
    }

    if(pthread_mutex_lock(&event->seatsLock[seat_index(event, row, col)]) != 0) {
      fprintf(stderr, "Error locking mutex seat lock.\n");
      return 1;
    }

    if (*get_seat_with_delay(event, seat_index(event, row, col)) != 0) {
      fprintf(stderr, "Seat already reserved\n");
      if(pthread_mutex_unlock(&event->seatsLock[seat_index(event, row, col)]) != 0) {
        fprintf(stderr, "Error unlocking mutex seat lock\n.");
        return 1;
      }
      break;
    }

    *get_seat_with_delay(event, seat_index(event, row, col)) = newId;
  }
  
  // If the reservation was not successful, free the seats that were reserved.
  if (i < num_seats) {
    //event->reservations--;
    for (size_t j = 0; j < i; j++) {
      *get_seat_with_delay(event, seat_index(event, xs[j], ys[j])) = 0;
      if(pthread_mutex_unlock(&event->seatsLock[seat_index(event, xs[j], ys[j])]) != 0) {
        fprintf(stderr, "Error unlocking mutex seat lock\n.");
        return 1;
      }
    }
    if (pthread_rwlock_unlock(&event->rwlock) != 0) {
      fprintf(stderr, "Error unlocking write lock.\n");
      return 1;
    }
    return 1;
  }
  for (size_t j = 0; j < num_seats; j++) {
    if(pthread_mutex_unlock(&event->seatsLock[seat_index(event, xs[j], ys[j])]) != 0) {
      fprintf(stderr, "Error unlocking mutex seat lock\n.");
      return 1;
    }
  }
  if (pthread_rwlock_unlock(&event->rwlock) != 0) {
    fprintf(stderr, "Error unlocking write lock.\n");
    return 1;
  }
  return 0;
}

int ems_show(unsigned int event_id, int fdWrite) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized.\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&global_rwlock) != 0) {
    fprintf(stderr, "Error locking read lock.\n");
    return 1;
  }
  struct Event* event = get_event_with_delay(event_id);
  if (pthread_rwlock_unlock(&global_rwlock) != 0) {
    fprintf(stderr, "Error unlocking read lock.\n");
    return 1;
  }

  if (event == NULL) {
    fprintf(stderr, "Event not found.\n");
    return 1;
  }

  char *buffer = malloc(((event->rows * event->cols) * sizeof(char)) * 2 + 2);
  if (buffer == NULL) {
    fprintf(stderr, "Error allocating memory for buffer.\n");
    return 1;
  }
  char *current = buffer;  // Auxiliar pointer to the current position in the buffer

  if(pthread_rwlock_wrlock(&event->rwlock) != 0) {
    fprintf(stderr, "Error locking write event lock.\n");
    return 1;
  }
  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {
      unsigned int* seat = get_seat_with_delay(event, seat_index(event, i, j));
      int written = snprintf(current, 2, "%u", *seat);
      current += written;

      if (j < event->cols) {
          int space_written = snprintf(current, 2, " ");
          current += space_written;
      }
    }
    int newline_written = snprintf(current, 2, "\n");
    current += newline_written;
  }

  *current = '\n';
  current++;
  *current = '\0';
  if (pthread_rwlock_unlock(&event->rwlock) != 0) {
    fprintf(stderr, "Error unlocking read lock.\n");
    return 1;
  }
  if (pthread_mutex_lock(&global_mutex) != 0) {
    fprintf(stderr, "Error locking global mutex.\n");
    return 1;
  }

  writeFile(fdWrite, buffer);

  if (pthread_mutex_unlock(&global_mutex) != 0) {
    fprintf(stderr, "Error unlocking global mutex.\n");
    return 1;
  }
  free(buffer);
  return 0;
}


int ems_list_events(int fdWrite) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized.\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&global_rwlock) != 0) {
    fprintf(stderr, "Error locking read lock.\n");
    return 1;
  }

  if (event_list->head == NULL) {
    if (pthread_rwlock_unlock(&global_rwlock) != 0) {
        fprintf(stderr, "Error unlocking read lock.\n");
        return 1;
    }
    if (pthread_mutex_lock(&global_mutex) != 0) {
        fprintf(stderr, "Error locking global mutex.\n");
        return 1;
    }

    writeFile(fdWrite, "No events\n");

    if (pthread_mutex_unlock(&global_mutex) != 0) {
        fprintf(stderr, "Error unlocking global mutex.\n");
        return 1;
    }

    return 0;
  }

  struct ListNode* current = event_list->head;
  if (pthread_rwlock_unlock(&global_rwlock) != 0) {
    fprintf(stderr, "Error unlocking read lock.\n");
    return 1;
  }

  char *buffer = malloc(sizeof(char) * BUFFERSIZE);
  if (buffer == NULL) {
    fprintf(stderr, "Error allocating memory for buffer.\n");
    return 1;
  }

  buffer[0] = '\0';

  while (current != NULL) {
    if (pthread_rwlock_rdlock(&global_rwlock) != 0) {
      fprintf(stderr, "Error locking read lock.\n");
      return 1;
    }

    char *id = malloc(IDMAX*sizeof(char));
    if (id == NULL) {
      fprintf(stderr, "Error allocating memory for id.\n");
      return 1;
    }
    sprintf(id, "%u\n", (current->event)->id);
    current = current->next;
    
    if (pthread_rwlock_unlock(&global_rwlock) != 0) {
    fprintf(stderr, "Error unlocking read lock.\n");
    return 1;
    }
    strcat(buffer, "Event: ");
    strcat(buffer, id);
    free(id);
  }

  if (pthread_mutex_lock(&global_mutex) != 0) {
    fprintf(stderr, "Error locking global mutex.\n");
    return 1;
  }

  writeFile(fdWrite, buffer);

  if (pthread_mutex_unlock(&global_mutex) != 0) {
    fprintf(stderr, "Error unlocking global mutex.\n");
    return 1;
  }
  free(buffer);

  return 0;
}

void ems_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}
