#include <rxp_player/Player.h>

namespace rxp {

  /* ------------------------------------------------------------------ */
  void rxp_player_on_video_frame(rxp_player* player, rxp_packet* pkt);
  void rxp_player_on_event(rxp_player* player, int event);
  /* ------------------------------------------------------------------ */

  Player::Player() 
    :is_init(false)
    ,on_event(NULL)
    ,on_video_frame(NULL)
    ,user(NULL)
  {
  }

  Player::~Player() {
    if (true == is_init) {
      shutdown();
    }

    on_event = NULL;
    on_video_frame = NULL;
    user = NULL;
    is_init = false;
  }

  int Player::init(std::string filepath) {

    int result = 0;

    if (true == is_init) {
      printf("Error: looks like you already initialized the player. First call shutdown().\n");
      return -1;
    }

    /* initialize the player. */
    if (0 != rxp_player_init(&ctx)) { 
      printf("Error: cannot initialize the rxp_player.\n");
      result = -1;
      goto error;
    }

    /* open the file. */
    if (0 != rxp_player_open(&ctx, (char*)filepath.c_str())) {
      printf("Error: cannot open the file: %s\n", filepath.c_str());
      if (0 != rxp_player_clear(&ctx)) {
        printf("Error: after not being able to open the file we tried to clear up memory which failed.\n");
        result = -4;
        goto error;
      }
      result = -3;
      goto error;
    }

    ctx.on_video_frame = rxp_player_on_video_frame;
    ctx.on_event = rxp_player_on_event;
    ctx.user = this;

    is_init = true;

    return result;

  error:
    ctx.on_video_frame = NULL;
    ctx.on_event = NULL;
    ctx.user = NULL;

    return result;
  }

  void Player::update() {

#if !defined(NDEBUG)
    if (false == is_init) {
      printf("Error: calling update(), but not initialized.\n");
    }
    return;
#endif

    rxp_player_update(&ctx);
  }

  int Player::shutdown() {
    
    if (false == is_init) {
      printf("Info: it looks like you didn't initialize the player or already shutdown the player.\n");
      return -1;
    }

    if (0 != rxp_player_clear(&ctx)) {
      printf("Error: cannot clear/shutdown the player.\n");
    }

    is_init = false;

    return 0;
  }

  int Player::play() {

    if (0 != rxp_player_play(&ctx)) {
      printf("Error: cannot start playing.\n");
      return -1;
    }

    return 0;
  }

  int Player::pause() {

    if (0 != rxp_player_pause(&ctx)) {
      printf("Error: cannot pause the video playback.\n");
      return -1;
    }

    return 0;
  }

  int Player::stop() {

    if (0 != isPlaying()) {
      printf("Info: cannot stop the player, we're not playing. \n");
      return -1;
    }

    if (0 != rxp_player_stop(&ctx)) {
      printf("Error: cannot stop the video playback.\n");
      return -2;
    }

    return 0;
  }

  int Player::isPlaying() {
    return rxp_player_is_playing(&ctx);
  }

  int Player::isPaused() {
    return rxp_player_is_paused(&ctx);
  }

  /* ------------------------------------------------------------------ */

  void rxp_player_on_video_frame(rxp_player* player, rxp_packet* pkt) {

    Player* p = static_cast<Player*>(player->user);
    if (NULL == p) {
      printf("Error: cannot get a handle to the player.\n");
      return;
    }

    if (NULL != p->on_video_frame) {
      p->on_video_frame(p, pkt);
    }
  }

  void rxp_player_on_event(rxp_player* player, int event) {

    Player* p = static_cast<Player*>(player->user);
    if (NULL == p) {
      printf("Error: cannot get a handle to the player.");
      return;
    }

    if (NULL != p->on_event) {
      p->on_event(p, event);
    }
    
    if (RXP_DEC_EVENT_AUDIO_INFO == event) {
#if !defined(NDEBUG)
      printf("+ Received RXP_DEC_EVENT_AUDIO_INFO event.\n");
      printf("+ ERROR: we do not yet support audio streams in the CPP driver.\n");
#endif
    }
    else if (RXP_PLAYER_EVENT_PLAY == event) {
#if !defined(NDEBUG)
      printf("+ Received RXP_PLAYER_EVENT_PLAY event.\n");
#endif
    }
    else if (RXP_PLAYER_EVENT_RESET == event) {
#if !defined(NDEBUG)
      printf("+ Received RXP_PLAYER_EVENT_RESET event.\n");
#endif
    }
    else {
      printf("+ Unhandled event: %d\n", event);
    }
  }

  /* ------------------------------------------------------------------ */

}; /* namespace rxp */
