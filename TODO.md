
 TODO overall
 -------------
 
 - All functions need to return  < 0 on error, 0 on success 

 TODO rxp::Player + rxp::PlayerGL
 --------------------------------
 - At this moment the rxp::Player and rxp::PlayerGL have no support for 
   audio playback so playing a video that contains also audio will 
   not work because the internal audio timer is not updated. The player 
   works fine with video w/o audio. For now you can use avconv to remove
   the audio. 

 TODO rxp_scheduler
 ------------------

 - When we've decoded all frames the decoder should stop/change state
   and cleanup the allocated tasks.

 - With ogg files it may be possible that the first N-bytes are only used
   for video and then the next N bytes are only used for audio, where the
   N is very big. This means that e.g. you have 5s of audio data first, 
   then 5s of video data (total playable duration is 5s). This means
   that the scheduler needs to check if both audio and video goal
   pts values are reached! 

 TODO rxp_decoder
 ----------------

 - The decoder has two members "samplerate" and "nchannels" of which I'm 
   not sure if they are necessary there. We could add a callback and 
   pass the audio information to that. The rxp_player also has these
   members as they need to be used by the user. Maybe we just need an
   info callback which passes all information that is related to the 
   streams that we found (audio and video, like pixel format, width,
   height etc..).

 - Figure out if libvorbis can return interleaved audio so we can 
   pass this directly w/o reformatting it to the output. 
  
 - Maybe we need to set the audio format in the player/decoder as well
   so the user knows what kind of audio data we're dealing with.


 TODO rxp_player
 ---------------

 - At this moment, we don't have a clear flow to handle the start/stop
   of the audio stream. maybe we should fire an event "start audio stream"
   and "stop audio stream" from the rxp_player. 

 - Add a pause feature. I'm wondering how to handle the audio callback in 
   this cause as it shouldn't update the number of asked samples.. maybe
   just fill with an empty audio buffer? 

 - When file is not found we should cancel everything + send event

 - The pause/stop/play code looks ugly ... and the rxp_player_is_playing(),
   rxp_player_open(), and rxp_player_is_paused() are not locking the state member
   becuase this will cause a crash when you've cleared the player. 

 - Rename samples to frames as the name samples is misleading.

 - Add information for the user about the cropping area it needs ot 
   use when drawing the video. 


 TODO test_glfw.cpp
 -------------------

 - Implement cropping fo rthe video. 

 
 TODO rxp_types
 --------------

 - Rename the events to more verbose, e.g. RXP_DEC_EVENT, RXP_DECODER_EVENT
