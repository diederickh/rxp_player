/*

  rxp::PlayerGL
  --------------

  Wrapper around rxp_player and the rxp::Player that uses openGL to render a 
  video. Note that this is all tested using a GL 3.2 core profile context. 


  EXAMPLE:
      
        main.cpp:

           ````c++
             #define RXP_PLAYER_GL_IMPLEMENATION
             #include <your_opengl_headers.h>
             #include <rxp_player/PlayerGL.h>
           ````

        your_app.cpp:

           ````c++
             #include <your_opengl_headers.h>
             #include <your_opengl_headers.h>
           ````

  USAGE:

        1. Include the <rxp_player/PlayerGL.h> header where you want to use the 
           PlayerGL. 
        2. In e.g. you main.cpp, use `#define RXP_PLAYER_GL_IMPLEMENTATION`

  IMPORTANT:

        - Before including this file make sure that you've included the correct GL 
          headers.
        - When we receive the RXP_PLAYER_EVENT_RESET we will shutdown the player.
          So to restart playing you would need to call init() again.

 */
#ifndef RXP_CPP_PLAYER_GL_H
#define RXP_CPP_PLAYER_GL_H

#include <string>
#include <rxp_player/Player.h>

namespace rxp {
  
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

  /* ------------------------------------------------------------------------*/

  class PlayerGL;
  typedef void(*rxp_playergl_on_event_callback)(PlayerGL* player, int event);
  typedef void(*rxp_playergl_on_video_frame_callback)(PlayerGL* player, rxp_packet* pkt);
  /* ------------------------------------------------------------------------*/

  class PlayerGL {

  public:
    PlayerGL();
    ~PlayerGL();
    int init(std::string filepath);
    int shutdown();
    void update();
    void draw();                                                            /* draw full screen (don't change the viewport)  */
    void draw(int x, int y, int w, int h);                                  /* draw the video at (x,y) from top left of the window (will call glViewport) */
    void resize(int w, int h);                                              /* call this when the window resizes. */
    int play();
    int pause();
    int stop();
    int isInit();                                                           /* returns 0 when init, else < 0 */
    int isPlaying();                                                        /* returns 0 when playing, else < 0 */ 
    int isPaused();                                                         /* returns 0 when paused, else < 0 */

  public:
    Player ctx;                                                             /* the CPP player wrapper. */

    /* callback */
    rxp_playergl_on_event_callback on_event;                                /* gets called when we receive a player event. */  
    rxp_playergl_on_video_frame_callback on_video_frame;                    /* gets called when we've update the video frame */
    void* user;                                                             /* can be set to any user data. */

    /* GL */
    GLuint vert;
    GLuint frag;
    GLuint prog;
    GLuint vao;
    GLuint tex_y;
    GLuint tex_u;
    GLuint tex_v;
    GLint vp[4];                                                            /* the viewport, used to position the video */

    int video_width;
    int video_height;
  };

  /* ------------------------------------------------------------------ */

  inline int PlayerGL::play() {
    return ctx.play();
  }

  inline int PlayerGL::pause() {
    return ctx.pause();
  }

  inline int PlayerGL::stop() {
    return ctx.stop();
  }

  inline void PlayerGL::update() {
    ctx.update();
  }

  inline int PlayerGL::isPlaying() {
    return ctx.isPlaying();
  }

  inline int PlayerGL::isInit() {
    return ctx.isInit();
  }

  inline int PlayerGL::isPaused() {
    return ctx.isPaused();
  }

  inline void PlayerGL::draw(int x, int y, int w, int h) {
    glViewport(x, (vp[3] - y) - h, w, h); 
        draw();
    glViewport(0, 0, vp[2], vp[3]);
  }

  inline void PlayerGL::resize(int w, int h) {
    vp[2] = w;
    vp[3] = h;
  }

  /* ------------------------------------------------------------------ */
} /* namespace rxp */

#endif // RXP_CPP_PLAYER_GL_H


#if defined(RXP_PLAYER_GL_IMPLEMENTATION)

/* ------------------------------------------------------------------------*/
/* Embeddable OpenGL wrappers.                                             */
/* ------------------------------------------------------------------------*/
static int create_shader(GLuint* out, GLenum type, const char* src);           /* create a shader, returns 0 on success, < 0 on error. e.g. create_shader(&vert, GL_VERTEX_SHADER, DRAWER_VS); */
static int create_program(GLuint* out, GLuint vert, GLuint frag, int link);    /* create a program from the given vertex and fragment shader. returns 0 on success, < 0 on error. e.g. create_program(&prog, vert, frag, 1); */
static int print_shader_compile_info(GLuint shader);                           /* prints the compile info of a shader. returns 0 when shader compiled, < 0 on error. */
static int print_program_link_info(GLuint prog);                               /* prints the program link info. returns 0 on success, < 0 on error. */
/* ------------------------------------------------------------------------*/


namespace rxp {

  /* ------------------------------------------------------------------------*/

  static GLuint create_texture(int width, int height);
  static void on_player_event(Player* player, int event);
  static void on_player_video_frame(Player* player, rxp_packet* pkt);
  
  /* ------------------------------------------------------------------------*/

  PlayerGL::PlayerGL() 
    :vert(0)
    ,frag(0)
    ,prog(0)
    ,vao(0)
    ,tex_y(0)
    ,tex_u(0)
    ,tex_v(0)
    ,video_width(0)
    ,video_height(0)
    ,on_event(NULL)
    ,on_video_frame(NULL)
    ,user(NULL)
  {
  }

  PlayerGL::~PlayerGL() {

    if (ctx.isInit()) {
      ctx.shutdown();
    }

    if (0 != vert)  {   glDeleteShader(vert);            vert = 0;    }
    if (0 != frag)  {   glDeleteShader(frag);            frag = 0;    }
    if (0 != prog)  {   glDeleteProgram(prog);           prog = 0;    }
    if (0 != vao)   {   glDeleteVertexArrays(1, &vao);   vao = 0;     }
    if (0 != tex_y) {   glDeleteTextures(1, &tex_y);     tex_y = 0;   }
    if (0 != tex_u) {   glDeleteTextures(1, &tex_u);     tex_u = 0;   }
    if (0 != tex_v) {   glDeleteTextures(1, &tex_v);     tex_v = 0;   }

    ctx.on_event = NULL;
    ctx.on_video_frame = NULL;
    ctx.user = NULL;

    on_event = NULL;
    on_video_frame = NULL;
    user = NULL;
  }

  int PlayerGL::init(std::string filepath) {
    int r = 0;

    /* initialize the player. */
    r = ctx.init(filepath);
    if (0 != r) {
      return r;
    }

    /* we only create the shader once and free it in the destructor */
    if (0 == prog) {
      if (0 != create_shader(&vert, GL_VERTEX_SHADER, RXP_PLAYER_VS)) {
        r = -1;
        goto error;
      }
      if (0 != create_shader(&frag, GL_FRAGMENT_SHADER, RXP_PLAYER_FS)) {
        r = -3;
        goto error;
      }
      if (0 != create_program(&prog, vert, frag, 1)) {
        r = -4;
        goto error;
      }

      glGenVertexArrays(1, &vao);
      glUseProgram(prog);
      glUniform1i(glGetUniformLocation(prog, "u_ytex"), 0);
      glUniform1i(glGetUniformLocation(prog, "u_utex"), 1);
      glUniform1i(glGetUniformLocation(prog, "u_vtex"), 2);
    }

    glGetIntegerv(GL_VIEWPORT, vp);
    if (0 == vp[2] || 0 == vp[3]) {
      printf("Error: we couldn't get the viewport size.\n");
    }

    ctx.on_event = on_player_event;
    ctx.on_video_frame = on_player_video_frame;
    ctx.user = this;

    return r;

  error:
    if (ctx.isInit()) {
      ctx.shutdown();
    }
    if (0 != vert) {
      glDeleteShader(vert);
      vert = 0;
    }
    if (0 != frag) {
      glDeleteShader(frag);
      frag = 0;
    }
    if (0 != prog) {
      glDeleteProgram(prog);
      prog = 0;
    }
    if (0 != vao) {
      glDeleteVertexArrays(1, &vao);
      vao = 0;
    }
    ctx.on_event = NULL;
    ctx.on_video_frame = NULL;
    ctx.user = NULL;
    return r;
  }

  void PlayerGL::draw() {

    if (0 == tex_y) {
      return;
    }

#if !defined(NDEBUG)
    if (false == ctx.isInit()) {
      printf("Warning: cannot draw the GL player because it's not initialized.\n");
      return;
    }
    if (0 == prog) {
      printf("Error: it looks like we've not shader program yet, did you init()?\n");
      return;
    }
#endif    

    glUseProgram(prog);
    glBindVertexArray(vao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_y);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex_u);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex_v);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  int PlayerGL::shutdown() {
    int r = 0;

    /* shutdown the player context. */
    if (0 == ctx.isInit()) {
      r = ctx.shutdown();
      if (0 != r) {
        printf("Error: the player returned an error while trying to shutdown.\n");
      }
    }

    return r;
  }

  /* ------------------------------------------------------------------------*/

  static void on_player_event(Player* player, int event) {

    PlayerGL* gl = static_cast<PlayerGL*>(player->user);
    if (NULL == gl) {
      printf("Error: cannot PlayerGL handle, not supposed to happen!\n");
      return;
    }

    if (RXP_PLAYER_EVENT_RESET == event) {
      gl->shutdown();
    }
    else if (RXP_DEC_EVENT_AUDIO_INFO == event) {
      printf("Error: we got a file with audio but we currently do not yet "
             "support this in PlayerGL (the timing info is not update because "
             "the rxp_player_fill_audio_buffer() is not called).\n");
    }

    /* and dispatch the event to the user. */
    if (NULL != gl->on_event) {
      gl->on_event(gl, event);
    }
  }

  static void on_player_video_frame(Player* player, rxp_packet* pkt) {

    PlayerGL* gl = static_cast<PlayerGL*>(player->user);
    if (NULL == gl) {
      printf("Error: cannot PlayerGL handle, not supposed to happen!\n");
      return;
    }

    /* did the width/height change? if so, we need to recreate the textures. */
    /* @todo - test this code correctly, should work, but not tested by switching the video source. */
    if (0 != gl->tex_y && (pkt->img[0].width != gl->video_width || pkt->img[0].height != gl->video_height)) {
      glDeleteTextures(1, &gl->tex_y);
      glDeleteTextures(1, &gl->tex_u);
      glDeleteTextures(1, &gl->tex_v);
      gl->tex_y = 0;
      gl->tex_u = 0;
      gl->tex_v = 0;
    }

    if (gl->tex_y == 0) {

      gl->video_width = pkt->img[0].width;
      gl->video_height = pkt->img[0].height;

      /* create textures after we've decoded a frame. */
      gl->tex_y = create_texture(pkt->img[0].width, pkt->img[0].height);
      gl->tex_u = create_texture(pkt->img[1].width, pkt->img[1].height);
      gl->tex_v = create_texture(pkt->img[2].width, pkt->img[2].height);
    }

    glBindTexture(GL_TEXTURE_2D, gl->tex_y);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, pkt->img[0].stride);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pkt->img[0].width, pkt->img[0].height, GL_RED, GL_UNSIGNED_BYTE, pkt->img[0].data);

    glBindTexture(GL_TEXTURE_2D, gl->tex_u);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, pkt->img[1].stride);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pkt->img[1].width, pkt->img[1].height, GL_RED, GL_UNSIGNED_BYTE, pkt->img[1].data);

    glBindTexture(GL_TEXTURE_2D, gl->tex_v);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, pkt->img[2].stride);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pkt->img[2].width, pkt->img[2].height, GL_RED, GL_UNSIGNED_BYTE, pkt->img[2].data);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    /* and notify listener */
    if (gl->on_video_frame) {
      gl->on_video_frame(gl, pkt);
    }
  }

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

  /* ------------------------------------------------------------------------*/

} /* namespace rxp */


/* ------------------------------------------------------------------------*/
/* Embeddable OpenGL wrappers.                                             */
/* ------------------------------------------------------------------------*/

static int create_shader(GLuint* out, GLenum type, const char* src) {
 
  *out = glCreateShader(type);
  glShaderSource(*out, 1, &src, NULL);
  glCompileShader(*out);
  
  if (0 != print_shader_compile_info(*out)) {
    *out = 0;
    return -1;
  }
  
  return 0;
}

/* create a program, store the result in *out. when link == 1 we link too. returns -1 on error, otherwise 0 */
static int create_program(GLuint* out, GLuint vert, GLuint frag, int link) {
  *out = glCreateProgram();
  glAttachShader(*out, vert);
  glAttachShader(*out, frag);

  if(1 == link) {
    glLinkProgram(*out);
    if (0 != print_program_link_info(*out)) {
      *out = 0;
      return -1;
    }
  }

  return 0;
}

/* checks + prints program link info. returns 0 when linking didn't result in an error, on link erorr < 0 */
static int print_program_link_info(GLuint prog) {
  GLint status = 0;
  GLint count = 0;
  GLchar* error = NULL;
  GLsizei nchars = 0;

  glGetProgramiv(prog, GL_LINK_STATUS, &status);
  if(status) {
    return 0;
  }

  glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &count);
  if(count <= 0) {
    return 0;
  }

  error = (GLchar*)malloc(count);
  glGetProgramInfoLog(prog, count, &nchars, error);
  if (nchars <= 0) {
    free(error);
    error = NULL;
    return -1;
  }

  printf("\nPROGRAM LINK ERROR");
  printf("\n--------------------------------------------------------\n");
  printf("%s", error);
  printf("--------------------------------------------------------\n\n");

  free(error);
  error = NULL;
  return -1;
}

/* checks the compile info, if it didn't compile we return < 0, otherwise 0 */
static int print_shader_compile_info(GLuint shader) {

  GLint status = 0;
  GLint count = 0;
  GLchar* error = NULL;

  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if(status) {
    return 0;
  }

  error = (GLchar*) malloc(count);
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &count);
  if(count <= 0) {
    free(error);
    error = NULL;
    return 0;
  }

  glGetShaderInfoLog(shader, count, NULL, error);
  printf("\nSHADER COMPILE ERROR");
  printf("\n--------------------------------------------------------\n");
  printf("%s", error);
  printf("--------------------------------------------------------\n\n");

  free(error);
  error = NULL;
  return -1;
}

/* ------------------------------------------------------------------------*/



#endif // RXP_PLAYER_GL_IMPLEMENTATION

#undef RXP_PLAYER_GL_IMPLEMENTATION
