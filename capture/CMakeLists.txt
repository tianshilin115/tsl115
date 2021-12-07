cmake_minimum_required(VERSION 2.8.12.2)
project(capture)
add_definitions(-march=native)
add_definitions(-m64)
add_definitions(-DRTE_MACHINE_CPUFLAG_SSE)
add_definitions(-DRTE_MACHINE_CPUFLAG_SSE2)
add_definitions(-DRTE_MACHINE_CPUFLAG_SSE3)
add_definitions(-DRTE_MACHINE_CPUFLAG_SSSE3)
add_definitions(-DRTE_MACHINE_CPUFLAG_SSE4_1)
add_definitions(-DRTE_MACHINE_CPUFLAG_SSE4_2)
add_definitions(-rdynamic)
add_definitions(-fPIC)
add_definitions(-fpermissive)
add_definitions(-std=c++11)
set(CMAKE_CXX_COMPILER "g++")
#SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS  "-O0 -g3 -ggdb")
set(DPDK_LIB_PATH ${DPDK_LIB_PATH} "../../../lib/dpdk")
file(GLOB LIBS "${DPDK_LIB_PATH}/librte_*.so")
#头文件
include_directories(../../../include/dpdk
        ../../../include/dpdk/generic
        ../../../include/dpdk/exec-env
        ../../../include/xdpi
	/usr/include/libxml2/libxml
        /usr/include/libxml2/
        src)

##源文件 - capture
AUX_SOURCE_DIRECTORY(. SRC_Y)
aux_source_directory(src/CaptureThread SRC_Y)
aux_source_directory(src/Common SRC_Y)
aux_source_directory(src/FrameInterface SRC_Y)
aux_source_directory(src/Init/MemeryPool SRC_Y)
aux_source_directory(src/Init/Network SRC_Y)
#aux_source_directory(src/Init/Filter SRC_Y)
aux_source_directory(src/Parser SRC_Y)
aux_source_directory(src/FrameInterfaceCapture SRC_Y)
add_library(capture SHARED ${SRC_Y})
target_link_libraries(capture curl cjson ${LIBS})
