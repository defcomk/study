# User Variables
set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( camxhwlife
    ../../camxifenode.cpp
)

# Libraries to compile
target_link_libraries (camxhwlife camxhwliqmodule)
target_link_libraries (camxhwlife camxcore)
target_link_libraries (camxhwlife ifestriping)


# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (../../../../core)
include_directories (${CAMX_CDK_PATH}/generated/g_chromatix)
include_directories (${CAMX_CDK_PATH}/generated/g_parser)
include_directories (../../../dspinterfaces)
include_directories (../..)
include_directories (../../../iqsetting)
include_directories (../../../ispiqmodule)
include_directories (../../../titan17x)
include_directories (../../../titan17x/regmap/titan170)
#include_directories (${CAMX_OUT_HEADERS})
include_directories (../../../iqinterpolation)
include_directories (${CAMX_SYSTEM_PATH}/stripinglib/external/utils/inc)
include_directories (${CAMX_SYSTEM_PATH}/stripinglib/common/inc)

if(IQSETTING EQUAL "OEM1")
        include_directories (${CAMX_OEM1IQ_PATH}/iqsetting)
else()
        include_directories (../../../iqinterpolation)
endif()

# Dependencies


# Definitions
if(CAMX_EXT_VBUILD EQUAL 0)
        set (CAMX_CPPFLAGS "${CAMX_CPPFLAGS} -Wno-address-of-packed-member")
endif()

add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})
# NO DATA FOR SECTION: Custom additions (end)

# Auto-Gen
#add_dependencies(camxhwlife autogen)

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

