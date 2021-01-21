# User Variables
set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( camxncs
    ../../camxncsservice.cpp
    ../../camxncssensor.cpp
    ../../camxncssensordata.cpp
    ../../camxncsintfqsee.cpp
    ../../camxtofsensorintf.cpp
    ../../camxncssscconnection.cpp
    ../../camxncssscutils.cpp
)

# Libraries to compile
target_link_libraries (camxncs camxcore)
target_link_libraries (camxncs camxcsl)
target_link_libraries (camxncs camxosutils)
target_link_libraries (camxncs camxutils)
target_link_libraries (camxncs camxgenerated)
target_link_libraries (camxncs utils cutils log sync)
target_link_libraries (camxncs protobuf sensorslog ssc)

# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (${CAMX_EXT_INCLUDE}/) #to include hardware/hardware.h
include_directories (${CAMX_EXT_INCLUDE}/system)
include_directories (${CAMX_CDK_PATH}/chi/)
include_directories (${CAMX_CDK_PATH}/pdlib/)

# Dependencies


# Definitions
add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})
# NO DATA FOR SECTION: Custom additions (end)

# Auto-Gen
#add_dependencies(camxncs autogen)

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

