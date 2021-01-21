# User Variables
set (CHROMATIX_VERSION 0x0310)

set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( com.qti.node.eisv2 SHARED
    ../../camxchinodeeisv2.cpp
)

# Libraries to compile
if (CAMERA_METADAT_ENABLE)
        target_link_libraries (com.qti.node.eisv2 camera_metadata)
endif()
#target_link_libraries (com.qti.node.eisv2 com.qti.chinodeutils)
target_link_libraries (com.qti.node.eisv2 sync)
target_link_libraries (com.qti.node.eisv2 camxgenerated)
target_link_libraries (com.qti.node.eisv2 camxosutils)
target_link_libraries (com.qti.node.eisv2 camxutils)
target_link_libraries (com.qti.node.eisv2 dl)
target_link_libraries (com.qti.node.eisv2 libcom.qti.chinodeutils)

if(IQSETTING EQUAL "OEM1")
        target_link_libraries (com.qti.node.eisv2 camxoem1chromatix)
 endif()


# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (${CAMX_CDK_PATH}/node)
include_directories (${CAMX_CDK_PATH}/ncs)
include_directories (${CAMX_SYSTEM_PATH}/isalgo/common)
include_directories (${CAMX_SYSTEM_PATH}/isalgo/eisv2algo)
include_directories (${CAMX_CDK_PATH}/generated/g_chromatix)
include_directories (${CAMX_CDK_PATH}/generated/g_parser)
include_directories (${CAMX_SYSTEM_PATH}/nclib/Logic)
# Dependencies


# Definitions
add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})
# NO DATA FOR SECTION: Custom additions (end)

# Auto-Gen
add_dependencies(com.qti.node.eisv2 camxgenerated)

#install
install(TARGETS com.qti.node.eisv2 LIBRARY DESTINATION lib/${CAMX_LIB_OUTPUT_PATH})

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

