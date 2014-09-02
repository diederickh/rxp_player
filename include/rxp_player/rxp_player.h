/* 
   rxp_player
   ----------

   The rxp_player wraps all the separate features together to create a video player. 
   Because we're using the scheduler which runs in a separate thread things become
   a bit more complicated because we need to synchornise tasks. Important is the
   way stop stop the player. When the user asks rxp_player_stop(), we have to 
   join the scheduler thread which handles the last important tasks, such as 
   closing the file (also handled by the scheduler), stopping the scheduler thread
   and resetting the player state.

   The flow goes something like: 

           The user calls rxp_player_stop(), which adds a stop task to the scheduler,
           which in turn will handle this as quickly as possible. When the scheduler
           finds this task, it cleans internally used memory, calls the close_file 
           callback if necessary and then calls the on_stop() callback. The on_stop()
           callback is handled by the player. When on_stop() is called, which is the 
           last step of the scheduler, we trigger the RXP_PLAYER_EVENT_RESET event
           which should be handled by the user. When the user receives this event, 
           he should stop the audio callback if they were used.

   rxp_player_fill_audio_buffer():

            The `rxp_player_fill_audio_buffer()` function must be called by the user
            when he wants to playback an .ogg file that contains an audio stream. This
            function should be called from the audio callback you get from your OS, or 
            from the library that you're using. The `buffer` you pass into this function
            must be big enough to whole the `nsamples` for all the channels that the 
            audio stream contains.

   rxp_player_event_callback():
 
            The event callback can be set, so the user is notified on certain events that
            we need to share. For example, when the decoder finds an audio stream and detects
            the samplerate/number of channels, we will fire an event so you can start an 
            output audio stream for your OS. Note that the event callback can be triggered
            from another thread, so ALWAYS LOCK THE PLAYER when you access it's members.
       

 */
#ifndef RXP_PLAYER_H
#define RXP_PLAYER_H

#include <uv.h>
#include <rxp_player/rxp_decoder.h>
#include <rxp_player/rxp_packets.h>
#include <rxp_player/rxp_ringbuffer.h>
#include <rxp_player/rxp_scheduler.h>
#include <rxp_player/rxp_clock.h>

typedef struct rxp_player rxp_player;

typedef void(*rxp_player_video_frame_callback)(rxp_player* player, rxp_packet* pkt);       /* is called when we you should draw a new video frame. */
typedef void(*rxp_player_event_callback)(rxp_player* player, int event);                   /* is called on certain events, e.g. when we detected an audio stream and the samplerate + number of channels is set, can be used to setup an audio stream for example */

struct rxp_player {

  rxp_clock clock;                                                                         /* at the time of writing the scheduler implements a clock but we need to remove that and use this one, see the TODO.txt */
  rxp_decoder decoder;                                                                     /* the decoder, which decodes the theora + vorbis stream, in the future other streams too */
  rxp_packet_queue packets;                                                                /* packets with decoded data */
  rxp_scheduler scheduler;                                                                 /* the scheduler, which makes sure the decoder performs the decoding work in a separate thread */
  rxp_ringbuffer audio_buffer;                                                             /* we use an ringbuffer to store decoded audio samples */
                                                                                      
  uv_mutex_t mutex;                                                                        /* mutex that is used to protect the player state */
  uint64_t last_used_pts;                                                                  /* the last used pts for the video packets, this is used to make sure we get the correct "next" video packet (= with higher pts then last_used_pts) */
  uint64_t samplerate;                                                                     /* the samplerate of the audio stream if found */
  uint64_t total_audio_frames;                                                             /* the total number of audio frames that we received from the decoder. we used this to tell the scheduler up till which pts we have decoded */
  int nchannels;                                                                           /* the number of audio channels of the audio stream when found */
  int state;                                                                               /* the player state */
  int must_stop;                                                                           /* this is set to 1 in the rxp_player_fill_audio_buffer() when there is no audio left to play back and we should stop playing. We cannot simply dealloc/clear/reset everything in the audio callback becuase that function is not allowed to take too much time */
  int is_init;                                                                            /* 1 = yes, -1 = no */ 

  /* callback */
  void* user;
  rxp_player_event_callback on_event;                                                      /* the event callback. */
  rxp_player_video_frame_callback on_video_frame;                                          /* is called when a new video frame needs to be shown */
};

int rxp_player_init(rxp_player* player);                                                   /* initialize all of the members */
int rxp_player_clear(rxp_player* player);                                                  /* frees all allocated memory and resets state to what it was before init() */
int rxp_player_open(rxp_player* player, char* file);                                       /* open a .ogg file */
int rxp_player_play(rxp_player* player);                                                   /* start playing. returns 0 on success. it's important to know that this will add a play task to the scheduler which will fire the play event only when it has decoded a couple of frames, so the playback will be smooth */
int rxp_player_pause(rxp_player* player);                                                  /* pause the player, returns < 0 on error, 0 on success, 1 when not playing */
int rxp_player_stop(rxp_player* player);                                                   /* stop playing, also stops the scheduler; basically resets all state to default, calling the appropriate callbacks and events. */
int rxp_player_lock(rxp_player* player);                                                   /* whenever you need to use any of the internal members make sure to lock/unlock the player. */
int rxp_player_unlock(rxp_player* player);                                                 /* unlock the player. */
int rxp_player_fill_audio_buffer(rxp_player* player, void* buffer, uint32_t nsamples);     /* the user should call this from their audio callback. buffer must be an pointer to an array that can be filled with the pcm that the decoder generates, nframes is the number of frames one wants to fill, we will return 0 on success, -1 when you need to stop your audio stream */
int rxp_player_is_playing(rxp_player* player);                                             /* returns 0 when the player is playing, else 1 or < 0 on error*/
int rxp_player_is_paused(rxp_player* player);                                              /* returns 0 when player is paused, else 1 or < 0 on error */
int rxp_player_get_state(rxp_player* player);                                              /* thread safe getting of the state */
int rxp_player_set_state(rxp_player* player, int state);                                   /* thread safe setting/unsetting of state flags. */
int rxp_player_unset_state(rxp_player* player, int state);                                 /* thread safe setting/unsetting of state flags. */  
int rxp_player_has_state(rxp_player* player, int state);                                   /* thread safe testing for a state; returns 0 when state is set otherwise < 0. */ 
void rxp_player_update(rxp_player* player);                                                /* you should call this regularly so we can call e.g. `on_video_frame()` callback when needed. */

#endif
