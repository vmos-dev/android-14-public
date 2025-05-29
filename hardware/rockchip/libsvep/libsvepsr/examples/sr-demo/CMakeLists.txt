# set minimum cmake version
cmake_minimum_required(VERSION 3.5)

# project name and language
project(sr_demo)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -O2")

if(ENABLE_ASAN)
  message(STATUS "BUILD WITH ADDRESS SANTITIZE")
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
  set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()

message(STATUS "Build test for soc: ${TARGET_SOC} PATH=${CMAKE_CURRENT_SOURCE_DIR}")

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
else()
  if(CMAKE_C_COMPILER MATCHES "aarch64")
    set(LIB_ARCH aarch64)
  else()
    set(LIB_ARCH armhf)
  endif()
endif()

message(STATUS "Build for soc: ${TARGET_SOC}")

# SR libary
set(SR_RESOURCE_PATH ${CMAKE_SOURCE_DIR}/../../lib/${CMAKE_SYSTEM_NAME}/${TARGET_SOC})

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(SR_LIB_ARCH_PATH ${SR_RESOURCE_PATH}/${CMAKE_ANDROID_ARCH_ABI})
  set(SR_LIB_HEAD_PATH ${SR_RESOURCE_PATH}/include)
  set(SR_LIBS
    ${SR_LIB_ARCH_PATH}/libsvepsr.so
    ${SR_LIB_ARCH_PATH}/libOpenCL.so
    ${SR_LIB_ARCH_PATH}/librknnrt-svep.so)
  include_directories(${SR_LIB_HEAD_PATH})
else()
  set(SR_LIB_ARCH_PATH ${SR_RESOURCE_PATH}/${LIB_ARCH})
  set(SR_LIB_HEAD_PATH ${SR_RESOURCE_PATH}/include)
  set(SR_LIBS
    ${SR_LIB_ARCH_PATH}/libsvepsr.so
    ${SR_LIB_ARCH_PATH}/libOpenCL.so
    ${SR_LIB_ARCH_PATH}/librknnrt-svep.so)
  include_directories(${SR_LIB_HEAD_PATH})
endif()

# demo
set(sr_demo_SRCS
  src/main.cpp
  src/worker.cpp
  src/AsyncWorker.cpp
)
add_executable(sr_demo ${sr_demo_SRCS})

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(sr_demo
  ${SR_LIBS}
  z
  dl
)

if(ANDROID)
  target_link_libraries(sr_demo log z)
else()
  target_link_libraries(sr_demo pthread)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  INSTALL(TARGETS sr_demo DESTINATION ${CMAKE_SYSTEM_NAME}/${TARGET_SOC}/${CMAKE_ANDROID_ARCH_ABI}/bin)
  INSTALL(FILES ${SR_LIBS}  DESTINATION ${CMAKE_SYSTEM_NAME}/${TARGET_SOC}/${CMAKE_ANDROID_ARCH_ABI}/lib)
else()
  INSTALL(TARGETS sr_demo DESTINATION ${CMAKE_SYSTEM_NAME}/${TARGET_SOC}/${LIB_ARCH}/bin)
  INSTALL(FILES ${SR_LIBS} DESTINATION ${CMAKE_SYSTEM_NAME}/${TARGET_SOC}/${LIB_ARCH}/lib)
endif()

set(INSTALL_HEAD_PATH ${CMAKE_SYSTEM_NAME}/${TARGET_SOC}/include)
INSTALL(DIRECTORY ${SR_LIB_HEAD_PATH}/ DESTINATION ${INSTALL_HEAD_PATH})
INSTALL(FILES ../../resources/osd/SrOsd.ttf DESTINATION resource)
