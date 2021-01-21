project(ais_log)
cmake_minimum_required(VERSION 2.6)

set(SOURCE_FILES
    ${AIS_ROOT_PATH}/Common/src/ais_log.c
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -fvisibility=hidden -Wno-pointer-to-int-cast")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -fvisibility=hidden -Wno-pointer-to-int-cast")

add_definitions(
    -D__LINUX
    -D__AGL__
    -D__USE_GNU
    -D_GNU_SOURCE
)

add_library (ais_log SHARED ${SOURCE_FILES})


include_directories (${MM-OSAL_INCPATH})
include_directories (${AIS_ROOT_PATH}/Common/inc)


include_directories (${SYSROOTINC_PATH})
include_directories (${SYSROOT_INCLUDEDIR})

link_directories(${SYSROOT_LIBDIR})

target_link_libraries (ais_log log)
target_link_libraries (ais_log glib-2.0)
target_link_libraries (ais_log cutils)
target_link_libraries (ais_log mmosal)

install (TARGETS ais_log LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
