# User Variables
set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( com.qti.camx.chiiqutils
        ../../camxchiiqutilshandler.cpp
)

# Libraries to compile


# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (${CAMX_CDK_PATH}/utils/)
include_directories (${CAMX_CDK_PATH}/pdlib)
include_directories (${CAMX_CDK_PATH}/fd/)
include_directories (${CAMX_CDK_PATH}/generated/g_chromatix)
include_directories (${CAMX_CDK_PATH}/generated/g_parser)
include_directories (../../ncs)
include_directories (${CAMX_SYSTEM_PATH}/nclib/Logic)
include_directories (${CAMX_SYSTEM_PATH}/nclib)


# Definitions
add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})
# NO DATA FOR SECTION: Custom additions (end)

# Auto-Gen
#add_dependencies(camxcore autogen)

# Dependencies
#add_dependencies(camxcore camxgenerated)

#install
install(FILES ${CAMX_PATH}/src/chiiqutils/camxchiiqutilshandler.h DESTINATION include/camx)

#library linking
target_link_libraries (com.qti.camx.chiiqutils com.qti.chinodeutils)
target_link_libraries (com.qti.camx.chiiqutils sync)
target_link_libraries (com.qti.camx.chiiqutils camxgenerated)
target_link_libraries (com.qti.camx.chiiqutils camxosutils)
target_link_libraries (com.qti.camx.chiiqutils camxutils)
target_link_libraries (com.qti.camx.chiiqutils nc)

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

