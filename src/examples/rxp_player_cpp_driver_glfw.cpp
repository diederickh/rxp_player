/*
 

  BASIC GLFW + GLXW WINDOW AND OPENGL SETUP 
  ------------------------------------------
  See https://gist.github.com/roxlu/6698180 for the latest version of the example.


  -------

  This file shows how you use the rxp::PlayerGL to draw a .ogg video file
  that is encoded with Theora. At this moment the rxp::Player and rxp::PlayerGL
  do not work with .ogg files that also contain audio. It's on the todo list. 
 
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

#include <rxp_player/Player.h>

#define RXP_PLAYER_GL_IMPLEMENTATION
#include <rxp_player/PlayerGL.h>
 
void button_callback(GLFWwindow* win, int bt, int action, int mods);
void cursor_callback(GLFWwindow* win, double x, double y);
void key_callback(GLFWwindow* win, int key, int scancode, int action, int mods);
void char_callback(GLFWwindow* win, unsigned int key);
void error_callback(int err, const char* desc);
void resize_callback(GLFWwindow* window, int width, int height);

rxp::PlayerGL player;
static void on_event(rxp::PlayerGL* p, int event);
static int setup_player(rxp::PlayerGL* p);         /* initialized the player + starts playback */
bool restart = false;

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
 
  win = glfwCreateWindow(w, h, "rxp_player cpp driver", NULL, NULL);
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

  if (0 != setup_player(&player)) {
    exit(EXIT_FAILURE);
  }

  player.on_event = on_event;
  
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

    if (true == restart) {
      setup_player(&player);
      restart = false;
    }

    player.update();
    player.draw();
    player.draw(10, 10, 640, 360);

    glfwSwapBuffers(win);
    glfwPollEvents();
  }
 
  player.stop();
  player.shutdown();

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

/* gets called from a different thread, so we can't do any GL here! */
static void on_event(rxp::PlayerGL* p, int event) {

  if (NULL == p) {
    printf("Error: playergl handle is invalid.\n");
    return;
  }

  if (RXP_PLAYER_EVENT_RESET == event) {
    restart = true;
  }
}


static int setup_player(rxp::PlayerGL* gl) {

  if (NULL == gl) {
    printf("Error: invalid playergl pointer.\n");
    return -1;
  }

  printf("+ We're going to (re)start the player.\n");

  std::string video = rx_get_exe_path() +"tiny.ogg";
  if (0 != gl->init(video)) {
    printf("Error: canot open the video file.\n");
    ::exit(EXIT_FAILURE);
  }

  if (0 != gl->play()) {
    printf("Error: cannot start playing.");
    ::exit(EXIT_FAILURE);
  }

  return 0;
}

