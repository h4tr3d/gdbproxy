cmake_minimum_required(VERSION 3.10)
PROJECT(boost-asio-proxy)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_C_STANDARD_REQUIRED On)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include_directories(cxxopts/include)

#include(CheckIncludeFiles)
#CHECK_INCLUDE_FILES(asio.hpp HAVE_ASIO_HPP LANGUAGE CXX)

# need to check asio.hpp
set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${CMAKE_THREAD_LIBS_INIT}")
set(CMAKE_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES} ${CMAKE_SOURCE_DIR}/cxxopts/include")

include(CheckIncludeFileCXX)
CHECK_INCLUDE_FILE_CXX(asio.hpp HAVE_ASIO_HPP)
check_include_file_cxx(cxxopts.hpp HAVE_CXXOPTS_HPP)

if (NOT HAVE_ASIO_HPP)
  message(FATAL_ERROR "Asio must be installed (https://think-async.com/Asio/)")
endif()

if (NOT HAVE_CXXOPTS_HPP)
  message(FATAL_ERROR "Cxxopts not found. Did you run 'git submodule update'?")
endif()

set (SOURCES
  proxy.cpp
  proxy-server.cpp
  proxy-conn.cpp
  gdb-packet.cpp
  gdb_packets.cpp
  target.cpp
        target_mb_freertos.cpp)

set (hex2bin_SOURCES hex2bin.cpp gdb_packets.cpp gdb-packet.cpp)

add_executable(gdbproxy ${SOURCES})
target_link_libraries(gdbproxy Threads::Threads)
set_target_properties(gdbproxy PROPERTIES LINKER_LANGUAGE CXX)

add_executable(hex2bin ${hex2bin_SOURCES})
target_link_libraries(hex2bin Threads::Threads)
set_target_properties(hex2bin PROPERTIES LINKER_LANGUAGE CXX)
