# User Variables
set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( camxchi
    ../../camxchi.cpp
    ../../camxchicomponent.cpp
    ../../camxchicontext.cpp
    ../../camxchinodewrapper.cpp
    ../../camxchisession.cpp
)

# Libraries to compile
target_link_libraries (camxchi cutils)

# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (${CAMX_CDK_PATH}/chi)
include_directories (${CAMX_CDK_PATH}/node)
include_directories (${CAMX_CDK_PATH}/stats)
include_directories (${CAMX_CDK_PATH}/pdlib)
include_directories (${CAMX_CDK_PATH}/pdlibwrapper)
include_directories (${CAMX_CDK_PATH}/ncs)
include_directories (../../../../hwl/titan17x)
include_directories (../../../../hwl/titan17x/regmap/titan170)
include_directories (../../../hal)
include_directories (../../../ncs)
include_directories (../../..)
#include_directories (camera)

# Dependencies


# Definitions
add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})
# NO DATA FOR SECTION: Custom additions (end)

# Auto-Gen
#add_dependencies(camxchi autogen)

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

