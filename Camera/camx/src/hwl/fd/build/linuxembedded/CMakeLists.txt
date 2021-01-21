# User Variables
set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( camxhwlfd
    ../../camxfdhwnode.cpp
    ../../camxfdhwutils.cpp
)

# Libraries to compile
target_link_libraries (camxhwlfd camxcore)

# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (../..)
include_directories (../../../titan17x)
include_directories (../../../titan17x/regmap/titan170)
include_directories (../../../../swl/fd/fdmanager)
include_directories (${CAMX_CDK_PATH}/generated/g_facedetection)

# Dependencies


# Definitions
add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})
# NO DATA FOR SECTION: Custom additions (end)

# Auto-Gen
#add_dependencies(camxhwlfd autogen)

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

