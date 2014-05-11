#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rxp_player/rxp_scheduler.h>

/* ---------------------------------------------------------------- */

static void rxp_scheduler_thread(void* scheduler);                         /* the thread function from which we trigger decoding and basic play/stop state */
static void rxp_scheduler_handle_task(rxp_scheduler* s, rxp_task* task);   /* is called from the thread function and will call any set callbacks */
static void rxp_scheduler_add_decode_task(rxp_scheduler* s);               /* adds a decode tasks and updates internal state */
static int rxp_scheduler_add_task(rxp_scheduler* s, int tasktype);         /* add a general task that's handled in the thread */
static int rxp_scheduler_update_goal_pts(rxp_scheduler* s, uint64_t pts);  /* is used internally to make sure we will decode some more when necessary */
static int rxp_scheduler_lock(rxp_scheduler* s);                           /* used to protect the internally used data of the scheduler */
static int rxp_scheduler_unlock(rxp_scheduler* s);                         /* used to protect the internally used data of the scheduler */

/* ---------------------------------------------------------------- */

rxp_scheduler* rxp_scheduler_alloc() {

  rxp_scheduler* s = (rxp_scheduler*)malloc(sizeof(rxp_scheduler));

  if (!s) {
    printf("Error: cannot allocate a scheduler.\n");
    return NULL;
  }
  
  if (rxp_scheduler_init(s) < 0) {
    return NULL;
  }

  return s;
}

int rxp_scheduler_clear(rxp_scheduler* s) {

  if (!s) { return -1; } 

  if(s->state != RXP_SCHED_STATE_NONE) {
    printf("Error: Your'e trying to dealloc a scheduler which still has some state: %d\n", s->state);
    return -2;
  }

  s->decode = NULL;
  s->open_file = NULL;
  s->close_file = NULL;
  s->play = NULL;
  s->stop = NULL;
  s->user = NULL;
  s->goal_pts = 0;
  s->decoded_pts = 0;
  s->played_pts = 0;
  s->state = RXP_SCHED_STATE_NONE;
  s->thread = 0;

  uv_mutex_destroy(&s->mutex);

  if (rxp_task_queue_shutdown(&s->tasks) < 0) {
    printf("Error: cannot cleanup the task queue of the scheduler.\n");
    return -4;
  }


  return 0;
}

int rxp_scheduler_init(rxp_scheduler* s) {

  if (!s) { return -1; } 

  /* callbacks */
  s->decode = NULL;
  s->open_file = NULL;
  s->close_file = NULL;
  s->play = NULL;
  s->stop = NULL;
  s->user = NULL;
  s->thread = 0;
  s->goal_pts = 0;
  s->decoded_pts = 0;
  s->played_pts = 0;
  s->state = RXP_SCHED_STATE_NONE;
  
  if (uv_mutex_init(&s->mutex) != 0) {
    printf("Error: cannot initialze the mutex for scheduler.\n");
    free(s);
    s = NULL;
    return -2;
  }

  if (rxp_task_queue_init(&s->tasks) < 0) {
    printf("Error: cannot initialize the task queue.\n");
    free(s);
    s = NULL;
    return -3;
  }

  return 0;
}

int rxp_scheduler_start(rxp_scheduler* s) {

  if (!s) { return -1; }

  s->state |= RXP_SCHED_STATE_STARTED;
  uv_thread_create(&s->thread, rxp_scheduler_thread, (void*)s);

  return 0;
}

int rxp_scheduler_open_file(rxp_scheduler* s, char* file) {
 
  if (!s) { return -1; } 
  if (!file) { return -2; } 

  rxp_task* task = rxp_task_alloc();
  if (!task) {
    return -3;
  }

  task->data = malloc(strlen(file) + 1); /* @todo: should we check for a max size in rxp_scheduler_open_file? */
  if (!task->data) {
    printf("Error: cannot allocate memory for the open file task.\n");
    free(task);
    task = NULL;
    return -4;
  }

  memcpy(task->data, file, strlen(file) + 1);

  task->type = RXP_TASK_OPEN_FILE;
 
  if (rxp_task_queue_add(&s->tasks, task) < 0) {
    printf("Error: cannot add the open file task to the task queue.\n");
    free(task);
    task = NULL;
    return -5;
  }
  
  rxp_scheduler_update_goal_pts(s, 3 * 1000ull * 1000ull * 1000ull); /* make sure to decode the next N-frames */
  rxp_scheduler_add_decode_task(s); 

  rxp_scheduler_lock(s);
    s->state |= RXP_SCHED_STATE_DECODING;
  rxp_scheduler_unlock(s);

  return 0;
}

int rxp_scheduler_play(rxp_scheduler* s) {
  if (!s) { return -1; } 
  rxp_scheduler_add_task(s, RXP_TASK_PLAY);
  return 0;
}

int rxp_scheduler_stop(rxp_scheduler* s) {
  if (!s) { return -1; } 
  rxp_scheduler_add_task(s, RXP_TASK_STOP);
  uv_thread_join(&s->thread);
  return 0;
}

int rxp_scheduler_close_file(rxp_scheduler* s) {
  if (!s) { return -1; } 
  rxp_scheduler_add_task(s, RXP_TASK_CLOSE_FILE);
  return 0;
}

void rxp_scheduler_update(rxp_scheduler* s) {

#if !defined(NDEBUG)
  if (!s) { printf("Error: invalid scheduler given.\n"); return;} 
#endif

  uint64_t decoded_pts;
  uint64_t goal_pts;
  int state = 0;

  /* check if we need to decode a  bit more */
  rxp_scheduler_lock(s);
    decoded_pts = s->decoded_pts;
    goal_pts = s->goal_pts;
    state = s->state;
  rxp_scheduler_unlock(s);

  if (state == RXP_SCHED_STATE_NONE) {
    return;
  }

  /* add a new decoding task when we're not yet decoding (the decoding 
     state is unset in the thread function)  */
  if ( (state & RXP_SCHED_STATE_DECODING) != RXP_SCHED_STATE_DECODING
      && decoded_pts < goal_pts) 
  {
    rxp_scheduler_add_decode_task(s);
  }

  /* update the goal pts */
  rxp_scheduler_update_goal_pts(s, (5 * 1000ull * 1000ull * 1000ull));
}

/* tell the scheduler up until what pts we've decoded either video or 
   audio, so the decoder knows if it needs to decode some more from 
   within the thread. this should be called by the user of the 
   scheduler */
int rxp_scheduler_update_decode_pts(rxp_scheduler* s, uint64_t pts) {

  if (!s) { return -1; } 

  rxp_scheduler_lock(s);
  {
    if (pts > s->decoded_pts) {
      s->decoded_pts = pts;
    }
  }
  rxp_scheduler_unlock(s);

  return 0;
}

/* update the last played pts value, is used to update the goal pts  */
int rxp_scheduler_update_played_pts(rxp_scheduler* s, uint64_t pts) {

  if (!s) { return -1; } 

  rxp_scheduler_lock(s);
  {
    if (pts > s->played_pts) { 
      s->played_pts = pts;
    }
  }
  rxp_scheduler_unlock(s);

  return 0;
}


/* ---------------------------------------------------------------- */

static int rxp_scheduler_lock(rxp_scheduler* s) {
  if (!s) { return -1; } 
  uv_mutex_lock(&s->mutex);
  return 0;
}

static int rxp_scheduler_unlock(rxp_scheduler* s) {
  if (!s) { return -1; } 
  uv_mutex_unlock(&s->mutex);
  return 0;
}

static void rxp_scheduler_add_decode_task(rxp_scheduler* s) {

  rxp_task* task = rxp_task_alloc();
  if(!task) {
    printf("Error: cannot allocate a new rxp_task for the scheduler.\n");
    exit(1);
  }

  rxp_scheduler_lock(s);
    s->state |= RXP_SCHED_STATE_DECODING;
  rxp_scheduler_unlock(s);

  task->type = RXP_TASK_DECODE;

  rxp_task_queue_add(&s->tasks, task);
}

static int rxp_scheduler_add_task(rxp_scheduler* s, int tasktype) {

  if (!s) { return -1; } 

  rxp_task* task = rxp_task_alloc();
  if(!task) {
    printf("Error: cannot allocate a new rxp_task for the scheduler, in rxp_scheduler_stop().\n");
    exit(1);
  }

  task->type = tasktype;
  
  if (rxp_task_queue_add(&s->tasks, task) < 0) {
    printf("Error: cannot append the stop task to the scheduler task queue.\n");
    return -2;
  }

  return 0;
}

static void rxp_scheduler_thread(void* scheduler) {
  
  rxp_scheduler* s = (rxp_scheduler*) scheduler;
  rxp_task_queue* queue = &s->tasks;
  rxp_task* work = NULL; /* list with current work. */
  rxp_task* task = NULL; /* a task from the current work list */
  int should_stop = 0;
  
  while (1) {

    /* wait for new work */
    rxp_task_queue_lock(&s->tasks);
    {
      while (s->tasks.size == 0) {
        rxp_task_queue_cond_wait(&s->tasks);
      }

      /* copy work */
      if (queue->size > 0) {
        work = queue->tasks;
        rxp_task_queue_reset(queue);
      }
    }
    rxp_task_queue_unlock(&s->tasks);

    /* check if we need to stop, if so only execute tasks that are really necessary */
    should_stop = 0;
    task = work;
    while(task) {
      if (task->type == RXP_TASK_STOP) {
        should_stop = 1;
        break;
      }
      task = task->next;
    }

    /* when we need to stop, handle all important tasks first */
    if (should_stop) {
      task = work;
      while(task) {
        if (task->type == RXP_TASK_CLOSE_FILE) {
          rxp_scheduler_handle_task(s, task);
        }
        if (task->type == RXP_TASK_STOP) {

          rxp_scheduler_lock(s);
          s->state &= ~(RXP_SCHED_STATE_DECODING | RXP_SCHED_STATE_STARTED);
          rxp_scheduler_unlock(s);

          /* when we recieved a stop task, we clear our task queue and stop the thread */
          rxp_scheduler_handle_task(s, task);
          rxp_task_dealloc_all(work); 
        
          should_stop = 1;
        }
        task = task->next;
      }
      return;
    }

    /* when no task with more preference is found we perform the work */
    task = work;
    while (task) {
      rxp_scheduler_handle_task(s, task);
      task = task->next;
    }
    rxp_task_dealloc_all(work);

    /* reset decoding state */
    rxp_scheduler_lock(s);
      s->state &= ~RXP_SCHED_STATE_DECODING;
    rxp_scheduler_unlock(s);
  }
}

static void rxp_scheduler_handle_task(rxp_scheduler* s, rxp_task* task) {

  switch (task->type) {
    case RXP_TASK_DECODE: {
      if (s->decode) {
        /* decode up to the goal pts */
        s->decode(s, s->goal_pts);
      }
      break;
    }
    case RXP_TASK_OPEN_FILE: {
      if (s->open_file) {
        if (s->open_file(s, (char*)task->data) < 0) {
          printf("Error: cannot open the file. RXP_TASK_OPEN_FILE failed.\n");
        }   
      }
      break;
    }
    case RXP_TASK_CLOSE_FILE: {
      if (s->close_file) { 
        if (s->close_file(s) < 0) {
          printf("Error: cannot close the file. RXP_TASK_CLOSE_FILE failed.\n");
        }
      }
      break;
    }
    case RXP_TASK_PLAY: { 
      if (s->play) {
        if (s->play(s) < 0) {
          printf("Error: cannot play. RXP_TASK_PLAY failed.\n");
        }
      }
      break;
    }
    case RXP_TASK_STOP: {
      if (s->stop) {
        if (s->stop(s) < 0) {
          printf("Error: cannot handle the stop task. RXP_TASK_STOP failed.\n");
        }
      }
      break;
    };
    default: {
      printf("Error: scheduler cannot handle task: %d\n", task->type);
      break;
    }
  }
}

static int rxp_scheduler_update_goal_pts(rxp_scheduler* s, uint64_t pts) {

  uint64_t new_goal = 0;
  if (!s) { return -1; } 

  rxp_scheduler_lock(s);
  {
    new_goal = s->played_pts + pts;
    if (new_goal > s->goal_pts) {
      s->goal_pts = new_goal;
    }
  }
  rxp_scheduler_unlock(s);

  return 0;
}
