# IMPORTANT NOTE
The AVS Device SDK supports wake word detectors from Sensory and KITT.ai, but not support platform of A113D.  
In this project, used CMU Sphinx to achieved.


# CROSS COMPILE
This is my cmake command using the toolchain.cmake toolchain file:
```
$ cmake -DCMAKE_TOOLCHAIN_FILE=~/toolchain.cmake ~/sdk-folder/sdk-source/avs-device-sdk/ 
        -DSENSORY_KEY_WORD_DETECTOR=OFF -DGSTREAMER_MEDIA_PLAYER=ON 
        -DCMAKE_PREFIX_PATH=~/A113D_20170831_alexa/output/mesonaxg_s400_32_release/ -DPORTAUDIO=ON 
        -DPORTAUDIO_LIB_PATH=~/sdk-folder/third-party/portaudio/lib/.libs/libportaudio.a 
        -DPORTAUDIO_INCLUDE_DIR=~/sdk-folder/third-party/portaudio/include/ 
        -DCMAKE_PREFIX_PATH=~/A113D_20170831_alexa/output/mesonaxg_s400_32_release/ 
        -DCMAKE_INSTALL_PREFIX=~/A113D_20170831_alexa/output/mesonaxg_s400_32_release/ 
        -DCMAKE_BUILD_TYPE=DEBUG -Wno-dev 
```
The toolchain.cmake toolchain file for ARM:
```
set(CMAKE_SYSTEM_NAME Linux)
set(TOOLCHAIN_PREFIX $ENV{TOOLCHAIN_PATH}/bin/arm-linux-gnueabihf-)

INCLUDE(CMakeForceCompiler)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)

set(CMAKE_SYSROOT $ENV{TOOLCHAIN_PATH}/arm-linux-gnueabihf/sysroot)

set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(COMPILER_FLAGS " -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -Os")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMPILER_FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILER_FLAGS}" CACHE STRING "" FORCE)
```
Environments variables which I exports below:
```
export TOOLCHAIN_PATH=~/A113D_20170831_alexa/output/mesonaxg_s400_32_release/host/usr
export PKG_CONFIG_DIR=""
export PKG_CONFIG_LIBDIR="${TOOLCHAIN_PATH}/lib/pkgconfig:${TOOLCHAIN_PATH}/arm-linux-gnueabihf/sysroot/usr/lib/pkgconfig"
export PKG_CONFIG_PATH="${TOOLCHAIN_PATH}/lib/pkgconfig:${TOOLCHAIN_PATH}/arm-linux-gnueabihf/sysroot/usr/lib/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="${TOOLCHAIN_PATH}/arm-linux-gnueabihf/sysroot"
```


# CITE SOURCES
[AVS Device SDK](https://github.com/alexa/avs-device-sdk)  
[CMU Sphinx](https://cmusphinx.github.io/)
