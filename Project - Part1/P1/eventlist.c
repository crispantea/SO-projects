#include "eventlist.h"
#include "auxFunctions.h"

#include <stdlib.h>
#include <stdio.h>

struct EventList* create_list() {
  struct EventList* list = (struct EventList*)malloc(sizeof(struct EventList));
  if (!list) return NULL;
  list->head = NULL;
  list->tail = NULL;
  return list;
}

int append_to_list(struct EventList* list, struct Event* event) {
  if (!list) return 1;

  struct ListNode* new_node = (struct ListNode*)malloc(sizeof(struct ListNode));
  if (!new_node) return 1;

  new_node->event = event;
  new_node->next = NULL;

  if (list->head == NULL) {
    list->head = new_node;
    list->tail = new_node;
  } else {
    list->tail->next = new_node;
    list->tail = new_node;
  }

  return 0;
}

static void free_event(struct Event* event) {
  if (!event) return;

  if (pthread_rwlock_destroy(&event->rwlock) != 0) {
    fprintf(stderr, "Erro ao destruir a read-write lock do evento\n");
  }

  if (pthread_mutex_destroy(&event->mutex) != 0) {
    fprintf(stderr, "Erro ao destruir o mutex do evento\n");
  }

  freeMutexes(event, event->rows, event->cols);
  free(event->data);
  free(event);
}

void free_list(struct EventList* list) {
  if (!list) return;

  struct ListNode* current = list->head;
  while (current) {
    struct ListNode* temp = current;
    current = current->next;

    free_event(temp->event);
    free(temp);
  }

  free(list);
}

struct Event* get_event(struct EventList* list, unsigned int event_id) {
  if (!list) return NULL;
  struct ListNode* current = list->head;
  while (current) {
    struct Event* event = current->event;
    if (event->id == event_id) {
      return event;
    }
    current = current->next;
  }

  return NULL;
}
