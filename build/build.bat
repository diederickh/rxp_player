; echo off
set d=%CD%
set bd=win-vs2012-x86_64
set bits=32
set type=%1
set cmake_bt="Release"
set cmake_gen="Visual Studio 11 Win64"
set cmake_opt=""

if "%type%" == "" (
  echo "Usage: build.bat [debug, release]"
  exit /b 2
)

if "%bits%" == "32" (
   set bd=win-vs2012-i386
   set cmake_gen="Visual Studio 11"
   set cmake_opt="-PPLAYER_USE_32BIT=1"
)

if "%type%" == "debug" (
   set bd="%bd%d"
   set cmake_bt="Debug"
)

if not exist "%d%\%bd%" (       
   mkdir %d%\%bd%
)


cd %d%\%bd%
cmake -DUSE_32BIT=1 -DCMAKE_BUILD_TYPE=%cmake_bt% -G %cmake_gen% %cmake_opt% ..\
cmake --build . --target install --config %cmake_bt%
cd %d%
cd ..\install\%bd%\bin
rxp_glfw_player.exe

