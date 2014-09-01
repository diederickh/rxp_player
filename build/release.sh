#!/bin/sh

if [ ! -d build.release ] ; then 
    mkdir build.release
fi

set id=""
if [ "$(uname)" == "Darwin" ] ; then 
    id="./../../install/mac-clang-x86_64//"
else 
    id="./../../install/linux-gcc-x86_64/"
fi     

cd build.release
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${id} ../ 
cmake --build . --target install

if [ "$(uname)" == "Darwin" ] ; then 
    cd ./../../install/mac-clang-x86_64/bin/
    #./rxp_glfw_player
    ./rxp_cpp_glfw_player
    
else 
    cd ./../../install/linux-gcc-x86_64/bin/
    ./rxp_glfw_player
fi

