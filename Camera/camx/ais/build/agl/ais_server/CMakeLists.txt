project(ais_server)
cmake_minimum_required(VERSION 2.6)

set(SRC_PATH "${AIS_ROOT_PATH}/CameraMulticlient/")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wno-int-to-pointer-cast")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wno-int-to-pointer-cast")


add_definitions(
    -D_GNU_SOURCE
    -D__LINUX
    -D__AGL__
)

set(SOURCE_FILES
    ${SRC_PATH}/common/src/linux/ais_conn.c
    ${SRC_PATH}/common/src/ais_event_queue.c
    ${SRC_PATH}/server/src/ais_server.c
)
add_executable (ais_server ${SOURCE_FILES})


include_directories (${AIS_ROOT_PATH}/API/inc)
include_directories (${AIS_ROOT_PATH}/Common/inc)
include_directories (${AIS_ROOT_PATH}/Engine/inc)
include_directories (${AIS_ROOT_PATH}/CameraQueue/CameraQueue/inc)
include_directories (${AIS_ROOT_PATH}/CameraMulticlient/common/inc)
include_directories (${AIS_ROOT_PATH}/CameraMulticlient/server/inc)
include_directories (${AIS_ROOT_PATH}/CameraOSServices/CameraOSServices/inc)
include_directories (${AIS_ROOT_PATH}/CameraOSServices/CameraOSServicesMMOSAL/inc)

include_directories (${SYSROOTINC_PATH})
include_directories (${SYSROOT_INCLUDEDIR})

link_directories(${SYSROOT_LIBDIR})

target_link_libraries (ais_server ais)
target_link_libraries (ais_server ais_log)
target_link_libraries (ais_server glib-2.0)
target_link_libraries (ais_server pthread)
target_link_libraries (ais_server cutils)

install (TARGETS ais_server DESTINATION ${CMAKE_INSTALL_BINDIR})
