# rxp_player


## About
 
 While working on a project recently I stumbled upon a memory leak in   
 a quicktime based video player which I couldn't fix because the source 
 code was closed. While googling for a basic video player I did not find any and
 therefore I started this project as both a research project and to create 
 an open source video player with a simple API.

 We wil only use the excellent libraries of [xiph](http://www.xiph.org), at 
 the time of writing this means that we're using vorbis and theora. In the 
 near future we will add support for Daala and Opus. 

 The main usage for this library is for in game use or in creative/museum 
 like installations. The glfw example contains all the code needed to create    
 a fully functional video player. 
 
## Usage

 - open a file: `rxp_player_open()`
 - start playing/decoding: `rxp_player_play()`
 - when the user recieves the RXP_PLAYER_EVENT_PLAY event it should
   start the audio if necessary and the audio stream. In the cause when 
   the .ogg file contains audio, you must call `rxp_player_fill_audio_buffer()`
   in your audio callback. 
 - when you receive a RXP_PLAYER_EVENT_RESET event, you need to stop the 
   playback and, if any, stop/destroy the audio stream. Also make sure to 
   call `rxp_player_clear()` to free all the used memory.

   _For an example see the `rxp_player_glfw.cpp` file in the examples dir._
   
## Depenencies
 
 *Depencies for the rxp_player library:*

 [libogg] for demuxing the ogg files  
 [libvorbis] for decoding the audio  
 [libtheora] for decoding video  
 [libuv] for threading  
 
 *Dependencies for the example:*

 [cubeb][cubeb] for audio output  
 [glfw][glfw] for GL context/window/input  
 [glxw][glxw] for GL function loading in on Windows/Linux  
 [tinylib][tinylib] for some handy GL functions  


## Lessons learned

 This project was much about researching the whole video playback spectrum
 and how to create a video player which can playback ogg/vorbis/theora
 easily.  Some things I've learned:
 
 - The clock and last decode pts is probably the most important part of this
   player. You can only get video and audio in sync when you're handling the 
   clock correctly. There are basically two kinds of clocks you can use. The
   first one is a high resolution CPU timer. You use this clock to check what
   frame you should render in case of a video w/o audio. When your video has 
   audio, you count the number of handled audio frames in the audio callback 
   function you get from your OS. Then you select the correct video packet
   which pts matches the one calculated by the amount of handled audio samples.

 - Another very important part is the way we decode streams. An ogg file can 
   contain multiple video/audio/text/.. streams but they may be stored with an 
   order like:

   ````
   
   0s                    5s , 0s                5s , 0s                5s
   +------------------------+----------------------+--------------------+
   |        AUDIO           |     VIDEO            |       TEXT         |
   +------------------------+----------------------+--------------------+


   ````

   Note the timings above, when the scheduler calls decode(), it passes 
   the pts up to which you need to decode each of the streams. So lets say
   the goal pts is 5s, then we need to continue decoding until each of the
   three streams is decoded till 5s.

 - When you use the audio callback as the time base (in case you have a video 
   with audio), you have to make sure, that this function is only used/called 
   when you already decoded a couple of audio frames so that the first callback
   can actually get some audio. This is easily implemented by directly decoding 
   the first N-seconds after you have opened the file and before you started playing.

 - Open/decoding flow: you need to follow a specific flow for the 
   open/decode/play process. When you open the file, you directly want to get
   some information about the used media streams so the user can setup e.g. 
   buffers, audio streams etc.. We're using a scheduler to perform all these
   tasks. We open a file, read the first N-seconds and then one can add a 
   play task to the scheduler. When all tasks up to the play task have been 
   done, the scheduler calls the play() callback so we know for sure we have 
   enough audio samples and video packets. Only when we have decoded enough 
   audio samples the player/user can start the audio callback of the os.
   

 - Play/stop/pause flow:  
   
   When the user wants to start playing a file, we need to open the .ogg
   file by adding a file open task to the scheduler thread. The scheduler, 
   will simply call the set open_file() callback from the thread itself.
   Also, the scheduler open_file() function will directly add a decode task
   so the player will fill up it's buffers for video/audio that hold decoded
   data. In practice the scheduler adds these tasks when open_file() is called:

     - open file taks
     - decode up to some goal pts task( e.g. 3 seconds)

   When the scheduler calls the play() callback (when it receives the play task), 
   you should start the audio callback of your OS when the player has its samplerate
   and nchannels members set. When the .ogg file has audio, we will use the total 
   number of played audio samples as time base to sync the video packets with. When
   the user receives the reset (RXP_PLAYER_EVENT_RESET) event, it should stop the 
   audio callback again.

   When the first tasks are done, the user needs to call rxp_scheduler_update()
   (or rxp_player_update() which will call this for you), because the scheduler needs
   to increment the goal_pts value, based on the current "decoded_pts" value. The 
   decoded_pts value keeps the pts up until which we have decoded. Using this flow,
   the scheduler makes sure that it has some decoded data ready.

   When the user wants to pause the player, we simply need to stop the pts value
   that we use to synchronize and advance the player. When we have audio, and thus
   using the number handled audio samples this means that we need to stop incrementing
   the number of played samples and only pass "zeroes" to the buffer. I'm not 
   entirely sure if this is the best solution because we're waisting some cpu
   cycles there because we could also have stopped the audio callback. 

   Then when we need to stop the video, we need to join the scheduler 
   thread which will call the on_stop() callback just before stopping the 
   scheduler. Only in on_stop() we can reset all state of the player/scheduler.

 - Managing the audio buffer:

   The player uses a ringbuffer to store raw audio samples. A thing to remember 
   is that we need to start/stop the audio output/callback. At this moment the user 
   of the player can use rxp_player_fill_audio_buffer() in the audio callback
   to fill the audio buffer of the OS. This function will return < 0, when there is 
   no audio data left and if this function return < 0, you should stop the audio 
   callback directly!

     
## Older versions

 - Before first big cleanup https://gist.github.com/roxlu/8d95e8b8f40addfb399c
 - Before second big cleanup: https://gist.github.com/roxlu/803941f0657b2f561a20

[libogg]: http://downloads.xiph.org/releases/ogg/libogg-1.3.1.tar.gz
[libvorbis]: http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.4.tar.gz
[libtheora]: http://downloads.xiph.org/releases/theora/libtheora-1.1.1.zip
[libuv]: https://github.com/joyent/libuv/
[cubeb]: https://github.com/kinetiknz/cubeb
[glfw]: http://www.glfw.org/
[glxw]: https://github.com/rikusalminen/glxw
[tinylib]: https://github.com/roxlu/tinylib

