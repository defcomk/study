# User Variables
set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( camxhwllrme
    ../../camxlrmenode.cpp
)

# Libraries to compile
target_link_libraries (camxhwllrme camxcore)


# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (${CAMX_CDK_PATH})
include_directories (../../../../core)
include_directories (${CAMX_CDK_PATH}/generated/g_parser)
include_directories (../..)
include_directories (../../../titan17x)
include_directories (../../../titan17x/regmap/titan170)
include_directories (${TARGET_OUT_HEADERS}/camx)
include_directories (${CAMX_SYSTEM_PATH}/stripinglib/external/utils/inc)
include_directories (${CAMX_SYSTEM_PATH}/stripinglib/common/inc)
# Dependencies


# Definitions
add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})
# NO DATA FOR SECTION: Custom additions (end)

# Auto-Gen
#add_dependencies(camxhwllrme autogen)

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

