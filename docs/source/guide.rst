.. _guide:

******************
Programmers Guide 
******************

On this page we will describe everything you need to know to create
a fully working video player with rxp_player with audio and video
output. For the output parts we refer to the glfw example that you can 
find in `src/examples`.

The simplest video player is one that does not have audio output. 
Therefore we will start with this first and in the next section we
describe how you can add audio output too. Although the API is similar
for both video with and without audio, for the user there are some
big differences.

A global flow of how to use the player is:

 * initialize a player with :func:`rxp_player_init()`
 * open a file using :func:`rxp_player_open()`
 * start playback with :func:`rxp_player_play()` 
 * call :func:`rxp_player_update()` repeatedly in your draw loop.
 * release all used memory when you receive the `RXP_PLAYER_EVENT_RESET` 
   by calling :func:`rxp_player_clear()` in you `on_event` callback.

Before you can use the player you need to initialize the `rxp_player`
context which manages all memory, video, audio etc.. Call
:func:`rxp_player_init()` with a pointer to a `rxp_player` struct. All functions
of the `rxp_player` library return zero on sucess, < 0 on error, so make
sure to check this. 

After you've initialized the struct you can open a file by calling
:func:`rxp_player_open()` and pass the context struct and the filepath 
to the .ogg file you want to play. 

To start playing call :func:`rxp_player_play()`. But by only calling 
these functions you're not yet there. You need to tell the `rxp_player`
that you want to receive video frames. For this we use a callback, 
that should accept a pointer to the player and a `rxp_packet`. 

The `rxp_packet` holds all the information you need to display a 
frame. Every time this function is called it means you need to 
update your screen with the recevied buffers. The `rxp_packet`
has a member `img[3]` that contains the `width`, `height`, `stride`
and video `data` for each of the video planes. At the time of writing
we only support YUV420P video data.

The function below shows a simple example of this: 

.. highlight:: c

:: 

   // player is `rxp_player player`
   static int setup_player() {
    
      if (rxp_player_init(&player) < 0) {
        printf("+ Error: cannot init player.\n");
        return -1;
      }
    
      if (rxp_player_open(&player, "bunny.ogg") < 0) {
        printf("+ Error: cannot open the ogg file.\n");
        return -2;
      }
    
      if (rxp_player_play(&player) < 0) {
        printf("+ Error: failed to start playing.\n");
        return -3;
      }
    
      player.on_video_frame = on_video_frame;
      player.on_event = on_event;
    
      return 0;
    }


An important aspect you need to implement is the `on_event` callback
where you clear all used memory when the player has finished playing
and decoding all frames. The `on_event` function will be called when 
certain player or decoder events happen. These events are:

   RXP_PLAYER_DEC_EVENT_AUDIO_INFO:
          The decoder decoded some audio frames and the 
          players' members nchannels and samplerate have been 
          set.

   RXP_PLAYER_EVENT_PLAY:
          The scheduler/player has opened the file and decoded
          the first couple of frames/seconds and the player is 
          ready to start running. This is when you should start the 
          audio stream when the .ogg file has audio samples. You 
          can check this by testing the number of channels, which 
          should be > 0, when the .ogg file has an audio stream.

   RXP_PLAYER_EVENT_RESET: 
          Whenever you receive the RXP_PLAYER_EVENT_RESET event 
          it's time to tear down the player and stop the audio 
          stream if it was running. Call :func:`rxp_player_clear()` when
          you receive this event. This event is fired when either
          you asked the player to stop by using :func:`rxp_player_stop()`
          or simply when we're ready decoding video frames or when 
          the audio buffer hasn't got any new samples that can be 
          played. 

The function below shows an example that implements an event handler, which 
also start an audio stream using the `cubeb`_ library. Note how we clear
the used memory where we receive the `RXP_PLAYER_EVENT_RESET` event. When 
you don't call :func:`rxp_player_clear()` memory will leak.

.. highlight:: c

:: 

    static void on_event(rxp_player* p, int event) {
    
      if (event == RXP_DEC_EVENT_AUDIO_INFO) {
        printf("+ Received RXP_DEC_EVENT_AUDIO_INFO event.\n");
      }
      else if (event == RXP_PLAYER_EVENT_PLAY) {
        printf("+ Received RXP_PLAYER_EVENT_PLAY event.\n");
        if (p->nchannels > 0) {
          start_audio();
        }
      }
      else if (event == RXP_PLAYER_EVENT_RESET) {
        printf("+ Received RXP_PLAYER_EVENT_RESET event.\n");
    
        if (rxp_player_clear(p) < 0) {
          printf("+ Failed clearing the player.\n");
        }
    
        /* check if this is a repeated call to start the audio stream */
        if (audio_ctx) {
          cubeb_stream_stop(audio_stream);
          cubeb_stream_destroy(audio_stream);
          cubeb_destroy(audio_ctx);
          audio_ctx = NULL;
          audio_stream = NULL;
          printf("+ Cleaned up the audio stream.\n");
        }
      }
    }



