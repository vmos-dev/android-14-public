# set minimum cmake version
cmake_minimum_required(VERSION 3.5)

# project name and language
project(vendor-storage-test)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2 -fPIC -pthread")

set(LIB_ARCH aarch64)

# demo
add_executable(vendor-storage-test main.cpp)


INSTALL(TARGETS vendor-storage-test DESTINATION bin)

