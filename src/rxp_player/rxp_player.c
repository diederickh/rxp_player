#include <string.h>
#include <rxp_player/rxp_player.h>

/* ---------------------------------------------------------------- */

static int rxp_player_on_open_file(rxp_scheduler* scheduler, char* file);                           /* is called when the scheduler is handling the open file task. */
static int rxp_player_on_close_file(rxp_scheduler* scheduler);                                      /* is called when the scheduler is handling the close file task. */
static int rxp_player_on_stop(rxp_scheduler* scheduler);                                            /* is called when the scheduler thread stopped. */
static int rxp_player_on_play(rxp_scheduler* scheduler);                                            /* is called by the scheduler when it handles a play task. */
static void rxp_player_on_decoder_event(rxp_decoder* decoder, int event);                           /* is called by the decoder when something "special" happens. */
static int rxp_player_on_decode(rxp_scheduler* scheduler, uint64_t goalpts);                        /* is called by the scheduler when we need to decode a frame (audio and/or video). */
static void rxp_player_on_theora_frame(rxp_decoder* decoder, uint64_t pts, th_ycbcr_buffer buffer); /* is called by the decoder when it decoded a theora frame */
static void rxp_player_on_audio(rxp_decoder* decoder, float** pcm, int nsamples);                   /* is called by the decoder when it decoded some audio samples */
static void rxp_player_reset(rxp_player* player);                                                   /* when we're ready playing all video/audio packets this cleans up internal state */

/* ---------------------------------------------------------------- */

int rxp_player_init(rxp_player* player) {
  
  if (!player) { return -1; } 

  if (uv_mutex_init(&player->mutex) != 0) {
    printf("Error: cannot initialize the mutex for the player.\n");
    return -2;
  }

  if (rxp_decoder_init(&player->decoder) < 0) {
    return -3;
  }

  if (rxp_packet_queue_init(&player->packets) < 0) {
    return -4;
  }

  if (rxp_scheduler_init(&player->scheduler) < 0) {
    return -5;
  }

  if (rxp_scheduler_start(&player->scheduler) < 0) {
    return -6;
  }

  if (rxp_clock_init(&player->clock) < 0) {
    return -7;
  }

  if (rxp_ringbuffer_reset(&player->audio_buffer) < 0) {
    return -8;
  }

  player->decoder.user = player;
  player->decoder.on_theora = rxp_player_on_theora_frame;
  player->decoder.on_audio = rxp_player_on_audio;
  player->decoder.on_event = rxp_player_on_decoder_event;
  player->scheduler.user = player;
  player->scheduler.open_file = rxp_player_on_open_file;
  player->scheduler.close_file = rxp_player_on_close_file;
  player->scheduler.stop = rxp_player_on_stop;
  player->scheduler.play = rxp_player_on_play;
  player->scheduler.decode = rxp_player_on_decode;
  player->last_used_pts = 0;
  player->total_audio_frames = 0;
  player->samplerate = 0;
  player->nchannels = 0;
  player->must_stop = 0;
  player->on_video_frame = NULL;
  player->on_event = NULL;
  player->state = RXP_PSTATE_NONE;
  player->is_init = 1;

  return 0;
}

int rxp_player_clear(rxp_player* player) {

  int r = 0;

  /* @todo - how to do we handle situations where the user calls 
             rxp_player_clear but e.g. the scheduler is still playing? do 
             we stop it and join the thread? */
  if (!player) { return -1; } 

  if (player->is_init != 1) {
    printf("NOT INITIALIZED\n");
  }

  if (player->state != RXP_PSTATE_NONE) {
    printf("Warning: you're trying to clear a player which states has still some meaning: %d\n", player->state);
  }

  if (rxp_packet_queue_dealloc(&player->packets) < 0) {
    printf("Error: cannot deallocate the allocated video packets of the player.\n");
    return -2;
  }
  
  if (rxp_scheduler_clear(&player->scheduler) < 0) {
    printf("Error: the player cannot dealloc the scheduler.\n");
    return -3;
  }
  
  if (rxp_decoder_clear(&player->decoder) < 0) {
    printf("Error: the player cannot dealloc the decoder.\n");
    return -4;
  }

  if (player->packets.packets) { 
    if (rxp_packet_queue_dealloc(&player->packets) < 0) {
      printf("Error: the player cannot deallocate the packet queue.\n");
      return -5;
    }
  }

  rxp_clock_stop(&player->clock);

  if (rxp_clock_shutdown(&player->clock) < 0) {
    printf("Error: cannot shutdown the clock.\n");
    return -6;
  }
  
  if (player->audio_buffer.capacity > 0) {
    printf("info: clearing the audio ringbuffer.\n");
    if (rxp_ringbuffer_clear(&player->audio_buffer) < 0) {
      printf("Error: cannot free the audio buffer.\n");
      return -7;
    }
  }

  uv_mutex_destroy(&player->mutex);

  player->last_used_pts = 0;
  player->total_audio_frames = 0;
  player->samplerate = 0;
  player->nchannels = 0;
  player->must_stop = 0;
  player->state = RXP_PSTATE_NONE;
  player->user = NULL;
  player->on_video_frame = NULL;
  player->on_event = NULL;
  player->is_init = 0;

  return 0;
}

int rxp_player_open(rxp_player* player, char* file) {
  if (!player) { return -1; } 
  if (!file) { return -2; }

  /* get current state */
  if (player->state != RXP_PSTATE_NONE) {
    printf("Error: cannot open ogg file becasue player has state: %d.\n", player->state);
    return -3;
  }

  return rxp_scheduler_open_file(&player->scheduler, file);
}

int rxp_player_play(rxp_player* player) {

  int state = 0;

  if (!player) { return -1; } 

  /* get current state */
  rxp_player_lock(player);
  state = player->state;
  rxp_player_unlock(player);

  if (state & RXP_PSTATE_PAUSED) {
    /* unset pause, reset play */
    rxp_player_lock(player);
    player->state &= ~RXP_PSTATE_PAUSED;
    player->state |= RXP_PSTATE_PLAYING;
    rxp_player_unlock(player);
    return 0;
  }
  else if (state & RXP_PSTATE_PLAYING) {
    printf("Warning: trying to set player state to play, but already playing.\n");
    return -3;
  }
  else {
    /* when everything is okay, start playing */
    rxp_player_lock(player);
    player->state = RXP_PSTATE_PLAYING;
    rxp_player_unlock(player);
  }

  if (rxp_clock_start(&player->clock) < 0) {
    return -2;
  }

  return rxp_scheduler_play(&player->scheduler);
}

int rxp_player_stop(rxp_player* player) {

  int r = 0;
  int least_state = (RXP_PSTATE_PLAYING | RXP_PSTATE_PAUSED);

  if (!player) { return -1; } 

  /* check if we're in the correct state */
  rxp_player_lock(player);
  {
    if (!(player->state & least_state)) {
      printf("Warning: trying to stop the player, but we're not playing.\n");
      r = -1;
    }
    else {
      player->state = RXP_PSTATE_NONE;
    }
  }
  rxp_player_unlock(player);

  if (r < 0) {
    printf("Error: cannot change state.\n");
    return r;
  }

  if (player->decoder.fp != NULL) {
    /* when file is already open, we need to close it first */
    if(rxp_scheduler_close_file(&player->scheduler) < 0) {
      printf("Erorr: failed to close the file through the scheduler.\n");
      return -2;
    }
  }

  return rxp_scheduler_stop(&player->scheduler);
}

int rxp_player_pause(rxp_player* player) {

  int r = 0;
   
  if (!player) { return -1; } 

  rxp_player_lock(player);
  {
    if (player->state != RXP_PSTATE_PLAYING) {
      printf("Warning: trying to pause the player, but we're not playing.\n");
      r = -1;
    }
    else {
      player->state |= RXP_PSTATE_PAUSED;
      player->state &= ~RXP_PSTATE_PLAYING;
    }
  }
  rxp_player_unlock(player);

  if (r < 0) {
    return r;
  }

  return 0;
}

void rxp_player_update(rxp_player* player) {

  uint64_t curr_time = 0;
  rxp_packet* video_pkt = NULL;
  int state = player->state;

  /* do we need to reset / clear? (by audio callback) */
  if(player->must_stop) {
    rxp_player_stop(player);
    return;
  }

  /* when we're not playing, don't do anything */
  if ( !(state & RXP_PSTATE_PLAYING) ) {
    return ;
  }

  /* update the current clock/time */
  rxp_clock_update(&player->clock);
  curr_time = player->clock.time; 

  /* do we have packets that need to be displayed. */
  rxp_packet_queue_lock(&player->packets);
  {

    /* not correct logic here ... we will probably skip the first frames */
    rxp_packet* tail = player->packets.packets;

    while (tail) {

      if (tail->pts <= player->last_used_pts) {
        /* @todo: this shouldn't actually happen.. these frames can/should be removed directly */
        tail->is_free = 1;
        tail = tail->next;
        continue;
      }
 
      if (tail->next && tail->pts <= curr_time && tail->next->pts > curr_time) {
        video_pkt = tail;
        player->last_used_pts = tail->pts;
        break;
      }
      else if( (state & RXP_PSTATE_DECODE_READY) && tail->pts <= curr_time) {
        video_pkt = tail;
        player->last_used_pts = tail->pts;
        break;
      }
      tail = tail->next;
    }
  }
  rxp_packet_queue_unlock(&player->packets);

  /* @todo: should we lock here ? -> the video packet won't be used because it's not free. */
  if (video_pkt && player->on_video_frame) {

    player->on_video_frame(player, video_pkt);
    
    rxp_scheduler_update_played_pts(&player->scheduler, video_pkt->pts);

    /* this is where we cleanup everything when we've reached the last packet. */
    /* @todo: make an is_decode_ready() function? */
    if(state & RXP_PSTATE_DECODE_READY 
       && video_pkt->pts >= player->scheduler.decoded_pts)
    {
      player->must_stop = 1;
      return;
    }
  }

  if (state & RXP_PSTATE_PLAYING) {
    rxp_scheduler_update(&player->scheduler);
  }
}

int rxp_player_lock(rxp_player* player) {
  if (!player) { return -1; } 
  uv_mutex_lock(&player->mutex);
  return 0;
}

int rxp_player_unlock(rxp_player* player) {
  if (!player) { return -1; } 
  uv_mutex_unlock(&player->mutex);
  return 0;
}

/* called by user whenever the OS-audio callback is called, 
   we fill the given buffer but don't allocate it. We return 
   0 on success and < 0 on failure or when there is no decoded
   audio left in the buffer..*/
int rxp_player_fill_audio_buffer(rxp_player* player, 
                                 void* buffer, 
                                 uint32_t nsamples) 
{
  int r = 0;
  uint32_t bytes_needed = 0;

  rxp_player_lock(player);
  {
    if (player->state & RXP_PSTATE_PLAYING) {
      /* read audio */
      bytes_needed = nsamples * sizeof(float) * player->nchannels;
      rxp_clock_add_samples(&player->clock, nsamples);

      if (rxp_ringbuffer_read(&player->audio_buffer, buffer, bytes_needed) < 0) {
        memset(buffer, 0x00, bytes_needed);     
        player->must_stop = 1;
        r = -1;
      }
    }
    else {
      /* just filling the audio with silence ... */
      memset(buffer, 0x00, sizeof(float) * player->nchannels * nsamples);
    }
  }
  rxp_player_unlock(player);

  /*
    When r < 0, it means the audio buffer has ran out of bytes; 
     this only happens when we reach the end. In this case we need 
     to reset the player. 

     UPDATE: 
     We cannot call rxp_player_reset() here because it will result
     in these crashes: https://gist.github.com/roxlu/20720d4ac90976b4ca82, 
     these crashes may happen because it takes too long to reset the 
     player (dealloc mem of queue) ... but I'm not entirely sure. 

     This solved by adding a must_stop member to the player which is 
     handled in rxp_player_update(), which is also called from the 
     main thread, which feels more safe :) 

     I've left the code below so I can investigate this further at some
     point. Would love to validate if my thoughts are right..
     
  */
#if 0
  if (r < 0) {
      rxp_player_reset(player);
  }
#endif

  return r;
}

/* returns 0 when playing, else 1, or < 0 on error */
int rxp_player_is_playing(rxp_player* p) {
  if (!p) { return -1; }
  return (p->state & RXP_PSTATE_PLAYING) ? 0 : 1;
}

/* returns 0 when paused, else 1, or < 0 on error */
int rxp_player_is_paused(rxp_player* p) {
  if (!p) { return -1; }
  return (p->state & RXP_PSTATE_PAUSED) ? 0 : 1;
}

/* ---------------------------------------------------------------- */

static int rxp_player_on_open_file(rxp_scheduler* scheduler, char* file) {
  rxp_player* p = (rxp_player*) scheduler->user;
  return rxp_decoder_open_file(&p->decoder, file);
}

/* when we are asked to close the file (by the scheduler) it means we're ready 
   with decoding everything and our next step is to close the file and stop the 
   scheduler */
static int rxp_player_on_close_file(rxp_scheduler* scheduler) {

  rxp_player* p = (rxp_player*) scheduler->user;
  if (rxp_decoder_close_file(&p->decoder) < 0) {
    printf("Error: cannot close the file in rxp_player_close_file.\n");
    return -1;
  }

  return 0;
}

static int rxp_player_on_play(rxp_scheduler* scheduler) {
  rxp_player* p = (rxp_player*) scheduler->user;
  if (p->on_event) {
    p->on_event(p, RXP_PLAYER_EVENT_PLAY);
  }
  return 0;
}

static int rxp_player_on_stop(rxp_scheduler* scheduler) {
  rxp_player* p = (rxp_player*) scheduler->user;
  rxp_player_reset(p);
  return 0;
}

/* this function is called by the scheduler and we need to decode all 
   the streams up to the given goal pts. 
*/
static int rxp_player_on_decode(rxp_scheduler* scheduler, uint64_t goalpts) { 

  rxp_player* p = (rxp_player*) scheduler->user;
  rxp_decoder* decoder = &p->decoder;
  rxp_stream* stream = NULL;
  int r = 0;
  int did_reach_goal = 0;
  int has_valid_streams = 0;

  do {

    r = rxp_decoder_decode(decoder);

    /* check if all streams reached the goal pts */
    did_reach_goal = 1;
    stream = decoder->streams;

    while(stream) {

      /* unsupported stream, or already ended? -> skip */
      if(stream->type == RXP_NONE || stream->eos) {
        stream = stream->next;
        continue;
      }
      
      /* when the stream decoded_pts hasn't reached the goal yet, we continue decoding */
      has_valid_streams = 1;
      if (stream->decoded_pts <= goalpts) {
        did_reach_goal = 0;
        break;
      }
      stream = stream->next;
    }
    
    /* did all streams reach the goal pts? */
    if (did_reach_goal && has_valid_streams) {
      break;
    }

  } while (r == 0);

  return r;
}

static void rxp_player_on_theora_frame(rxp_decoder* decoder, uint64_t pts, th_ycbcr_buffer buffer) {
  
  int y_bytes, u_bytes, v_bytes, nbytes;
  rxp_player* p = (rxp_player*) decoder->user;
  rxp_packet* pkt = rxp_packet_queue_find_free_packet(&p->packets);
  
  /* allocate a new packet when no free one was found. */
  if (!pkt) {
    pkt = rxp_packet_alloc();

    if (!pkt) {
      printf("Error: cannot allocate a new rxp_packet.\n");
      exit(1);
    }
  }

  y_bytes = buffer[0].stride * buffer[0].height;
  u_bytes = buffer[1].stride * buffer[1].height;
  v_bytes = buffer[2].stride * buffer[2].height;

  nbytes = y_bytes + u_bytes + v_bytes;

  /* @todo: we should check if the nbytes is somewhat valid. */
  if (pkt->size < nbytes && !pkt->data) {

    pkt->data = (void*)malloc(nbytes);
    if (!pkt->data) {
      printf("Error: cannot allocate data for the packet.\n");
      exit(1);
    }

    if (rxp_packet_queue_add(&p->packets, pkt) < 0) {
      printf("Error: cannot add the new packet to the queue.\n");
      exit(1);
    }
  }
  else {
    if (nbytes > pkt->size) {
      printf("@todo: we need to either realloc, or make sure we have a ringbuffer with enough space.\n");
    }
  }
  
  memcpy(pkt->data, buffer[0].data, y_bytes);
  memcpy(pkt->data + y_bytes, buffer[1].data, u_bytes);
  memcpy(pkt->data + y_bytes + u_bytes, buffer[2].data, v_bytes);

  pkt->type = RXP_YUV420P;
  pkt->pts = pts;
  pkt->size = nbytes;
  pkt->is_free = 0;
  
  pkt->img[0].width = buffer[0].width;
  pkt->img[0].height = buffer[0].height;
  pkt->img[0].stride = buffer[0].stride;
  pkt->img[0].data = pkt->data;

  pkt->img[1].width = buffer[1].width;
  pkt->img[1].height = buffer[1].height;
  pkt->img[1].stride = buffer[1].stride;
  pkt->img[1].data = pkt->img[0].data + (pkt->img[0].stride * pkt->img[0].height);

  pkt->img[2].width = buffer[2].width;
  pkt->img[2].height = buffer[2].height;
  pkt->img[2].stride = buffer[2].stride;
  pkt->img[2].data = pkt->img[1].data + (pkt->img[1].stride * pkt->img[1].height);

  /* tell the scheduler what pts we decoded. */
  rxp_scheduler_update_decode_pts(&p->scheduler, pts);
}

/* called when the decoder decoded some audio samples */
static void rxp_player_on_audio(rxp_decoder* decoder, float** pcm, int nframes) {

  static float tmp[4096] = { 0 } ;
  uint64_t pts  = 0;
  rxp_player* player = (rxp_player*)decoder->user;
  int dx = 0;
  int i,c;

  int needed = player->nchannels * nframes;

  if (needed > 4096) {
    printf("Error: our current tmp buffer is not big enough ...\n");
    exit(0);
  }

  /* reformat the channels, the incoming data is not interleaved
     which we will do here. */
  for (i = 0; i < nframes; ++i) {
    for (c = 0; c < player->nchannels; ++c) {
      tmp[dx++] = pcm[c][i]; 
    }
  }
 
  rxp_player_lock(player);
  {
    player->total_audio_frames += nframes;
    pts = rxp_clock_calculate_audio_time(&player->clock, player->total_audio_frames);
    rxp_ringbuffer_write(&player->audio_buffer, tmp, needed * sizeof(float));
  }
  rxp_player_unlock(player);
  
  /* we need to tell the scheduler up till what pts we decoded */
  rxp_scheduler_update_decode_pts(&player->scheduler, pts);
}

static void rxp_player_on_decoder_event(rxp_decoder* decoder, int event) {

  rxp_player* player = (rxp_player*)decoder->user;

  /* make sure update the state */
  if (event == RXP_DEC_EVENT_READY) { 

    rxp_player_lock(player);
      player->state |= RXP_PSTATE_DECODE_READY;
    rxp_player_unlock(player);
    
    if (rxp_scheduler_close_file(&player->scheduler) < 0) {
      printf("Error: cannot stop the scheduler.\n");
    }
    
  }
  else if (event == RXP_DEC_EVENT_AUDIO_INFO) {

    /* Make sure we're going to use the audio clock by setting the samplerate */
    rxp_player_lock(player);
    {
      player->samplerate = decoder->samplerate;
      player->nchannels = decoder->nchannels;
      rxp_clock_set_samplerate(&player->clock, decoder->samplerate);
      rxp_ringbuffer_init(&player->audio_buffer, 1024 * 1024 * 5); 
    }
    rxp_player_unlock(player);

#if !defined(NDEBUG)
    printf("Info: The video file has audio, so make sure to call rx_player_filll_audio_buffer which is also used for timings; w/o your video will not play!\n");
#endif
    
  }

  /* dispatch the event to a external, user defined listener */
  if (player->on_event) {
    player->on_event(player, event);
  }
}

/* rxp_player_reset() is called when we finished playing the video, 
   or when the video had to stop because of a call to rxp_player_stop(). */
static void rxp_player_reset(rxp_player* player) {

  rxp_player_lock(player);
  {
    player->state = RXP_PSTATE_NONE;
    rxp_clock_stop(&player->clock);
  }
  rxp_player_unlock(player);

  if (player->on_event) {
    player->on_event(player, RXP_PLAYER_EVENT_RESET);
  }

  player->must_stop = 0;
}
