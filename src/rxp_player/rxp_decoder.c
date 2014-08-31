#include <stdlib.h>
#include <stdio.h>
#include <string.h> 
#include <rxp_player/rxp_decoder.h>
#include <rxp_player/rxp_types.h>

/* ---------------------------------------------------------------- */

static int rxp_theora_init();                             
static int rxp_vorbis_init();
static int rxp_decoder_read_oggpage(rxp_decoder* decoder, ogg_page* page);
static int rxp_decoder_find_stream(rxp_decoder* decoder, ogg_page* page, rxp_stream** stream); /* stream is set to the stream which was previously created or to one which is allocated */
static int rxp_decoder_detect_stream_type(rxp_decoder* decoder, rxp_stream* stream, ogg_page* page, ogg_packet* packet);
static int rxp_decoder_add_stream(rxp_decoder* decoder, rxp_stream* stream);
static int rxp_decoder_decode_theora(rxp_decoder* decoder, rxp_stream* stream, ogg_packet* packet);
static int rxp_decoder_decode_vorbis(rxp_decoder* decoder, rxp_stream* stream, ogg_packet* packet);
static int rxp_decoder_trigger_event(rxp_decoder* decoder, int event);
static char* rxp_decoder_theora_error_to_string(int err);
static int rxp_decoder_set_audio_info(rxp_decoder* decoder, uint64_t samplerate, int nchannels); /* when an audio stream is found this function should be called with the audio info */

/* ---------------------------------------------------------------- */

rxp_decoder* rxp_decoder_alloc() {

  rxp_decoder* d = (rxp_decoder*)malloc(sizeof(rxp_decoder));
  if (!d) {
    printf("Error: cannot allocate the decoder.\n");
    return NULL;
  }

  if (rxp_decoder_init(d) < 0) {
    return NULL;
  }

  return d;
}

int rxp_decoder_init(rxp_decoder* d) {
  
  if (!d) { return -1; } 
  
  if (rxp_theora_init(&d->theora) < 0) {
    printf("Error: cannot initialize theora decoder.\n");
    return -2;
  }

  if (rxp_vorbis_init(&d->vorbis) < 0) {
    printf("Error: cannot initialize the vorbis decoder.\n");
    return -3;
  }

  if (ogg_sync_init(&d->sync_state) != 0) {
    printf("Error: cannot initialize the ogg sync state.\n");
    return -4;
  }

  d->streams = NULL;
  d->user = NULL;
  d->on_audio = NULL;
  d->on_theora = NULL;
  d->on_event = NULL;
  d->state = RXP_NONE;          
  d->samplerate = 0;
  d->nchannels = 0;

  return 0;
}

int rxp_decoder_clear(rxp_decoder* d) {

  /* clear ogg */
  rxp_stream* stream = NULL;
  rxp_stream* next_stream = NULL;

  if (!d) { return -1; } 

  stream = d->streams;

  while (stream) {
    next_stream = stream->next;
    ogg_stream_clear(&stream->stream_state);
    free(stream);
    stream = next_stream;
  }

  ogg_sync_clear(&d->sync_state);
  
  /* clear theora related */
  if (d->theora.ctx) { 
    th_setup_free(d->theora.setup);
    th_decode_free(d->theora.ctx);
    d->theora.ctx = NULL;
    d->theora.setup = NULL;
  }

  th_comment_clear(&d->theora.comment);
  th_info_clear(&d->theora.info);

  /* clear vorbis related */
  vorbis_block_clear(&d->vorbis.block);
  vorbis_dsp_clear(&d->vorbis.state);
  vorbis_comment_clear(&d->vorbis.comment);
  vorbis_info_clear(&d->vorbis.info);

  d->streams = NULL;
  d->state = RXP_NONE;

  return 0;
}

rxp_stream* rxp_stream_alloc() {

  rxp_stream* s = (rxp_stream*)malloc(sizeof(rxp_stream));
  if (!s) {
    printf("Error: cannot allocate the rxp_stream.\n");
    return NULL;
  }
  
  s->decoded_frames = 0;
  s->decoded_pts = 0;
  s->eos = 0;
  s->serial = -1;
  s->type = RXP_NONE;
  s->theora = NULL;
  s->next = NULL;

  return s;
}

int rxp_decoder_open_file(rxp_decoder* decoder, char* filepath) {
  
  long size = 0;

  if (!decoder) { return -1; } 
  if (!filepath) { return -2; } 

  /* try to open the file */
  decoder->fp = fopen(filepath, "rb");
  if (!decoder->fp) { 
    printf("Error: cannot read file: %s\n", filepath);
    return -3;
  }

  /* find size */
  fseek(decoder->fp, 0, SEEK_END);
  size = ftell(decoder->fp);
  fseek(decoder->fp, 0, SEEK_SET);

  if (size <= 0) {
    printf("Error: the file is empty.\n");
    return -4;
  }

  decoder->file_size = size;
  decoder->state |= RXP_DEC_STATE_DECODING;

  return 0;
}

int rxp_decoder_close_file(rxp_decoder* decoder) {

  if (!decoder) { return -1; } 
  if (!decoder->fp) { return -2; } 

  if (fclose(decoder->fp) != 0) {
    printf("Error: fclose() failed in rxp_decoder_close_file(). \n");
    return -3;
  }

  decoder->fp = NULL;

  decoder->state |= RXP_DEC_STATE_READY;

  return 0;
}

int rxp_decoder_decode(rxp_decoder* decoder) {

  /* retrieve an ogg page */
  int r = 0;
  ogg_page page;
  ogg_packet packet;
  rxp_stream* stream = NULL;

  if (!decoder) { return -1; }
  if (!decoder->fp) { return -2; } 

  /* reaad an page */
  if (rxp_decoder_read_oggpage(decoder, &page) < 0) {
    printf("Error: cannot read next ogg_page, in rxp_decoder_decode().\n");
    return -3;
  }

  /* find the stream of create a new one */
  if (rxp_decoder_find_stream(decoder, &page, &stream) < 0) {
    printf("Error: cannot find stream, in rxp_decoder_decode().\n");
    return -4;
  }

  /* feed the page for this stream */
  r = ogg_stream_pagein(&stream->stream_state, &page);
  if(r != 0) {
    printf("Error: cannot feed a page into the stream: %d\n", stream->serial);
    return -5;
  }

  r = ogg_stream_packetout(&stream->stream_state, &packet);
  if (r != 1) {
    /* @todo - not 100% what to do here, when r != 1 it means that there
               is a gap in the data (r == -1) or we need to read a bit more
               from the file (r == 0). */
    return r;
  }

  /* when we don't know the type of the stream try to detect it */
  if (stream->type == RXP_NONE) {
    if (rxp_decoder_detect_stream_type(decoder, stream, &page, &packet) < 0) {
#if !defined(NDEBUG)
      printf("Warning: Cannot find stream type\n");
#endif
      return 0; /* < 0 means we should fail.. we simply ignore this stream by returning 0 (= means everything is still ok).*/
    }
  }

  /* decode as many packets as possible (need to do this in a loop, else you'll leak memory) */
  do {
    if (stream->type == RXP_VORBIS) {
      rxp_decoder_decode_vorbis(decoder, stream, &packet);
    }
    else if (stream->type == RXP_THEORA) {
      rxp_decoder_decode_theora(decoder, stream, &packet);
    }

    else {
      printf("Error: unknown stream type.\n");
      return -8;
    }
  } while (ogg_stream_packetout(&stream->stream_state, &packet) == 1);

  return 0;
}

/* ---------------------------------------------------------------- */

static int rxp_theora_init(rxp_theora* t) {

  if (!t) { return -1; }

  t->setup = NULL;
  t->ctx = NULL;

  th_comment_init(&t->comment); 
  th_info_init(&t->info);       
 
  return 0;
}

static int rxp_vorbis_init(rxp_vorbis* v) {

  if (!v) { return -1; } 
  
  v->num_header_packets = 0;

  vorbis_info_init(&v->info);
  vorbis_comment_init(&v->comment);

  return 0;
}

/* returns < 0, on error or when we reached the end of the file */
static int rxp_decoder_read_oggpage(rxp_decoder* decoder, ogg_page* page) {

  static size_t ntotal = 0;
  int r = 0;
  size_t read = 0;

  if (decoder->state & RXP_DEC_STATE_READY) { 
    printf("Error: cannot read ogg page because state is invalid, in rxp_decoder_read_oggpage().\n");
    return -1;
  }

  if (!decoder->fp) {
    printf("Error: cannot read an oggpage, file hasn't been opened.\n");
    return -2;
  }

  while (ogg_sync_pageout(&decoder->sync_state, page) != 1) {

    /* obtain some memory that we can use to store the file data */
    char* buffer = ogg_sync_buffer(&decoder->sync_state, 4096);
    if (!buffer) {
      printf("Error: ogg_sync_buffer returned an invalid buffer; non mem?\n");
      return -3;
    }

    /* read a chunk of data */
    read = fread(buffer, 1, 4096, decoder->fp); 
    if (read == 0) {

      decoder->state |= RXP_DEC_STATE_READY;
      decoder->state &= ~RXP_DEC_STATE_DECODING;

      if (rxp_decoder_trigger_event(decoder, RXP_DEC_EVENT_READY) < 0) {
        printf("Error: something went wrong when triggering a decoder event.\n");
        return -1;
      }

      return -4;
    }

    ntotal += read;
    r = ogg_sync_wrote(&decoder->sync_state, read);
    if (r != 0) {
      printf("Error: ogg_sync_wrote() failed.\n");
      return -5;
    }
  }

  return 0;
}

static int rxp_decoder_decode_theora(rxp_decoder* decoder, rxp_stream* stream, ogg_packet* packet) {

  ogg_int64_t granulepos = -1;
  th_ycbcr_buffer buffer;
  int r = -1;
  rxp_theora* theora = &decoder->theora;
  double time_s;

  if(theora->ctx == NULL) {

    /* parse all header packets; returns 0 when ready. */
    r = th_decode_headerin(&theora->info, &theora->comment, &theora->setup, packet);
    if (r != 0) {
      return 0;
    }

    /* allocate the theora context */
    theora->ctx = th_decode_alloc(&theora->info, theora->setup);
    if (theora->ctx == NULL) {
      printf("Error: cannot allocate the theora decoder context.\n");
      exit(1);
    }
    
    return 0;
  }

  /* decoder packet */
  r = th_decode_packetin(theora->ctx, packet, &granulepos);
  if (r != 0) {
    /* we must skip a bad packet. */
    if (r == TH_EBADPACKET) { 
      return 0;
    }
    else if (r == TH_DUPFRAME) {
      return 0;
    }
    printf("Error: cannot decode the theora packet: %d = %s\n", r, rxp_decoder_theora_error_to_string(r));
    return 0;
  }

  if (packet->granulepos >= 0) {
    /* @todo: this must be set after a seek or gap, but this would mean we have to 
              back-track from the last packet on the page and compute the correct
              granulepos for the first packet after a seek or gap. We're just setting
              it everytime we get a granulepos and that sees to work fine. 
              reference: https://svn.xiph.org/trunk/theora/examples/player_example.c

              This fixes out of sync videos. You can test this by extracting e.g.
              50 seconds using avconv of the Blender Sintel.ogv movie and playback
              w/o this, you'll it's about 2.5 seconds out of sync.
    */
    th_decode_ctl(theora->ctx, TH_DECCTL_SET_GRANPOS, 
                  &packet->granulepos, sizeof(packet->granulepos));

  }

  /* retrieve the yuv data */
  r = th_decode_ycbcr_out(theora->ctx, buffer);
  if (r != 0) {
    printf("Error: cannot decode the theora data: %d\n", r);
    exit(1);
  }

  time_s = (double)(th_granule_time(theora->ctx, granulepos)) ;
  //stream->decoded_pts = time_s * 1000ull * 1000ull * 1000ull;
  stream->decoded_pts = time_s * 1e9;

  if (decoder->on_theora) {
    decoder->on_theora(decoder, stream->decoded_pts, buffer);
  }

  return 0;
}

static int rxp_decoder_decode_vorbis(rxp_decoder* decoder, rxp_stream* stream, ogg_packet* packet) {

  int samples = 0;
  int r = 0;
  rxp_vorbis* v = &decoder->vorbis;
  float** pcm; 

  if (v->num_header_packets < 3) {

    /* until we haven't decode the header, append it */
    r = vorbis_synthesis_headerin(&v->info, &v->comment, packet);
    if (r != 0) {
      printf("Error: vorbis_synthesis_headerin(), failed: %d\n", r);
      return -1;
    }

    v->num_header_packets++;

    /* when all header packets read, init decoder */
    if (v->num_header_packets == 3) {

      if (vorbis_synthesis_init(&v->state, &v->info) != 0) {
        printf("Error: cannot initialize vorbis.\n");
        return -2;
      }

      if (vorbis_block_init(&v->state, &v->block) != 0) {
        printf("Error: cannot initialize the vorbis block.\n");
        return -3;
      }

      return rxp_decoder_set_audio_info(decoder, v->info.rate, v->info.channels);
    }
    return 0;
  }

  r = vorbis_synthesis(&v->block, packet);
  if (r != 0) {
    printf("Error: vorbis_synthesis failed: %d\n", r);
    return -5;
  }

  r = vorbis_synthesis_blockin(&v->state, &v->block);
  if (r != 0) {
    printf("Error: vorbis_synthesis_blockin failed: %d\n", r);
    return -6;
  }

  samples = vorbis_synthesis_pcmout(&v->state, &pcm);
  if (samples <= 0) {
    printf("Warning: no samples from vorbis: %d\n", samples);
    return -4;
  }

  stream->decoded_frames += samples;
  stream->decoded_pts = (1.0 / decoder->samplerate) * stream->decoded_frames * 1000ull * 1000ull * 1000ull;

  r = vorbis_synthesis_read(&v->state, samples);
  if (r != 0) {
    printf("Error: vorbis_synthesis_read() failed: %d\n", r);
    return -8;
  }

  if (decoder->on_audio) {
    /* @todo: samples is actually frames .. change all names */
    decoder->on_audio(decoder, pcm, samples);
  }
  
  return 0;    
}

static int rxp_decoder_detect_stream_type(rxp_decoder* decoder, 
                                          rxp_stream* stream, 
                                          ogg_page* page, 
                                          ogg_packet* packet)
{

  int r = 0;

  if (stream->type != RXP_NONE) {
    return 0;
  }

  /* is this a theora packet ? */
  r = th_decode_headerin(&decoder->theora.info, 
                         &decoder->theora.comment,
                         &decoder->theora.setup,
                         packet);

  if (r >= 0) {
    stream->type = RXP_THEORA;
    return r;
  }

  /* is this a vorbis packet */
  r = vorbis_synthesis_idheader(packet);
  if (r == 1) {
    stream->type = RXP_VORBIS;
    return r;
  }

  /* unknown type */
  return -1;
}

static int rxp_decoder_find_stream(rxp_decoder* decoder, 
                                   ogg_page* page, 
                                   rxp_stream** stream) 
{

  int r = 0;
  rxp_stream* s = *stream;
  int serial = ogg_page_serialno(page);

  if (ogg_page_bos(page)) {

    /* initialize when we're at the beginning of a stream */
    s = rxp_stream_alloc();
    if (!s)  {
      printf("Error: cannot allocate stream.\n");
      return -1;
    }

    s->serial = serial;

    r = ogg_stream_init(&s->stream_state, serial);
    if (r != 0) {
      printf("Error: cannot initialize the ogg stream.\n");
      return -2;
    }

    *stream = s;

    if (rxp_decoder_add_stream(decoder, *stream) < 0) {
      printf("Error: cannot add stream.\n");
      return -3;
    }

  }
  else {
    /* find the stream for the serial */
    rxp_stream* tail = decoder->streams;
    while (tail) { 
      if (tail->serial == serial) {
        break;
      }
      tail = tail->next;
    }
    
    *stream = tail;
  }

  if (*stream == NULL) {
    printf("Error: cannot find the stream object :( for %d\n", serial);
    return -5;
  }

  (*stream)->eos = ogg_page_eos(page);

  return 0;
}

static int rxp_decoder_add_stream(rxp_decoder* decoder, rxp_stream* stream) {
  
  if (decoder->streams == NULL) {
    /* fist stream */
    decoder->streams = stream;
  }
  else {
    rxp_stream* tail = decoder->streams;
    while (tail) {
      if (tail->next == NULL) {
        break;
      }
      tail = tail->next;
    }
    tail->next = stream;
  }

  return 0;
}

static int rxp_decoder_set_audio_info(rxp_decoder* decoder, 
                                      uint64_t samplerate, 
                                      int nchannels) 
{
  if (!decoder) { return -1; } 

  decoder->samplerate = samplerate;
  decoder->nchannels = nchannels;
  
  return rxp_decoder_trigger_event(decoder, RXP_DEC_EVENT_AUDIO_INFO);
}

static int rxp_decoder_trigger_event(rxp_decoder* decoder, int event) {
  if (!decoder) { return -1; } 
  if (decoder->on_event) {
    decoder->on_event(decoder, event);
  }
  return 0;
}

static char* rxp_decoder_theora_error_to_string(int err) {
  switch(err) {
    case TH_EFAULT:     { return "TH_EFAULT";               } 
    case TH_EBADHEADER: { return "TH_EBADHEADER";           } 
    case TH_EVERSION:   { return "TH_EVERSION";             } 
    case TH_ENOTFORMAT: { return "TH_ENOTFORMAT";           } 
    case TH_DUPFRAME:   { return "TH_DUPFRAME";             }
    case TH_EBADPACKET: { return "TH_EBADPACKET";           } 
    case TH_EIMPL:      { return "TH_EIMPL";                } 
    default:            { return "Unknown theora error\n";  }
  }
}

