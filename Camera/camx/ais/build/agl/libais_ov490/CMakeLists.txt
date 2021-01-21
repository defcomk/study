project(ais_ov490)
cmake_minimum_required(VERSION 2.6)

aux_source_directory(${AIS_ROOT_PATH}/ImagingInputs/SensorLibs/ov490_lib/src SENSOR_SRCS)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error -fvisibility=hidden")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error -fvisibility=hidden")

set(SOURCE_FILES
    ${SENSOR_SRCS}
)
add_library (ais_ov490 SHARED ${SOURCE_FILES})
add_definitions (-DCAMERA_CONFIG)

include_directories (${AIS_ROOT_PATH}/API/inc)
include_directories (${AIS_ROOT_PATH}/Common/inc)
include_directories (${AIS_ROOT_PATH}/CameraDevice/inc)
include_directories (${AIS_ROOT_PATH}/CameraEventLog/inc)
include_directories (${AIS_ROOT_PATH}/CameraOSServices/CameraOSServices/inc)
include_directories (${AIS_ROOT_PATH}/CameraOSServices/CameraOSServicesMMOSAL/inc)
include_directories (${AIS_ROOT_PATH}/CameraPlatform/inc)
include_directories (${AIS_ROOT_PATH}/CameraPlatform/linux)
include_directories (${AIS_ROOT_PATH}/ImagingInputs/ImagingInputDriver/inc)

include_directories (${SYSROOTINC_PATH})
include_directories (${SYSROOT_INCLUDEDIR})

link_directories(${SYSROOT_LIBDIR})

target_link_libraries (ais_ov490 ais)
target_link_libraries (ais_ov490 glib-2.0)
target_link_libraries (ais_ov490 cutils)

install (TARGETS ais_ov490 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
