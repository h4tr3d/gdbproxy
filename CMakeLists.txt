cmake_minimum_required(VERSION 3.10)
PROJECT(boost-asio-proxy)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_C_STANDARD_REQUIRED On)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

#include(CheckIncludeFiles)
#CHECK_INCLUDE_FILES(asio.hpp HAVE_ASIO_HPP LANGUAGE CXX)

# need to check asio.hpp
set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${CMAKE_THREAD_LIBS_INIT}")

include(CheckIncludeFileCXX)
CHECK_INCLUDE_FILE_CXX(asio.hpp HAVE_ASIO_HPP)

if (NOT HAVE_ASIO_HPP)
  message(FATAL_ERROR "Asio must be installed (https://think-async.com/Asio/)")
endif()

set (SOURCES
  proxy.cpp
  proxy-server.cpp
  proxy-conn.cpp
  gdb-packet.cpp)

add_executable(gdbproxy ${SOURCES})
target_link_libraries(gdbproxy Threads::Threads)
set_target_properties(gdbproxy PROPERTIES LINKER_LANGUAGE CXX)
