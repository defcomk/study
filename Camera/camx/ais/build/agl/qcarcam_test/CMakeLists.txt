project(qcarcam_test)
cmake_minimum_required(VERSION 2.6)

set(QCARCAM_SRC_PATH "${AIS_ROOT_PATH}/test/qcarcam_test/src")
set(UTIL_PATH "${AIS_ROOT_PATH}/test/test_util")

set (WAYLANDSCANNER_PATH "${SYSTEM_ROOT_HOST}/usr/bin")
set (WAYLAND_PROTOCOL_PATH "${UTIL_PATH}/src/agl/")
set (WAYLAND_PROTOCOL_XML_PATH "${UTIL_PATH}/src/agl/disp_protocol/")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wno-int-conversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror ")

add_definitions(
    -D__AGL__
    -D_GNU_SOURCE
    -DUSE_GBM_BACKEND
)

function (generate_header arg arg1)
    add_custom_command(OUTPUT ${WAYLAND_PROTOCOL_PATH}/${arg}.h
    WORKING_DIRECTORY ${WAYLAND_PROTOCOL_PATH}
    DEPENDS ${WAYLAND_PROTOCOL_XML_PATH}/${arg1}.xml ${WAYLANDSCANNER_PATH}/wayland-scanner
    COMMAND
        ${WAYLANDSCANNER_PATH}/wayland-scanner client-header < ${WAYLAND_PROTOCOL_XML_PATH}/${arg1}.xml > ${arg}.h
    COMMENT "Generating ${arg} header" VERBATIM
    )

endfunction ()

function (generate_src arg arg1)
    add_custom_command(OUTPUT ${WAYLAND_PROTOCOL_PATH}/${arg}.c
    WORKING_DIRECTORY ${WAYLAND_PROTOCOL_PATH}
    DEPENDS ${WAYLAND_PROTOCOL_XML_PATH}/${arg1}.xml ${WAYLANDSCANNER_PATH}/wayland-scanner
    COMMAND
        ${WAYLANDSCANNER_PATH}/wayland-scanner code < ${WAYLAND_PROTOCOL_XML_PATH}/${arg1}.xml > ${arg}.c
    COMMENT "Generating ${arg}" VERBATIM
    )

endfunction ()

generate_header(ivi-application-client-protocol ivi-application)
generate_header(gbm-buffer-backend-client-protocol gbm-buffer-backend)
generate_header(scaler-client-protocol scaler)
generate_header(xdg-shell-client-protocol xdg-shell)
generate_header(viewporter-client-protocol viewporter)

generate_src(ivi-application-client-protocol ivi-application)
generate_src(scaler-client-protocol scaler)
generate_src(gbm-buffer-backend-client-protocol gbm-buffer-backend)
generate_src(viewporter-client-protocol viewporter)

add_custom_target(wayland_protocol_files
    DEPENDS
        ${WAYLAND_PROTOCOL_PATH}/gbm-buffer-backend-client-protocol.h
        ${WAYLAND_PROTOCOL_PATH}/ivi-application-client-protocol.h
        ${WAYLAND_PROTOCOL_PATH}/scaler-client-protocol.h
        ${WAYLAND_PROTOCOL_PATH}/xdg-shell-client-protocol.h
        ${WAYLAND_PROTOCOL_PATH}/viewporter-client-protocol.h
        ${WAYLAND_PROTOCOL_PATH}/ivi-application-client-protocol.c
        ${WAYLAND_PROTOCOL_PATH}/scaler-client-protocol.c
        ${WAYLAND_PROTOCOL_PATH}/gbm-buffer-backend-client-protocol.c
        ${WAYLAND_PROTOCOL_PATH}/viewporter-client-protocol.c
)

set(SOURCE_FILES
    ${QCARCAM_SRC_PATH}/qcarcam_test.cpp
    ${UTIL_PATH}/src/test_util.cpp
    ${UTIL_PATH}/src/agl/test_util_agl.cpp
    ${WAYLAND_PROTOCOL_PATH}/ivi-application-client-protocol.c
    ${WAYLAND_PROTOCOL_PATH}/scaler-client-protocol.c
    ${WAYLAND_PROTOCOL_PATH}/gbm-buffer-backend-client-protocol.c
    ${WAYLAND_PROTOCOL_PATH}/viewporter-client-protocol.c
)
add_executable (qcarcam_test ${SOURCE_FILES})


include_directories (${MM-OSAL_INCPATH})
include_directories (${XML2_INCPATH})
include_directories (${AIS_ROOT_PATH}/API/inc)
include_directories (${AIS_ROOT_PATH}/CameraOSServices/CameraOSServices/inc)
include_directories (${AIS_ROOT_PATH}/Common/inc)
include_directories (${UTIL_PATH}/inc)
include_directories (${UTIL_PATH}/src/agl)

include_directories (${SYSROOTINC_PATH})
include_directories (${SYSROOT_INCLUDEDIR})

link_directories(${SYSROOT_LIBDIR})

target_link_libraries (qcarcam_test ais_client)
target_link_libraries (qcarcam_test ais_log)
target_link_libraries (qcarcam_test mmosal)
target_link_libraries (qcarcam_test pthread)
target_link_libraries (qcarcam_test cutils)
target_link_libraries (qcarcam_test C2D2)
target_link_libraries (qcarcam_test xml2)
target_link_libraries (qcarcam_test wayland-client)
target_link_libraries (qcarcam_test gbm)
target_link_libraries (qcarcam_test EGL)
target_link_libraries (qcarcam_test dl)
target_link_libraries (qcarcam_test glib-2.0)

add_dependencies (qcarcam_test wayland_protocol_files)

install (TARGETS qcarcam_test DESTINATION ${CMAKE_INSTALL_BINDIR})
