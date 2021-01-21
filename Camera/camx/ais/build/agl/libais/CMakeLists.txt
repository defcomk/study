project(ais)
cmake_minimum_required(VERSION 2.6)

aux_source_directory(${AIS_ROOT_PATH}/CameraEventLog/src EVENT_SRCS)
aux_source_directory(${AIS_ROOT_PATH}/CameraQueue/CameraQueueSCQ/src QUEUE_SRCS)
aux_source_directory(${AIS_ROOT_PATH}/Engine/src ENGINE_SRCS)
aux_source_directory(${AIS_ROOT_PATH}/CameraOSServices/CameraOSServicesMMOSAL/src OS_SRCS)
aux_source_directory(${AIS_ROOT_PATH}/CameraDevice/src/ DEVICE_SRCS)
aux_source_directory(${AIS_ROOT_PATH}/CameraPlatform/linux/ PLATFORM_SRCS)
aux_source_directory(${AIS_ROOT_PATH}/ImagingInputs/ImagingInputDriver/src INPUT_SRCS)
aux_source_directory(${AIS_ROOT_PATH}/ImagingInputs/ImagingInputDriver/src/linux INPUT_LINUX_SRCS)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -fvisibility=hidden")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -fvisibility=hidden")

add_definitions(
    -D_GNU_SOURCE
    -DPRINTF_ENABLED
    -D__LINUX
    -D__AGL__
)

set(SOURCE_FILES
    ${EVENT_SRCS}
    ${QUEUE_SRCS}
    ${ENGINE_SRCS}
    ${OS_SRCS}
    ${DEVICE_SRCS}
    ${DEVICE_S_SRCS}
    ${PLATFORM_SRCS}
    ${INPUT_SRCS}
    ${INPUT_LINUX_SRCS}
)
add_library (ais SHARED ${SOURCE_FILES})


include_directories (${MM-OSAL_INCPATH})
include_directories (${AIS_ROOT_PATH}/API/inc)
include_directories (${AIS_ROOT_PATH}/CameraDevice/inc)
include_directories (${AIS_ROOT_PATH}/CameraConfig/inc)
include_directories (${AIS_ROOT_PATH}/CameraEventLog/inc)
include_directories (${AIS_ROOT_PATH}/Common/inc)
include_directories (${AIS_ROOT_PATH}/CameraQueue/CameraQueue/inc)
include_directories (${AIS_ROOT_PATH}/CameraOSServices/CameraOSServices/inc)
include_directories (${AIS_ROOT_PATH}/CameraOSServices/CameraOSServicesMMOSAL/inc)
include_directories (${AIS_ROOT_PATH}/CameraPlatform/inc)
include_directories (${AIS_ROOT_PATH}/CameraPlatform/linux)
include_directories (${AIS_ROOT_PATH}/Engine/inc)
include_directories (${AIS_ROOT_PATH}/HWDrivers/API/MIPICSIDriver)
include_directories (${AIS_ROOT_PATH}/HWDrivers/API/IFEDriver)
include_directories (${AIS_ROOT_PATH}/ImagingInputs/ImagingInputDriver/inc)
include_directories (${AIS_ROOT_PATH}/ImagingInputs/ImagingInputDriver/src)
include_directories (${AIS_ROOT_PATH}/CameraMulticlient/common/inc)
include_directories (${AIS_ROOT_PATH}/Engine/src)
include_directories (${AIS_ROOT_PATH})

include_directories (${SYSROOTINC_PATH})
include_directories (${SYSROOT_INCLUDEDIR})

link_directories(${SYSROOT_LIBDIR})

target_link_libraries (ais dl)
target_link_libraries (ais glib-2.0)
target_link_libraries (ais pthread)
target_link_libraries (ais cutils)
target_link_libraries (ais mmosal)

install (TARGETS ais LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
