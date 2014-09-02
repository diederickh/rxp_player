#include <stdlib.h>
#include <stdio.h>
#include <rxp_player/rxp_tasks.h>

rxp_task* rxp_task_alloc() {

  rxp_task* t = (rxp_task*)malloc(sizeof(rxp_task));

  if (!t) {
    printf("Error: cannot allocate a new rxp_task.\n");
    return NULL;
  }

  t->type = RXP_NONE;
  t->state = RXP_NONE;
  t->data = NULL;
  t->next = NULL;

  return t;
}

int rxp_task_dealloc (rxp_task* task) {

  if (!task) { return -1; } 

  if (task->data) {
    free(task->data);
    task->data = NULL;
  }

  free(task);
  task = NULL;

  return 0;
}

int rxp_task_dealloc_all (rxp_task* task) {

  rxp_task* next = NULL;
  
  if (!task) { return -1; } 

  while (task) {
    next = task->next;
    rxp_task_dealloc(task);
    task = next;
  }

  return 0;
}

int rxp_task_queue_init (rxp_task_queue* queue) {
 
  if (!queue) { return -1; } 

  if (1 == queue->is_init) {
    printf("Info: task queue already initialized.\n");
    return 0;
  }

  if (uv_mutex_init(&queue->mutex) != 0) {
    printf("Error: cannot initialize the queue mutex.\n");
    return -1;
  }

  if (uv_cond_init(&queue->cond) != 0) {
    printf("Error: cannot initialize the queue condition var.\n");
    return -2;
  }

  queue->size = 0;
  queue->tasks = NULL;
  queue->last_task = NULL;
  queue->is_init = 1;

  return 0;
}

int rxp_task_queue_shutdown(rxp_task_queue* queue) {
  
  if (!queue) { return -1; } 

  if (-1 == queue->is_init) {
    printf("Info task queue already shutdown.\n");
    return 0;
  }

  /* @todo in rxp_task_queue_shutdown, we should check if the mutex is actually initialized. */
  uv_mutex_destroy(&queue->mutex);

  /* @todo in rxp_task_queue_shutdown, we should check if the cond var is actually initialized. */
  uv_cond_destroy(&queue->cond);

  if (queue->tasks) { 
    if (rxp_task_dealloc_all(queue->tasks) < 0) {
      printf("Error: cannot deallocate the task queue tasks.\n");
      return -4;
    }
  }
  
  queue->size = 0;
  queue->tasks = NULL;
  queue->last_task = NULL;
  queue->is_init = -1;

  return 0;
}

int rxp_task_queue_reset (rxp_task_queue* queue) {
  
  if (!queue) { return -1; } 

  queue->tasks = NULL;
  queue->last_task = NULL;
  queue->size = 0;
  
  return 0;
}

int rxp_task_queue_add (rxp_task_queue* queue, rxp_task* task) {
  
  if (!queue) { return -1; }
  if (!task) { return -2; } 

  uv_mutex_lock(&queue->mutex);
  {
    /* append the task to the end */
    if (!queue->last_task) {
      queue->tasks = task;
    }
    else {
      queue->last_task->next = task;
    }

    queue->last_task = task;
    queue->size++;
  }

  uv_cond_signal(&queue->cond);
  uv_mutex_unlock(&queue->mutex);

  return 0;
}


int rxp_task_queue_lock(rxp_task_queue* queue) {
  if (!queue) { return -1; } 
  uv_mutex_lock(&queue->mutex);
  return 0;
}

int rxp_task_queue_unlock(rxp_task_queue* queue) {
  if (!queue) { return -1; } 
  uv_mutex_unlock(&queue->mutex);
  return 0;
}

void rxp_task_queue_cond_wait(rxp_task_queue* queue) {
#if !defined(NDEBUG)
  if(!queue) { printf("Error: invalid queue given.\n"); return; } 
#endif

  uv_cond_wait(&queue->cond, &queue->mutex);
}

int rxp_task_queue_count_tasks(rxp_task_queue* queue, int type) {

  int count = 0;
  
  if (!queue) { return -1; } 
 
  rxp_task_queue_lock(queue);
  {
    rxp_task* task = queue->tasks;
    while (task) {
      if (task->type == type) {
        count++;
      }
      task = task->next;
    }
  }
  rxp_task_queue_unlock(queue);

  return count;
}
