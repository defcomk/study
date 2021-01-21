# User Variables
set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( camxutils
    ../../camxatomic.cpp
    ../../camxdebug.cpp
    ../../camxdebugprint.cpp
    ../../camxhashmap.cpp
    ../../camximagedump.cpp
    ../../camximageformatutils.cpp
    ../../camxmemspy.cpp
    ../../camxstabilization.cpp
    ../../camxthreadcore.cpp
    ../../camxthreadjoblist.cpp
    ../../camxthreadjobregistry.cpp
    ../../camxthreadmanager.cpp
    ../../camxthreadqueue.cpp
    ../../camxtrace.cpp
    ../../camxtranslator.cpp
    ../../camxtypes.cpp
)

# Libraries to compile


# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (..)

# Dependencies


# Definitions
add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})
# NO DATA FOR SECTION: Custom additions (end)

# Auto-Gen
#add_dependencies(camxutils autogen)

install(FILES
        ${CAMX_PATH}/src/utils/camxatomic.h
        ${CAMX_PATH}/src/utils/camxdebug.h
        ${CAMX_PATH}/src/utils/camxdebugprint.h
        ${CAMX_PATH}/src/utils/camxdefs.h
        ${CAMX_PATH}/src/utils/camxformats.h
        ${CAMX_PATH}/src/utils/camxhashmap.h
        ${CAMX_PATH}/src/utils/camximageformatutils.h
        ${CAMX_PATH}/src/utils/camxincs.h
        ${CAMX_PATH}/src/utils/camxlist.h
        ${CAMX_PATH}/src/utils/camxmemspy.h
        ${CAMX_PATH}/src/utils/camxthreadcommon.h
        ${CAMX_PATH}/src/utils/camxthreadcore.h
        ${CAMX_PATH}/src/utils/camxthreadjoblist.h
        ${CAMX_PATH}/src/utils/camxthreadjobregistry.h
        ${CAMX_PATH}/src/utils/camxthreadmanager.h
        ${CAMX_PATH}/src/utils/camxthreadqueue.h
        ${CAMX_PATH}/src/utils/camxtrace.h
        ${CAMX_PATH}/src/utils/camxtypes.h
        ${CAMX_PATH}/src/utils/camxutils.h
        DESTINATION include/camx)

install(TARGETS camxutils ARCHIVE DESTINATION lib)

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

