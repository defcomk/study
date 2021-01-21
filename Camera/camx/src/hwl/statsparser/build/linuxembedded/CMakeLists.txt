# User Variables
set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( camxstatsparser
    ../../camxcstatsparser.cpp
    ../../camxstatsparserentry.cpp
    ../../titan17x/camxtitan17xsparser.cpp
)

# Libraries to compile


# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (${CAMX_CDK_PATH}/stats)
include_directories (../../../../core)
include_directories (../..)
include_directories (../../titan17x)
include_directories (../../../titan17x)

# Dependencies


# Definitions
add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})
# NO DATA FOR SECTION: Custom additions (end)

# Auto-Gen
#add_dependencies(camxstatsparser autogen)

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

