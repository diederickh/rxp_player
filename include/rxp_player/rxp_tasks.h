/*

  rxp_task_queue 
  ---------------

  The rxp_task_queue is used by the scheduler to manage tasks. Tasks 
  are added to the end of the queue and should be handled in order. 


 */
#ifndef RXP_TASKS_H
#define RXP_TASKS_H

#include <rxp_player/rxp_types.h>
#include <uv.h>

typedef struct rxp_task rxp_task;
typedef struct rxp_task_queue rxp_task_queue;

struct rxp_task {
  int type;
  int state;
  void* data;
  rxp_task* next;
};

struct rxp_task_queue {
  rxp_task* tasks;
  rxp_task* last_task;
  uv_mutex_t mutex;
  uv_cond_t cond;
  int size;
};

rxp_task* rxp_task_alloc();                                              /* allocates and initializes a new task */
int rxp_task_dealloc(rxp_task* task);                                    /* deallocate a task */
int rxp_task_dealloc_all(rxp_task* task);                                /* deallocate all task int the list */
int rxp_task_queue_init(rxp_task_queue* queue);                          /* initializes the task queue */
int rxp_task_queue_shutdown(rxp_task_queue* queue);                      /* cleans up and frees all allocated members of a task queue */
int rxp_task_queue_add(rxp_task_queue* queue, rxp_task* task);           /* add a new task to the end of the queue - locking the queue + triggering the condition variable */
int rxp_task_queue_reset(rxp_task_queue* queue);                         /* resets the tasks to init state - NOT FREEING the elements*/
int rxp_task_queue_lock(rxp_task_queue* queue);                          /* locks the used mutex to protect internal data */
int rxp_task_queue_unlock(rxp_task_queue* queue);                        /* unlocks the mutex */
int rxp_task_queue_count_tasks(rxp_task_queue* queue, int type);         /* count the number of tasks for the given type */
void rxp_task_queue_cond_wait(rxp_task_queue* queue);

#endif
