cmake_minimum_required(VERSION 2.8)
project(nhugens_chuck)

include_directories(${CHUCK_PATH}/src)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(LINUX 1)
endif()

if(LINUX)
    add_definitions(-D__LINUX_ALSA__)
    add_definitions(-D__PLATFORM_LINUX__)
endif()
if(APPLE)
    add_definitions(-D__MACOSX_CORE__)
endif()

set(CMAKE_SHARED_MODULE_PREFIX "")
set(CMAKE_SHARED_MODULE_SUFFIX ".chug")

add_library("NHHall_chuck" MODULE "NHHall.cpp")
