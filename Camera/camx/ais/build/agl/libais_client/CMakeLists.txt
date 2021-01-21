project(ais_client)
cmake_minimum_required(VERSION 2.6)

if ("$ENV{AIS_MACHINE_TYPE}" STREQUAL "HYP")
MESSAGE( STATUS "Hypervisor  build")

set(SOURCE_FILES
    ${AIS_ROOT_PATH}/CameraMulticlient/client/src/ais_client.c
    ${AIS_ROOT_PATH}/CameraMulticlient/client/src/qcarcam.c
    ${AIS_ROOT_PATH}/CameraMulticlient/common/src/ais_event_queue.c
    ${AIS_ROOT_PATH}/CameraMulticlient/common/src/hypervisor/ais_conn.c
    ${AIS_ROOT_PATH}/CameraQueue/CameraQueueSCQ/src/CameraQueue.c
    ${AIS_ROOT_PATH}/CameraOSServices/CameraOSServicesMMOSAL/src/CameraOSServices.c
    ${AIS_ROOT_PATH}/Common/src/ais_log.c
)
add_definitions(
     -DUSE_HYP
)
else ("$ENV{AIS_MACHINE_TYPE}" STREQUAL "HYP")
MESSAGE( STATUS "Metal build")
set(SOURCE_FILES
    ${AIS_ROOT_PATH}/CameraMulticlient/client/src/ais_client.c
    ${AIS_ROOT_PATH}/CameraMulticlient/client/src/qcarcam.c
    ${AIS_ROOT_PATH}/CameraMulticlient/common/src/ais_event_queue.c
    ${AIS_ROOT_PATH}/CameraMulticlient/common/src/linux/ais_conn.c
    ${AIS_ROOT_PATH}/CameraQueue/CameraQueueSCQ/src/CameraQueue.c
    ${AIS_ROOT_PATH}/CameraOSServices/CameraOSServicesMMOSAL/src/CameraOSServices.c
    ${AIS_ROOT_PATH}/Common/src/ais_log.c
)
endif ("$ENV{AIS_MACHINE_TYPE}" STREQUAL "HYP")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -fvisibility=hidden -Wno-pointer-to-int-cast")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -fvisibility=hidden -Wno-pointer-to-int-cast")

add_definitions(
    -D__LINUX
    -D__AGL__
    -D__USE_GNU
    -D_GNU_SOURCE
)

add_library (ais_client SHARED ${SOURCE_FILES})


include_directories (${MM-OSAL_INCPATH})
include_directories (${AIS_ROOT_PATH}/API/inc)
include_directories (${AIS_ROOT_PATH}/Common/inc)
include_directories (${AIS_ROOT_PATH}/Engine/inc)
include_directories (${AIS_ROOT_PATH}/CameraMulticlient/common/inc)
include_directories (${AIS_ROOT_PATH}/CameraMulticlient/client/inc)
include_directories (${AIS_ROOT_PATH}/CameraMulticlient/client/src)
include_directories (${AIS_ROOT_PATH}/CameraEventLog/inc)
include_directories (${AIS_ROOT_PATH}/CameraQueue/CameraQueue/inc)
include_directories (${AIS_ROOT_PATH}/CameraOSServices/CameraOSServices/inc)
include_directories (${AIS_ROOT_PATH}/CameraOSServices/CameraOSServicesMMOSAL/inc)

include_directories (${SYSROOTINC_PATH})
include_directories (${SYSROOT_INCLUDEDIR})

link_directories(${SYSROOT_LIBDIR})

target_link_libraries (ais_client glib-2.0)
target_link_libraries (ais_client cutils)
target_link_libraries (ais_client mmosal)
if ("$ENV{AIS_MACHINE_TYPE}" STREQUAL "HYP")
target_link_libraries (ais_client uhab)
endif ("$ENV{AIS_MACHINE_TYPE}" STREQUAL "HYP")

install (TARGETS ais_client LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
