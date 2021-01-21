project(ccidbgr)
cmake_minimum_required(VERSION 2.6)

set(SRC_PATH "${AIS_ROOT_PATH}/test/ccidbgr/la/")


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wno-int-to-pointer-cast")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wno-int-to-pointer-cast")


set(SOURCE_FILES
    ${SRC_PATH}/ccidbgr.c
)
add_executable (ccidbgr ${SOURCE_FILES})


include_directories (${SYSROOTINC_PATH})
include_directories (${SYSROOT_INCLUDEDIR})

link_directories(${SYSROOT_LIBDIR})

install (TARGETS ccidbgr DESTINATION ${CMAKE_INSTALL_BINDIR})
