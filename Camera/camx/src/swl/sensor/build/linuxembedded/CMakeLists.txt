# User Variables
set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( camxsensor
    ../../camxactuator.cpp
    ../../camxois.cpp
    ../../camxcsiphysubmodule.cpp
    ../../camxexternalsensor.cpp
    ../../camxflash.cpp
    ../../camximagesensorutils.cpp
    ../../camxpdlibraryhandler.cpp
    ../../camxsensornode.cpp
    ../../camxsensorpickmode.cpp
    ../../camxsensorsubmodulebase.cpp
    ../../camxtorchnode.cpp
)

# Libraries to compile


# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (${CAMX_CDK_PATH}/pdlib)
include_directories (${CAMX_CDK_PATH}/pdlibwrapper)
include_directories (${CAMX_CDK_PATH}/chi)
include_directories (${CAMX_CDK_PATH}/fd)

# Dependencies


# Definitions
add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})
# NO DATA FOR SECTION: Custom additions (end)

# Auto-Gen
add_dependencies(camxsensor autogen)

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

