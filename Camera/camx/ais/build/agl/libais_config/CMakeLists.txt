project(ais_config)
cmake_minimum_required(VERSION 2.6)

aux_source_directory(${AIS_ROOT_PATH}/CameraConfig/src ADP_DIR_SRCS)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -fvisibility=hidden")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -fvisibility=hidden")

add_definitions(

)

set(SOURCE_FILES
    ${ADP_DIR_SRCS}
)


add_library (ais_config SHARED ${SOURCE_FILES})


include_directories (${AIS_ROOT_PATH}/API/inc)
include_directories (${AIS_ROOT_PATH}/Common/inc)
include_directories (${AIS_ROOT_PATH}/ImagingInputs/ImagingInputDriver/inc)
include_directories (${AIS_ROOT_PATH}/CameraDevice/inc)
include_directories (${AIS_ROOT_PATH}/CameraConfig/inc)
include_directories (${AIS_ROOT_PATH}/CameraOSServices/CameraOSServices/inc)
include_directories (${AIS_ROOT_PATH}/CameraOSServices/CameraOSServicesMMOSAL/inc)
include_directories (${AIS_ROOT_PATH}/CameraPlatform/inc)
include_directories (${AIS_ROOT_PATH}/CameraPlatform/linux)

include_directories (${SYSROOTINC_PATH})
include_directories (${SYSROOT_INCLUDEDIR})

link_directories(${SYSROOT_LIBDIR})

target_link_libraries (ais_config ais)
target_link_libraries (ais_config glib-2.0)
target_link_libraries (ais_config cutils)

install (TARGETS ais_config LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
