# User Variables
set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( camxcsl
    ../../camxcdmdefs.cpp
    ../../camxcsl.cpp
    ../../camxcsljumptable.cpp
    ../../common/camxcslcommonutils.cpp
    ../../hw/camxcslhw.cpp
    ../../hw/camxcslhwicptypes.cpp
    ../../hw/camxcslhwifetypes.cpp
    ../../hw/camxcslhwinternal.cpp
    ../../hw/camxcslhwinternalactuator.cpp
    ../../hw/camxcslhwinternalcpas.cpp
    ../../hw/camxcslhwinternalcsiphy.cpp
    ../../hw/camxcslhwinternaleeprom.cpp
    ../../hw/camxcslhwinternalfd.cpp
    ../../hw/camxcslhwinternalois.cpp
    ../../hw/camxcslhwinternalicp.cpp
    ../../hw/camxcslhwinternalife.cpp
    ../../hw/camxcslhwinternaljpeg.cpp
    ../../hw/camxcslhwinternallrme.cpp
    ../../hw/camxcslhwinternalsensor.cpp
    ../../hw/camxcslhwinternalvfe.cpp
    ../../hw/camxcslhwinternalflash.cpp
    ../../hw/camxcslhwlrmetypes.cpp
    ../../hw/camxcslhwtypes.cpp
    ../../hw/camxcslhwvfetypes.cpp
    ../../hw/camxsyncmanager.cpp
    ../../ifh/camxcslifh.cpp
)

# Libraries to compile


# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (../../common)
include_directories (../../ifh)
include_directories (../../../utils)
#include_directories (${CAMX_SYSTEM_PATH}/stripinglib/external/utils/inc)
# Dependencies


# Definitions
if(CAMX_EXT_VBUILD EQUAL 0)
        set (CAMX_CPPFLAGS "${CAMX_CPPFLAGS} -Wno-address-of-packed-member")
endif()

add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})

# Custom additions (end)

#MESSAGE( STATUS "Configuration: " ${PresilConfiguration}_${PresilPlatform} )

#add_custom_target(CopyPresilDependencies
        #    COMMAND ${CMAKE_COMMAND} -E copy ${CAMX_PATH}/precompiled/csim/titan170/linux/${PresilConfiguration}_${PresilPlatform}/libcsimAPI.so libcsimAPI.so
        #    COMMAND ${CMAKE_COMMAND} -E copy ${CAMX_PATH}/precompiled/pcam/config/pcam_config.txt pcam_config.txt
        #    COMMAND ${CMAKE_COMMAND} -E copy ${CAMX_PATH}/precompiled/pcam/linux/${PresilConfiguration}_${PresilPlatform}/libtcp_bridge.so libtcp_bridge.so
        #    COMMAND ${CMAKE_COMMAND} -E copy ${CAMX_PATH}/precompiled/pcam/linux/${PresilConfiguration}_${PresilPlatform}/libsisci.so libsisci.so
        #    COMMAND ${CMAKE_COMMAND} -E copy ${CAMX_PATH}/precompiled/pcam/linux/${PresilConfiguration}_${PresilPlatform}/libsisci.so.3 libsisci.so.3
        #    COMMAND ${CMAKE_COMMAND} -E copy ${CAMX_PATH}/precompiled/pcam/linux/${PresilConfiguration}_${PresilPlatform}/libptypes.so libptypes.so
        #    COMMAND ${CMAKE_COMMAND} -E copy ${CAMX_PATH}/precompiled/pcam/linux/${PresilConfiguration}_${PresilPlatform}/libptypes.so.21 libptypes.so.21
        #    COMMAND ${CMAKE_COMMAND} -E copy ${CAMX_PATH}/system/firmware/CAMERA_ICP_AAAAAA.elf CAMERA_ICP_AAAAAA.elf
        #)

        #add_dependencies(camxcsl CopyPresilDependencies)

# Auto-Gen
#add_dependencies(camxcsl autogen)

install(FILES ${CAMX_PATH}/src/csl/camxcsl.h DESTINATION include/camx)

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

