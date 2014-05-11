/* 

   rxp_clock
   ---------
   
   Basic timer that can be used both for a CPU timer or a audio based one. The
   CPU timer is just like any other timer and used by the ideo player when we're
   playing a video which doesn't contain an audio stream. 

   When a .ogg file contains an audio stream, the player uses the number of handled
   audio samples to synchronise the video stream. To calculate the correct time in 
   this case, which we call a RXP_CLOCK_AUDIO type, we need to know the samplerate,
   which is set using `rxp_clock_set_samplerate()`. 

   If you want to calculate the pts, when using an audio clock, you can use 
   `rxp_clock_calculate_audio_time()` by passing the total number of played samples.
   It will return the pts in nanoseconds. 

 */

#ifndef RXP_CLOCK_H
#define RXP_CLOCK_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <uv.h>

#define RXP_CLOCK_NONE 0
#define RXP_CLOCK_CPU 1
#define RXP_CLOCK_AUDIO 2

typedef struct rxp_clock rxp_clock;

struct rxp_clock {
  int type;                                                                   /* what kind of clock (cpu or audio sampler based) */
  uint64_t time_start;                                                        /* time when we started */
  uint64_t time_last;                                                         /* last time value */
  uint64_t time;                                                              /* duration since the clock started in nanos*/
  uint64_t samplerate;                                                        /* audio clock: the sample rate to determine the current time */
  uint64_t nsamples;                                                          /* audio clock: how many audio samples were played */
  uint64_t sample_time;                                                       /* audio clock: time in nano-seconds for one frame */
};

int rxp_clock_init(rxp_clock* clock);                                         /* initialize the members of the clock to defaults. */
int rxp_clock_start(rxp_clock* clock);                                        /* start the clock, after calling `rxp_clock_update()`, the time member will holdt the time in milliseconds */
int rxp_clock_stop(rxp_clock* clock);                                         /* stop the clock, resets everything */
int rxp_clock_update(rxp_clock* clock);                                       /* update, this will calculate the new `time` member value */
int rxp_clock_shutdown(rxp_clock* clock);                                     /* resets everything to a state as it was before init() */
int rxp_clock_set_samplerate(rxp_clock* clock, uint64_t samplerate);          /* when you set the samplerate, you make this clock a audio based clock, make sure to use */
uint64_t rxp_clock_calculate_audio_time(rxp_clock* clock, uint64_t samples);  /* based on the samplerate of the clock this function will return the timestamp for the given number of samples */
void rxp_clock_add_samples(rxp_clock* clock, uint32_t samples);               /* whenever you played some audio samples, call this with the number of audio samples you played so we know what current pts to use */

#endif
