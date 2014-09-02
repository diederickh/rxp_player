/*

  rxp::Player
  -----------

  C++ wrapper around the player. This basically works exactly the same as 
  the C version, though it doesn't support audio at the moment of writing this.

 */
#ifndef RXP_CPP_PLAYER_H
#define RXP_CPP_PLAYER_H

#include <string> 

extern "C" {
#  include <rxp_player/rxp_player.h>
}

namespace rxp { 

  class Player;
  typedef void(*rxp_cpp_player_on_video_frame_callback)(Player* player, rxp_packet* pkt);
  typedef void(*rxp_cpp_player_on_event_callback)(Player* player, int event);

  class Player {

  public:
    Player();
    ~Player();

    int init(std::string filepath);                    /* open the given file; make sure that you first initialize the player. */
    int shutdown();                                    /* cleanup everything, free memory. */
    void update();                                     /* call this often as it will process data from the current queue. */
    int play();                                        /* start playing, returns 0 on success, < 0 on error */ 
    int pause();                                       /* pause the current playback, returns 0 on success, < 0 on error */
    int stop();                                        /* stop, returns 0 on success, < 0 on error. when you call play() again it will start from the 0.0 */
    
    int isInit();                                      /* returns 0 if we're initialized otherwise < 0. */
    int isPlaying();                                   /* returns 0 when we're currently playing, otherwise < 0. */
    int isPaused();                                    /* returns 0 when we're paused, otherwise < 0. */

  public:
    rxp_player ctx;
    bool is_init;
    bool is_playing;
    bool is_paused;
    rxp_cpp_player_on_video_frame_callback on_video_frame;
    rxp_cpp_player_on_event_callback on_event;         /* gets called whenever an event from the player occurs; this may happen from a different thread. */
    void* user;
  };

  inline int Player::isInit() {
    return (is_init) ? 0 : -1;
  }


} /* namespace rxp */


#endif
