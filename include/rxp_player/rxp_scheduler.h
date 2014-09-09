/*

  rxp_scheduler
  --------------
  
  The rxp_scheduler runs a seperate thread in which it calls the set callbacks
  whenever the decoder needs to perform some action. The task of this scheduler
  is to make sure that the decoder keeps decoding up to the goal pts and that
  the renderer knows when it needs to display/output any video/audio. 

  The "correct pts" is based on some goal pts. This goal pts is updated when 
  necessary in by calling `rxp_scheduler_update()`.  The user of the scheduler 
  needs to make sure that it calls `rxp_scheduler_update()` regurlarly. 

  The goal pts is based on the last decoded pts + some extra time. We do this 
  to make sure that we're always N-seconds ahead with decoding frames so the 
  playback will be smooth. This 'extra time' is measured in nanoseconds and a 
  value of one second means that we will pre-decode from the last decoded pts 
  up to one second extra. When the current playback time is 1 second and we 
  the goal pts is 2 seconds, the decoder will be called until we have reached our 
  goal pts. The goal pts is a running value which means it's updated every time 
  you call rxp_scheduler_update().

  The scheduler needs to know when to stop decoding and if it needs to add more
  decoding tasks. For this we use a 'decoded_pts' which represents the pts up
  until which we decoded audio or video frames. We will only add more decode
  tasks as long as the decoded_pts hasn't reached the goal pts. The rxp_decoder, 
  calls a set callback when it decoded some audio or video with information
  about the pts. In this callback you'll want to call 
  `rxp_scheduler_update_decode_pts()`.

  All regular control over the playback of a video needs to go through the scheduler
  when you're using it. This means that tasks like open-file, close-file, play, stop
  etc.. are tasks that get pushed to the internal task queue and read/handled in 
  the thread. 

  An important task is the play task. When you want to play a file we first decode
  a couple of frames so we have enough data that can be send to the screen/soundcard.
  Only when we've decoded enough frames we will call the set play callback function. 

  ! IMPORTANT:
                It's important that the decoder updates the decoded_pts value
                by called rxp_scheduler_update_decode_pts() so the decoder knows
                that it shouldn't continue decoding when we've reached our goal.
       

  rxp_scheduler_update_played_pts()
                Every time you playback some video or audio frame, you need to 
                make sure that the scheduler knows about this, so it can make sure
                that we have decoded a small buffer with frames that will be played 
                next. 

 */
#ifndef RXP_SCHEDULER_H
#define RXP_SCHEDULER_H

#include <rxp_player/rxp_tasks.h>
#include <uv.h>

typedef struct rxp_scheduler rxp_scheduler;

typedef int(*rxp_scheduler_callback)(rxp_scheduler* s);                               /* generic callback */
typedef int(*rxp_scheduler_decode_callback)(rxp_scheduler* s, uint64_t goalpts);      /* gets called when you need to decode a new frame (audio or video). must return 0 on success and < 0 on error or when end of file has been reached. you need to decode all streams up to the given goal-pts*/
typedef int(*rxp_scheduler_open_file_callback)(rxp_scheduler* s, char* file);         /* gets called when you need to open the given file */

struct rxp_scheduler {
  rxp_task_queue tasks;                                                               /* the tasks that we need to handle in the thread */
  uint64_t goal_pts;                                                                  /* we will need to decode until we reached this pts */
  uint64_t decoded_pts;                                                               /* we've decoded up to this pts, see rxp_scheduler_update_decode_pts() */
  uint64_t played_pts;                                                                /* the highest value of the pts that we played (e.g. sent to the audio card or the last video frame) */
  int state;                                                                          /* we need to keep state because we don't want to add extra decoding tasks when we're already decoding */
  uv_mutex_t mutex;                                                                   /* mutex to protect the data of the scheduler */
  uv_thread_t thread;                                                                 /* handle to the thread in which we decode */
  int is_init;                                                                        /* is set to 1 when initialized, otherwise -1 */

  /* callbacks */
  void* user;                                                                         /* custom user data */
  rxp_scheduler_decode_callback decode;                                               /* is called when the user needs to decode one more video/audio frame, in the callback, make sure that you call `rxp_scheduler_update_decode_pts` so the scheduler knows how far decoding is */
  rxp_scheduler_open_file_callback open_file;                                         /* will be called from the thread when we're ready to open the file. */
  rxp_scheduler_callback close_file;                                                  /* will be called from the thread when the file needs to be closed. normally this should be done when we're ready decoding all data. */
  rxp_scheduler_callback stop;                                                        /* is called when the thread stops */
  rxp_scheduler_callback play;                                                        /* is called when we're ready with 'pre-buffering' some frames and we start playing. this is a good place to start your audio stream in case the .ogg file has audio */
};

rxp_scheduler* rxp_scheduler_alloc();                                                 /* allocate a rxp_scheduler on the heap. you can use this or create one on the stack and call `rxp_scheduler_init()`. when you allocate a scheduler with this function we will initialize if for you.*/                                     
int rxp_scheduler_reset(rxp_scheduler* s); /* only call once, before init; sets the initial members */
void rxp_scheduler_update(rxp_scheduler* s);                                          /* call this often, this will add new decode tasks when necessary */
int rxp_scheduler_init(rxp_scheduler* s);                                             /* init all members of the scheduler, call this if you created a scheduler on the stack. */
int rxp_scheduler_clear(rxp_scheduler* s);                                            /* clear all allocated memory and reset the state. if you want to reuse the scheduler again, make sure to call `rxp_scheduler_init` again. */
int rxp_scheduler_start(rxp_scheduler* s);                                            /* starts the scheduler thread */
int rxp_scheduler_play(rxp_scheduler* s);                                             /* start decoding and the playback */
int rxp_scheduler_stop(rxp_scheduler* s);                                             /* stop decoding and/or calling the callbacks */
int rxp_scheduler_open_file(rxp_scheduler* s, char* file);                            /* adds a task to open the file (will result in a call to the open_file callback from the thread) */
int rxp_scheduler_close_file(rxp_scheduler* s);                                       /* will call the close_file callback from the thread */
int rxp_scheduler_update_decode_pts(rxp_scheduler* s, uint64_t pts);                  /* the user must call this function whenever it decoded a video/audio frame to let us know if we need to decode some more frames (to make sure that we always have some decoded frames... aka pre-buffering) */
int rxp_scheduler_update_played_pts(rxp_scheduler* s, uint64_t pts);                  /* whenever an audio sample or video frame is send to the output (screen/soundcard) you need to tell the scheduler the pts of the last outputted data so we know if we need to decode some more frames. */

#endif
