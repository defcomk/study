# User Variables
set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( camxdspstreamer
    ../../camxifedspinterface.cpp
    ../../dsp_streamer_callback_skel.c
    ../../dsp_streamer_stub.c
    ../../remote.c
)

# Libraries to compile


# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (../..)

# Dependencies


# Definitions
set (CAMX_CFLAGS "${CAMX_CFLAGS} \
                 -Wno-uninitialized \
                 -Wno-unused-parameter \
                 -Wno-unused-variable"
    )

add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})
# NO DATA FOR SECTION: Custom additions (end)

# Auto-Gen
#add_dependencies(camxdspstreamer autogen)

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

