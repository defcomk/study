project(ais_diagnostic)
cmake_minimum_required(VERSION 2.6)

set(SRC_PATH "${AIS_ROOT_PATH}/Diagnostic/")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wno-int-to-pointer-cast")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wno-int-to-pointer-cast")


add_definitions(
    -D__LINUX
    -D__AIS_DIAG_CLIENT_
    -D__AGL__
)

set(SOURCE_FILES
    ${SRC_PATH}/src/client/ais_diagnostic.cpp
    ${SRC_PATH}/src/client/ais_diag_cmdprocessor.cpp
    ${SRC_PATH}/src/client/ais_diag_cmdprocessor_common.cpp
    ${SRC_PATH}/src/client/ais_diag_cmdprocessor_log.cpp
    ${SRC_PATH}/src/client/ais_diag_cmdprocessor_hw.cpp
    ${SRC_PATH}/src/client/ais_diag_parser.cpp
    ${SRC_PATH}/src/common/linux/ais_diag_client_ipc.c
    ${SRC_PATH}/src/client/linux/ais_diag_hw.c
)
add_executable (ais_diagnostic ${SOURCE_FILES})


include_directories (${MM-OSAL_INCPATH})
include_directories (${AIS_ROOT_PATH}/API/inc)
include_directories (${AIS_ROOT_PATH}/CameraEventLog/CameraEventLog/inc)
include_directories (${AIS_ROOT_PATH}/Common/inc)
include_directories (${AIS_ROOT_PATH}/CameraQueue/CameraQueue/inc)
include_directories (${AIS_ROOT_PATH}/CameraOSServices/CameraOSServices/inc)
include_directories (${AIS_ROOT_PATH}/CameraOSServices/CameraOSServicesMMOSAL/inc)
include_directories (${AIS_ROOT_PATH}/Diagnostic/inc)
include_directories (${AIS_ROOT_PATH}/Diagnostic/src/common/)
include_directories (${AIS_ROOT_PATH})

include_directories (${SYSROOTINC_PATH})
include_directories (${SYSROOT_INCLUDEDIR})

link_directories(${SYSROOT_LIBDIR})


target_link_libraries (ais_diagnostic glib-2.0)
target_link_libraries (ais_diagnostic pthread)
target_link_libraries (ais_diagnostic cutils)
target_link_libraries (ais_diagnostic ais_log)

install (TARGETS ais_diagnostic DESTINATION ${CMAKE_INSTALL_BINDIR})