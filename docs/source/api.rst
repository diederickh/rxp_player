.. _apiref:

*************
API Reference
*************

.. highlight:: c

.. function:: rxp_player_init(rxp_player* player)

   Initialize the player and all it's members. Internally this will create a queue
   for the video plackets, sets up the ringbuffer for audio to default values (won't 
   allocate any bytes for the ringbuffer here), initializes the decoder, scheduler
   clock etc.. This must be called before you make any other call on the rxp_player, 
   and everytime where you called :func:`rxp_player_clear()`.

   :param rxp_player*: Pointer to the rxp_player 
   :returns: 0 on success, < 0 on error.

.. function:: rxp_player_clear(rxp_player* player)

   This function clears all used memory of the `rxp_player`. This function will 
   will deallocate the packet queue, deallocate the scheduler, decoder, clock
   etc.. After calling :func:`rxp_player_clear()` you can call :func:`rxp_player_init()` 
   again if  you want to reload or replay the file.

   :param rxp_player*: Pointer to the rxp_player 
   :returns: 0 on success, < 0 on error.

.. function:: rxp_player_open(rxp_player* player, char* file)

   This will open the given .ogg file. Make sure that you've called :func:`rxp_player_init()`
   before calling this function. Also, when you want to re-open the same file after it has
   been played completely and you already called :func:`rxp_player_clear()` to free internally
   used memory, you need to call :func:`rxp_player_init()` before calling this function again.

   :param rxp_player*: Pointer to the rxp_player 
   :returns: 0 on success, < 0 on error.

   :: 
   
       // somewhere globally ....
       rxp_player player;
    
       // opening a file
       { 
         if (rxp_player_init(&player) < 0) {
           exit(1);
         }
      
         if (rxp_player_open(&player, "bunny.ogg") < 0) {
           exit(1);
         }
      
         if (rxp_player_play(&player) < 0) {
           exit(1);
         }
    
         // set callbacks
         player.on_event = on_event
         player.on_video_frame = on_video_frame
       }
   
.. function:: rxp_player_play(rxp_player* player)

   Start playing the opened file. Make sure that you've called :func:`rxp_player_init()`,
   :func:`rxp_player_open()` first. 

   :param rxp_player*: Pointer to the rxp_player 
   :returns: 0 on success, < 0 on error.

.. function:: rxp_player_update(rxp_player* player)

   Make sure to call this function as often as possible as it will check if you need
   to display a new video frame. And it will make sure that the internally used 
   scheduler will be updated as well so it will continue decoding as needed. 

   :param rxp_player*: Pointer to the rxp_player 
   :returns: 0 on success, < 0 on error.

.. function:: rxp_player_pause(rxp_player* player)

   Pause the playback. This will change the state of the player and the 
   :func:`rxp_player_update()` will not handle any frames/timings until we continue
   playing again. To continue playback, call :func:`rxp_player_play()` again.
   
   :param rxp_player*: Pointer to the rxp_player 
   :returns: 0 on success, < 0 on error.

.. function:: rxp_player_stop(rxp_player* player)

   Stop the currently being played player. This will stop everything completely 
   and you'll need to re-initialize the player again if you want to start playing 
   again. This will trigger the `RXP_PLAYER_EVENT_RESET` from where you call 
   :func:`rxp_player_clear` as described in the programmers guide.

   :param rxp_player*: Pointer to the rxp_player
   :returns: 0 on success, < 0 on error.

.. function:: rxp_player_is_playing(rxp_player* player)

   Check if the video player is playing. 

   :param rxp_player*: Pointer to the rxp_player 
   :returns: 0 when the player is playing, else 1, < 0 on error.

.. function:: rxp_player_is_paused(rxp_player* player)

   Check if the video player is paused. 

   :param rxp_player*: Pointer to the rxp_player 
   :returns: 0 when the player is paused, else 1, < 0 on error.
