# User Variables
set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( camxhwljpeg
    ../../camxjpegencconfig.cpp
    ../../camxjpegencnode.cpp
)

# Libraries to compile
target_link_libraries (camxhwljpeg camxcore)

# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (../../../../core)
include_directories (../..)
include_directories (../../../titan17x)
include_directories (../../../titan17x/regmap/titan170)
include_directories (../../../../swl/jpeg)

# Dependencies


# Definitions
if(CAMX_EXT_VBUILD EQUAL 0)
        set (CAMX_CPPFLAGS "${CAMX_CPPFLAGS} -Wno-address-of-packed-member")
endif()
add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})
# NO DATA FOR SECTION: Custom additions (end)

# Auto-Gen
#add_dependencies(camxhwljpeg autogen)

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

