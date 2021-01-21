# User Variables
set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( ${CAMX_LIB} SHARED
    ../../camxentry.cpp
    ../../camxhal3.cpp
    ../../camxhal3entry.cpp
    ../../camxhal3metadatatags.cpp
    ../../camxhal3module.cpp
    ../../camxhal3stream.cpp
    ../../camxhal3types.cpp
    ../../camxhaldevice.cpp
    ../../camxpresilmem.cpp
    ../../camxthermalmanager.cpp
)


# Libraries to compile
target_link_libraries (${CAMX_LIB} camxchi)
target_link_libraries (${CAMX_LIB} camxofflinestats)
target_link_libraries (${CAMX_LIB} nc)
target_link_libraries (${CAMX_LIB} camxdspstreamer)
target_link_libraries (${CAMX_LIB} camxhwlbps)
target_link_libraries (${CAMX_LIB} camxhalutils)
#FD disabled
#target_link_libraries (${CAMX_LIB} camxhwlfd)
target_link_libraries (${CAMX_LIB} camxhwlife)
target_link_libraries (${CAMX_LIB} camxhwlipe)
target_link_libraries (${CAMX_LIB} camxhwliqmodule)
#FD disabled
#target_link_libraries (${CAMX_LIB} camxswlfdmanager)
target_link_libraries (${CAMX_LIB} camxswljpeg)
target_link_libraries (${CAMX_LIB} camxhwljpeg)
target_link_libraries (${CAMX_LIB} camxhwllrme)
target_link_libraries (${CAMX_LIB} camxswlransac)
target_link_libraries (${CAMX_LIB} camxiqsetting)
target_link_libraries (${CAMX_LIB} camxosutils)
target_link_libraries (${CAMX_LIB} camxstats)
target_link_libraries (${CAMX_LIB} camxsensor)
target_link_libraries (${CAMX_LIB} camxutils)
target_link_libraries (${CAMX_LIB} camxiqinterpolation)
target_link_libraries (${CAMX_LIB} camxcore)
target_link_libraries (${CAMX_LIB} camxcsl)
target_link_libraries (${CAMX_LIB} camxhwltitan17x)
target_link_libraries (${CAMX_LIB} camxncs)
target_link_libraries (${CAMX_LIB} striping)
target_link_libraries (${CAMX_LIB} ifestriping)
target_link_libraries (${CAMX_LIB} camxgenerated)
target_link_libraries (${CAMX_LIB} dl)
target_link_libraries (${CAMX_LIB} camxfdengine)
target_link_libraries (${CAMX_LIB} cutils)


# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (${CAMX_CDK_PATH}/chi)
include_directories (../../../ncs)

# Dependencies


# Definitions
add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})

#set(ANDROID_API 27)

if(${ANDROID_API} EQUAL 28)
add_definitions (-DCAMX_ANDROID_API=28)
else()
add_definitions (-DCAMX_ANDROID_API=27)
endif()

# NO DATA FOR SECTION: Custom additions (end)

# Auto-Gen
#add_dependencies(camxhal autogen)

install(FILES
        ${CAMX_PATH}/src/core/hal/camxhal3types.h
        ${CAMX_PATH}/src/core/hal/camxcommontypes.h
               DESTINATION include/camx)

install(TARGETS ${CAMX_LIB} LIBRARY DESTINATION lib/hw)

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

