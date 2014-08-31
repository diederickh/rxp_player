/*
 
  BASIC GLFW + GLXW WINDOW AND OPENGL SETUP 
  ------------------------------------------
  See https://gist.github.com/roxlu/6698180 for the latest version of the example.
 
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
 
#if defined(__linux) || defined(_WIN32)
#  include <GLXW/glxw.h>
#endif
 
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#define ROXLU_USE_PNG
#define ROXLU_USE_OPENGL
#define ROXLU_USE_MATH
#define ROXLU_USE_FONT
#define ROXLU_IMPLEMENTATION
#include <tinylib.h>
 
void button_callback(GLFWwindow* win, int bt, int action, int mods);
void cursor_callback(GLFWwindow* win, double x, double y);
void key_callback(GLFWwindow* win, int key, int scancode, int action, int mods);
void char_callback(GLFWwindow* win, unsigned int key);
void error_callback(int err, const char* desc);
void resize_callback(GLFWwindow* window, int width, int height);

/* ---------------------- begin: player specific ------------------------ */

extern "C" {
#  include <rxp_player/rxp_player.h>
#  include <cubeb/cubeb.h>
}

static const char* RXP_PLAYER_VS = ""
  "#version 330\n"
  "const vec2 pos[] = vec2[4](                \n"
  "  vec2(-1.0,  1.0),                        \n"
  "  vec2(-1.0, -1.0),                        \n"
  "  vec2( 1.0,  1.0),                        \n"
  "  vec2( 1.0, -1.0)                         \n"
  ");                                         \n"
  "                                           \n"
  " const vec2[] tex = vec2[4](               \n"
  "   vec2(0.0, 0.0),                         \n"
  "   vec2(0.0, 1.0),                         \n"
  "   vec2(1.0, 0.0),                         \n"
  "   vec2(1.0, 1.0)                          \n"
  ");                                         \n"
  "out vec2 v_tex;                            \n" 
  "void main() {                              \n"
  "   vec2 p = pos[gl_VertexID];              \n"
  "   gl_Position = vec4(p.x, p.y, 0.0, 1.0); \n"
  "   v_tex = tex[gl_VertexID];               \n"
  "}                                          \n"
  "";

static const char* RXP_PLAYER_FS = "" 
  "#version 330                                                      \n"
  "uniform sampler2D u_ytex;                                         \n"
  "uniform sampler2D u_utex;                                         \n"
  "uniform sampler2D u_vtex;                                         \n"
  "in vec2 v_tex;                                                    \n"
  "const vec3 R_cf = vec3(1.164383,  0.000000,  1.596027);           \n"
  "const vec3 G_cf = vec3(1.164383, -0.391762, -0.812968);           \n"
  "const vec3 B_cf = vec3(1.164383,  2.017232,  0.000000);           \n"
  "const vec3 offset = vec3(-0.0625, -0.5, -0.5);                    \n"
  "layout( location = 0 ) out vec4 fragcolor;                        \n" 
  "void main() {                                                     \n"
  "  float y = texture(u_ytex, v_tex).r;                             \n"
  "  float u = texture(u_utex, v_tex).r;                             \n"
  "  float v = texture(u_vtex, v_tex).r;                             \n"
  "  vec3 yuv = vec3(y,u,v);                                         \n"
  "  yuv += offset;                                                  \n"
  "  fragcolor = vec4(0.0, 0.0, 0.0, 1.0);                           \n"
  "  fragcolor.r = dot(yuv, R_cf);                                   \n"
  "  fragcolor.g = dot(yuv, G_cf);                                   \n"
  "  fragcolor.b = dot(yuv, B_cf);                                   \n"
  "}                                                                 \n"
  "";

static void on_event(rxp_player* player, int event);               /* gets called whenever an event occurs */
static void on_video_frame(rxp_player* player, rxp_packet* pkt);   /* gets called whenever a new video frame needs to be displayed. */
static GLuint create_texture(int width, int height);               /* create yuv420p specific texture */
static int setup_opengl();                                         /* sets up all the openGL state for the player */
static int setup_player();                                         /* sets up the rxp_player */

GLuint prog = 0;                                                   /* or program to render the yuv buffes */
GLuint vert = 0;                                                   /* vertex shader (see above) */
GLuint frag = 0;                                                   /* fragment shader (see above) */
GLuint vao = 0;                                                    /* we need to use a vao for attribute-less rendering */
GLuint tex_y = 0;                                                  /* y-channel texture to which we upload the y of the yuv420p */
GLuint tex_u = 0;                                                  /* u-channel texture to which we upload the u of the yuv420p */
GLuint tex_v = 0;                                                  /* v-channel texture to which we upload the v of the yuv420p */
rxp_player player;                                                 /* the all mighty player =) */ 

cubeb* audio_ctx = NULL;
cubeb_stream* audio_stream = NULL;

static void start_audio();
static long audio_data_cb(cubeb_stream* stream, void* user, void* buffer, long nframes);
static void audio_state_cb(cubeb_stream* stream, void* user, cubeb_state state);

/* ---------------------- end:   player specific ------------------------ */
 
int main() {
 
  glfwSetErrorCallback(error_callback);
 
  if(!glfwInit()) {
    printf("Error: cannot setup glfw.\n");
    exit(EXIT_FAILURE);
  }
 
  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  
  GLFWwindow* win = NULL;
  int w = 1280;
  int h = 720;
 
  win = glfwCreateWindow(w, h, "GLFW", NULL, NULL);
  if(!win) {
    glfwTerminate();
    exit(EXIT_FAILURE);
  }
 
  glfwSetFramebufferSizeCallback(win, resize_callback);
  glfwSetKeyCallback(win, key_callback);
  glfwSetCharCallback(win, char_callback);
  glfwSetCursorPosCallback(win, cursor_callback);
  glfwSetMouseButtonCallback(win, button_callback);
  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);
 
#if defined(__linux) || defined(_WIN32)
  if(glxwInit() != 0) {
    printf("Error: cannot initialize glxw.\n");
    exit(EXIT_FAILURE);
  }
#endif
 
  // ----------------------------------------------------------------
  // THIS IS WHERE YOU START CALLING OPENGL FUNCTIONS, NOT EARLIER!!
  // ----------------------------------------------------------------

  if (setup_opengl() < 0) {
    printf("Error: cannot setup the opengl objects for the player.\n");
    exit(EXIT_FAILURE);
  }

  if (setup_player() < 0) {
    exit(EXIT_FAILURE);
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  Painter painter;
  painter.fill();
  painter.color(0.3, 0.3, 0.3, 0.7);
  painter.rect(0, 0, 400, 60);

  PixelFont font;
  font.write(10, 10, "P - pause");
  font.write(10, 30, "SPACE - continue playback when paused, or stop.");

  while(!glfwWindowShouldClose(win)) {
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Player code */
    /* ----------------------------------------------- */

    /* update the player */
    rxp_player_update(&player);

    /* drawing */
    glUseProgram(prog);
    glBindVertexArray(vao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_y);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex_u);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex_v);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    /* ----------------------------------------------- */
    painter.draw();
    font.draw();

    glfwSwapBuffers(win);
    glfwPollEvents();
  }
 
  glfwTerminate();
 
  return EXIT_SUCCESS;
}

void char_callback(GLFWwindow* win, unsigned int key) {
}
 
void key_callback(GLFWwindow* win, int key, int scancode, int action, int mods) {
  
  if(action != GLFW_PRESS) {
    return;
  }
 
  switch(key) {
    case GLFW_KEY_SPACE: {
      /* toggle between:
         - start/continue playing
         - pause
         - stop
      */
      if(rxp_player_is_paused(&player) == 0) {
        if (rxp_player_play(&player) < 0) {
          printf("+ Failed to continue after pause.\n");
        }
      }
      else if (rxp_player_is_playing(&player) != 0) {
        setup_player();
      }
      else { 
        if (rxp_player_stop(&player) < 0) {
          printf("+ Failed to stop the player.\n");
        }
      }
      break;
    }
    case GLFW_KEY_P: {
      /* Pause */
      if (rxp_player_is_playing(&player) == 0) {
        if (rxp_player_pause(&player) < 0) {
          printf("+ Failed to pause the player.\n");
        }
      }
      else {
        printf("+ Not playing, cannot pause.\n");
      }
      break;
    }
    case GLFW_KEY_R: {
      /* Restart (used for some stress testing) */
      if (rxp_player_is_playing(&player) == 0) {
        printf("+ Stop + Restart.\n");
        if (rxp_player_stop(&player) == 0) {
          setup_player();
        }
        else {
          printf("+ Stop failed.\n");
        }
      }
      break;
    }
    case GLFW_KEY_S: {
      if (rxp_player_stop(&player) < 0) {
        printf("Error: cannot stop for some reason.\n");
      }
      break;
    }
    case GLFW_KEY_C: {
      break;
    }
    case GLFW_KEY_ESCAPE: {
      glfwSetWindowShouldClose(win, GL_TRUE);
      break;
    }
  };
}
 
void resize_callback(GLFWwindow* window, int width, int height) {
}
 
void cursor_callback(GLFWwindow* win, double x, double y) {
}
 
void button_callback(GLFWwindow* win, int bt, int action, int mods) {
}
 
void error_callback(int err, const char* desc) {
  printf("GLFW error: %s (%d)\n", desc, err);
}

/* -------------------------------------------------------------------------------------- */

static GLuint create_texture(int width, int height) {
  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  return tex;
}

static int setup_opengl() {
  
  vert = rx_create_shader(GL_VERTEX_SHADER, RXP_PLAYER_VS);
  frag = rx_create_shader(GL_FRAGMENT_SHADER, RXP_PLAYER_FS);
  prog = rx_create_program(vert, frag, true);
  glUseProgram(prog);
  glUniform1i(glGetUniformLocation(prog, "u_ytex"), 0);
  glUniform1i(glGetUniformLocation(prog, "u_utex"), 1);
  glUniform1i(glGetUniformLocation(prog, "u_vtex"), 2);

  glGenVertexArrays(1, &vao);

  return 0;
}

/*
  
  Here we initialize the player, open a file and start playing. 
  Every thime you want to open an new file, make sure that the 
  player is initialized by calling `rxp_player_init()`. 

 */
static int setup_player() {

  if (rxp_player_init(&player) < 0) {
    printf("+ Error: cannot init player.\n");
    return -1;
  }

  std::string video = rx_get_exe_path() +"/big_buck_bunny_720p_stereo.ogg";
  //std::string video = rx_get_exe_path() +"/big_buck_bunny_720p_no_audio.ogg";
  if (rxp_player_open(&player, (char*)video.c_str()) < 0) {
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

/*

  At this moment we only support YUYV420P video and here
  we create the 3 textures for each Y,U and V layer. Note 

 */

static void on_video_frame(rxp_player* player, rxp_packet* pkt) {

  if (tex_y == 0) {
    /* create textures after we've decoded a frame. */
    tex_y = create_texture(pkt->img[0].width, pkt->img[0].height);
    tex_u = create_texture(pkt->img[1].width, pkt->img[1].height);
    tex_v = create_texture(pkt->img[2].width, pkt->img[2].height);
  }

  glBindTexture(GL_TEXTURE_2D, tex_y);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, pkt->img[0].stride);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pkt->img[0].width, pkt->img[0].height, GL_RED, GL_UNSIGNED_BYTE, pkt->img[0].data);

  glBindTexture(GL_TEXTURE_2D, tex_u);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, pkt->img[1].stride);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pkt->img[1].width, pkt->img[1].height, GL_RED, GL_UNSIGNED_BYTE, pkt->img[1].data);

  glBindTexture(GL_TEXTURE_2D, tex_v);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, pkt->img[2].stride);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pkt->img[2].width, pkt->img[2].height, GL_RED, GL_UNSIGNED_BYTE, pkt->img[2].data);
}  

/* 
   This is an important piece of the player as it is the 
   place where you start the audio stream, clean the audio
   stream and clean the player. 

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
          stream if it was running. Call `rxp_player_clear()` when
          you receive this event. This event is fired when either
          you asked the player to stop by using `rxp_player_stop()`
          or simply when we're ready decoding video frames or when 
          the audio buffer hasn't got any new samples that can be 
          played. 

 */
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

/*

  This is the audio callback from the OS. We're using the excellent cubeb library
  for audio output. This is where we ask the player to fill the audio buffer by 
  calling `rxp_player_fill_audio_buffer()`. 
  
  When `rxp_player_fill_audio_buffer()` returns < 0, it means there are no audio
  sample left to be written. This is when the player will stop playback and trigger
  the RXP_PLAYER_EVENT_RESET as soon as poaaible

 */
static long audio_data_cb(cubeb_stream* stream, void* user, void* buffer, long nframes) {
  rxp_player_fill_audio_buffer(&player, buffer, nframes);
  return nframes;
}

/*

  This is called by cubeb whenever the audio stream state changes; we can simply
  ignore this here.

 */
static void audio_state_cb(cubeb_stream* stream, void* user, cubeb_state state) {
}

/*

  This starts the audio stream using cubeb. Note that we lock the player because
  we're accessing the internal state and the player uses threads; so just making
  sure we're safe here : ) 
  
 */
static void start_audio() {

  printf("+ Starting the audio callback\n");

  /* create audio stream */
  uint64_t samplerate = 0;
  int nchannels = 0;
  int r = 0;

  rxp_player_lock(&player);
  samplerate = player.samplerate;
  nchannels = player.nchannels;
  rxp_player_unlock(&player);

  if (!nchannels) {
    printf("+ Error: Invalid number of channels (no channels).\n");
    exit(1);
  }
    
  /* only initalize the audio stream once. */
  if (!audio_ctx) {
    printf("+ Initializing the audio stream.\n");
      
    /* init cubeb */
    r = cubeb_init(&audio_ctx, "Player");
    if (r != CUBEB_OK) {
      printf("Error: cannot initialize the audio player.\n");
      exit(1);
    }
  }

  /* init stream */
  cubeb_stream_params params;
  params.format = CUBEB_SAMPLE_FLOAT32LE; /* @todo we're hardcoding the audio output format now this must be configurable */
  params.rate = samplerate;
  params.channels = nchannels;

  r = cubeb_stream_init(audio_ctx, &audio_stream, "Player audio stream", 
                        params, 300, audio_data_cb, audio_state_cb, NULL);
  if (r != CUBEB_OK) {
    printf("+ Error: cannot init the audio stream.\n");
    exit(1);
  }

  cubeb_stream_start(audio_stream);
}

