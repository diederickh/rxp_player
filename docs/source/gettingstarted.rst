***************
Getting Started
***************

.. highlight:: c

To compile rxp_player you need to:

- Clone the repository 
- Make sure you compiled the dependencies (or use the one we provide)
- Compile the library with the packaged build script

Building the library
====================

Clone the rxp_player repository from github:

::

    git clone git@github.com:roxlu/rxp_player.git


Dependencies
------------

rxp_player depends on the follow libraries

 * `libogg`_
 * `libvorbis`_
 * `libtheora`_
 * `libuv`_

Furthermore the example that is part of the library, which implements 
a fully working video player needs a couple of other libraries. We provide
precompiled libraries that are tested on Mac/Win/Arch-Linux and will be
automatically downloaded upon first build. These libraries are:

 * `cubeb`_
 * `glfw`_
 * `glxw`_
 * `tinylib`_

Compiling rxp_player on Mac and Linux
-------------------------------------

We provide build scripts for both linux, mac and windows. We're using
CMake for the build system. By default the CMake file will download the
dependencies and necessary test files the first time you execute the
scripts. To build execute:

::

    cd build
    ./release.sh

This will automatically download a test video and starts the test
application. It will also build a `librxp_player.a` file and copies it 
to the root `install` directory for you compiler and operation system.


Compiling on Windows
--------------------

@TODO

Libraries to link with on Mac
-----------------------------

When you want to link with `librxpplayer` in your application you need 
to link with the following frameworks and libraries on mac.

 * CoreFoundation
 * Cocoa
 * OpenGL
 * IOKit
 * CoreVideo
 * AudioUnit
 * CoreAudio
 * AudioToolbox

.. _libogg: http://downloads.xiph.org/releases/ogg/libogg-1.3.1.tar.gz
.. _libvorbis: http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.4.tar.gz
.. _libtheora: http://downloads.xiph.org/releases/theora/libtheora-1.1.1.zip
.. _libuv: http://downloads.xiph.org/releases/theora/libtheora-1.1.1.zip
.. _cubeb: https://github.com/kinetiknz/cubeb
.. _glfw: http://www.glfw.org/
.. _glxw: https://github.com/rikusalminen/glxw
.. _tinylib: https://github.com/roxlu/tinylib
