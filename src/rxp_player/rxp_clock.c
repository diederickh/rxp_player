#include <rxp_player/rxp_clock.h>

int rxp_clock_init(rxp_clock* clock) {
  if (!clock) { return -1; } 
  clock->type = RXP_CLOCK_CPU;
  clock->time = 0;
  clock->time_last = 0;
  clock->time_start = 0;
  clock->nsamples = 0;
  clock->sample_time = 0;
  clock->samplerate = 0;
  return 0;
}

int rxp_clock_start(rxp_clock* clock) {
  if (!clock) { return - 1; } 
  clock->time_start = uv_hrtime();
  clock->time_last = clock->time_start;
  clock->time = 0;
  return 0;
}

int rxp_clock_stop(rxp_clock* clock) {
  if (!clock) { return -1; } 
  return rxp_clock_init(clock);
}

int rxp_clock_update(rxp_clock* clock) {
  if (!clock) { return -1; } 

  if (clock->type == RXP_CLOCK_CPU) {

    /* cpu based time */
    clock->time_last = uv_hrtime();
    clock->time = clock->time_last - clock->time_start;
  }
  else { 
    /* audio sample based on the nubmer of added samples */
    clock->time_last = clock->nsamples * clock->sample_time;
    clock->time = clock->time_last;
  }

  return 0;
}

/* just resets the clock, same as init, init<>shutdown seems ok api wise */
int rxp_clock_shutdown(rxp_clock* clock) {
  return rxp_clock_init(clock);
}

int rxp_clock_set_samplerate(rxp_clock* clock, uint64_t samplerate) {
  if (!clock) { return  -1; }

  /* init all members to defaults, before switching to AUDIO clock */
  rxp_clock_init(clock);

  /* we cannot use uv_hrtime() as used in rxp_clock_start() because we base
     the time on the number of player/added samples, therefore the current 
     time is calculated only based on the played samples times sample_time. */
  clock->samplerate = samplerate;
  clock->type = RXP_CLOCK_AUDIO;
  clock->sample_time = ((double)1.0 / (double)(samplerate)) * 1000llu * 1000llu * 1000llu;
  return 0;
}

void rxp_clock_add_samples(rxp_clock* clock, uint32_t samples) {
  clock->nsamples += samples;
}

uint64_t rxp_clock_calculate_audio_time(rxp_clock* clock, uint64_t samples) {
  return clock->sample_time * samples;
}
