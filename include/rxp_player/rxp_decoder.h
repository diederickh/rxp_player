/*

  rxp_decoder
  -----------
  
  The rxp_decoder takes care of demuxing an .ogg stream that contains
  theora and/or vorbis (at the time of writing only these stream types are 
  supported by in the future we will add support for daala and opus). The 
  general flow is showed in the `test_decoder.c` file. 

  Every time you call `rxp_decoder_decode()` we will decoder another batch
  of packets and make sure the set callbacks will be called. You can set a 
  callback that receives the decoded video buffer (yuv420p).

  You can set an event listener that is called whenever something worth notifying
  occurs. Note that if you use the rxp_decoder directly with the rxp_scheduler (or
  rxp_player), the callback may be called from another thread. 

  callbacks
  ----------

     on_theora_frame: this function will be called whenever we've decoded
                      a new theora frame. If you want to keep the buffer
                      in memory, you should copy it in your callback.


     on_event:        this will be called when certain events occur like
                      when we're ready with reading all packets. 
        

 */
#ifndef RXP_DECODER_H
#define RXP_DECODER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ogg/ogg.h>
#include <theora/theoradec.h>
#include <vorbis/codec.h>

typedef struct rxp_theora rxp_theora;
typedef struct rxp_vorbis rxp_vorbis;
typedef struct rxp_stream rxp_stream;
typedef struct rxp_decoder rxp_decoder;

typedef void (*decoder_event_callback)(rxp_decoder* decoder, int event);                           /* callback interface for decoder events. */ 
typedef void (*theora_frame_callback)(rxp_decoder* decoder, uint64_t pts, th_ycbcr_buffer buffer); /* callback that is called when decoding theora frames. */
typedef void (*decoder_audio_callback)(rxp_decoder* decoder, float** pcm, int nframes);            /* callback that is called whne the decoder decodes audio data (vorbis at the time of writing), pcm[0] contains the left channel, pcm[1] the right one (when multi channel audio is used) */

struct rxp_theora {
  th_info info;
  th_comment comment;
  th_setup_info* setup;
  th_dec_ctx* ctx;
};

struct rxp_vorbis {
  vorbis_info info;
  vorbis_comment comment;
  vorbis_dsp_state state;
  vorbis_block block;
  int num_header_packets;
};

struct rxp_stream {
  int64_t decoded_pts;                                                                             /* last decoded pts for this stream, we need to keep track of this, so a user of this code can decode up to a given goal pts as done in the rxp_player.*/
  uint64_t decoded_frames;                                                                         /* decoded video frames or audio samples, used to e.g. calculate the decoded_pts for audio */
  int eos;                                                                                         /* end of stream, is set to 1 when the stream ended */
  int serial;                                                                                      /* serial of the stream */
  int type;                                                                                        /* what kind of decoder type */      
  ogg_stream_state stream_state;                                                                   /* ogg stream state */
  rxp_theora* theora;                                                                              /* pointer to the theora decoder */
  rxp_stream* next;
};

struct rxp_decoder {
  rxp_vorbis vorbis;                                                                               /* the vorbis decoder members */
  rxp_theora theora;                                                                               /* the theora decoder members */
  rxp_stream* streams;                                                                             /* the streams in the ogg file */
  FILE* fp;                                                                                        /* at this moment we only support reading from a file */
  uint32_t file_size;                                                                              /* size of the file we're reading */
  ogg_sync_state sync_state;                                                                       /* used by ogg, state tracking */
  int state;                                                                                       /* the current state */
  uint64_t samplerate;                                                                             /* @todo: not sure if we need to store this here ... it's in player where we need it .. maybe pass it into callback (?) - when we find an audio stream, we set the samplerate and fire the RXP_DEC_EVENT_AUDIO_INFO event - UPDATE: I think it might be worth having here as we use it now (as experiment) to calculate the pts for the audio stream */
  int nchannels;                                                                                   /* @todo: not sure if we need to store this here .... "" "" - number of audio channels found */
  void* user;                                                                                      /* can be set to anything by the user */
  theora_frame_callback on_theora;                                                                 /* will be called when we've decoded a theora frame */
  decoder_audio_callback on_audio;                                                                 /* will be called when we've decoded some audio */
  decoder_event_callback on_event;                                                                 /* will be called when an event occurs (e.g. file closed.) */
};

rxp_stream* rxp_stream_alloc();                                                                    /* allocate a stream */
rxp_decoder* rxp_decoder_alloc();                                                                  /* allocate the decoder */
int rxp_decoder_init(rxp_decoder* decoder);                                                        /* initialize a decoder, returns < 0 on error after which you should dealloc if necessary */
int rxp_decoder_clear(rxp_decoder* decoder);                                                       /* free the allocated memory of the decoder */
int rxp_decoder_open_file(rxp_decoder* decoder, char* filepath);                                   /* open the video file */
int rxp_decoder_decode(rxp_decoder* decoder);                                                      /* decodes one frame */
int rxp_decoder_close_file(rxp_decoder* decoder);                                                  /* close file */

#endif
