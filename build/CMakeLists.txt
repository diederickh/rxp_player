cmake_minimum_required(VERSION 2.8)

option(BUILD_EXAMPLES "Build examples" ON)

include(${CMAKE_CURRENT_LIST_DIR}/Triplet.cmake)

set(rxp_player "rxp_player")
set(rxp_glfw_player "rxp_glfw_player")

set(sd ${CMAKE_CURRENT_LIST_DIR}/../src/rxp_player/)
set(bd ${CMAKE_CURRENT_LIST_DIR}/../)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
  set(rxp_player "${rxp_player}_debug")
  set(rxp_glfw_player "${rxp_glfw_player}_debug")
endif()

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}/../src
  ${CMAKE_CURRENT_LIST_DIR}/../include
  ${extern_include_dir}
  ${bd}/extern/tinylib/src
)

set(rxp_player_sources
  ${sd}/rxp_ringbuffer.c
  ${sd}/rxp_packets.c
  ${sd}/rxp_tasks.c
  ${sd}/rxp_scheduler.c
  ${sd}/rxp_clock.c
  ${sd}/rxp_decoder.c
  ${sd}/rxp_player.c
)

add_library(${rxp_player} ${rxp_player_sources})

if(APPLE)

  find_library(fr_corefoundation CoreFoundation)
  find_library(fr_cocoa Cocoa)
  find_library(fr_opengl OpenGL)
  find_library(fr_iokit IOKit)
  find_library(fr_corevideo CoreVideo)
  find_library(fr_audio_unit AudioUnit)
  find_library(fr_core_audio CoreAudio)
  find_library(fr_audio_toolbox AudioToolbox)

  set(app_libs
    ${extern_lib_dir}/libglfw3.a
    ${extern_lib_dir}/libuv.a
    ${extern_lib_dir}/libpng.a
    ${extern_lib_dir}/libtheora.a
    ${extern_lib_dir}/libogg.a
    ${extern_lib_dir}/libvorbis.a
    ${extern_lib_dir}/libcubeb.a
    ${fr_corefoundation}
    ${fr_cocoa}
    ${fr_opengl}
    ${fr_iokit}  
    ${fr_corevideo}
    ${fr_audio_unit}
    ${fr_core_audio}
    ${fr_audio_toolbox}
    -lz
    )
elseif(UNIX)
  set(app_libs
    ${extern_lib_dir}
    ${extern_lib_dir}/libglfw3.a
    ${extern_lib_dir}/libpng.a
    ${extern_lib_dir}/libtheora.a
    ${extern_lib_dir}/libvorbis.a
    ${extern_lib_dir}/libcubeb.a
    ${extern_lib_dir}/libz.a
    ${extern_lib_dir}/libogg.a
    ${extern_lib_dir}/libuv.a
    GL
    X11
    Xxf86vm
    Xrandr
    pthread
    dl
    Xi
    asound
    )
elseif(WIN32)
  set(app_libs
    ${extern_lib_dir}glfw3.lib
    ${extern_lib_dir}libpng16_static.lib
    ${extern_lib_dir}libtheora_static.lib
    ${extern_lib_dir}libvorbis_static.lib
    ${extern_lib_dir}libogg_static.lib
    ${extern_lib_dir}zlibstatic.lib
    ${extern_lib_dir}cubeb_static.lib
    ${extern_lib_dir}libuv.lib
    Opengl32.lib
    ws2_32.lib
    psapi.lib
    iphlpapi.lib
    )
endif()

#add_executable(${app} ${app_sources})

if(UNIX AND NOT APPLE OR WIN32)
  list(APPEND example_sources ${extern_source_dir}/GLXW/glxw.c)
#  message(STATUS "Adding glxw.c for GL-function loading.")
endif()

if(BUILD_EXAMPLES)

  add_executable(${rxp_glfw_player} ${bd}/src/examples/rxp_player_glfw.cpp ${example_sources})
  target_link_libraries(${rxp_glfw_player} ${rxp_player} ${app_libs})
  install(TARGETS ${rxp_glfw_player} DESTINATION bin)
  install(FILES ${extern_lib_dir}/../bin/libuv.dll DESTINATION bin)
  
  if(NOT EXISTS ${install_dir}/bin/big_buck_bunny_720p_stereo.ogg)
    message(STATUS "Downloading example video (only once).")
    set(video_url "http://blender-mirror.kino3d.org/peach/bigbuckbunny_movies/big_buck_bunny_720p_stereo.ogg")
    file(DOWNLOAD "${video_url}" ${install_dir}/bin/big_buck_bunny_720p_stereo.ogg SHOW_PROGRESS TIMEOUT 600 STATUS status)
  endif()

endif()

install(DIRECTORY ${bd}/include/ DESTINATION include)
install(TARGETS ${rxp_player} ARCHIVE DESTINATION lib)
