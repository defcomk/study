# User Variables
set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( camxhwltitan17x
    ../../camxtitan17xcontext.cpp
    ../../camxtitan17xfactory.cpp
    ../../camxtitan17xhwl.cpp
    ../../camxtitan17xsettingsmanager.cpp
    ../../camxtitan17xstatsparser.cpp
    ${CAMX_PATH}/src/hwl/titan17x/g_camxtitan17xsettings.cpp
)
SET_SOURCE_FILES_PROPERTIES(${CAMX_PATH}/src/hwl/titan17x/g_camxtitan17xsettings.cpp PROPERTIES GENERATED 1)

# Libraries to compile
target_link_libraries (camxhwltitan17x camxswljpeg)
target_link_libraries (camxhwltitan17x camxswlransac)
target_link_libraries (camxhwltitan17x camxhwlbps)
target_link_libraries (camxhwltitan17x camxhwlife)
target_link_libraries (camxhwltitan17x camxhwljpeg)
target_link_libraries (camxhwltitan17x camxstats)
target_link_libraries (camxhwltitan17x camxhwllrme)
#target_link_libraries (camxhwltitan17x camxhwlfd)
target_link_libraries (camxhwltitan17x camxofflinestats)
#target_link_libraries (camxhwltitan17x camxswlfdmanager)
target_link_libraries (camxhwltitan17x camxhwlipe)
target_link_libraries (camxhwltitan17x camxsensor)


# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (${CAMX_CDK_PATH}/pdlib)
include_directories (${CAMX_CDK_PATH}/stats)
include_directories (${CAMX_CDK_PATH}/chromatix/isp)
include_directories (${CAMX_CDK_PATH}/generated/g_facedetection)
include_directories (../../../bps)
include_directories (../../../fd)
include_directories (../../../ife)
include_directories (../../../ipe)
include_directories (../../../iqsetting)
include_directories (../../../ispiqmodule)
include_directories (../../../jpeg)
include_directories (../../../lrme)
include_directories (../../regmap)
include_directories (../../regmap/titan170)
include_directories (../../../../swl/fd/fdmanager)
include_directories (../../../../swl/jpeg)
include_directories (../../../../swl/offlinestats)
include_directories (../../../../swl/ransac)
include_directories (../../../../swl/sensor)
include_directories (../../../../core/ncs)
include_directories (../../../../swl/stats)
include_directories (../../../../core/ncs)
include_directories (${CAMX_OUT_HEADERS})
include_directories (${CAMX_OUT_HEADERS}/fd/fdengine)
include_directories (../../../iqinterpolation)
include_directories (${CAMX_SYSTEM_PATH}/localhistogramalgo/inc)
include_directories (${CAMX_SYSTEM_PATH}/registermap/titan17x/titan170)
include_directories (${CAMX_SYSTEM_PATH}/stripinglib/external/utils/inc)
include_directories (${CAMX_SYSTEM_PATH}/stripinglib/common/inc)
include_directories (${CAMX_SYSTEM_PATH}/fd/fdengine/inc)
include_directories (${CAMX_SYSTEM_PATH}/swprocessalgo/inc)

if(IQSETTING EQUAL "OEM1")
        include_directories (${CAMX_OEM1IQ_PATH}/iqsetting)
else()
        include_directories (../../../iqinterpolation)
endif()
# Dependencies


# Definitions
add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})
# NO DATA FOR SECTION: Custom additions (end)

# Auto-Gen
#add_dependencies(camxhwltitan17x autogen)

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

