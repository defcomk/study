# User Variables
set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( camxcore
    ../../camxactuatordata.cpp
    ../../camxcmdbuffer.cpp
    ../../camxcmdbuffermanager.cpp
    ../../camxdeferredrequestqueue.cpp
    ../../camxeepromdata.cpp
    ../../camxoisdata.cpp
    ../../camxflashdata.cpp
    ../../camxhwcontext.cpp
    ../../camxhwenvironment.cpp
    ../../camxhwfactory.cpp
    ../../camximagebuffer.cpp
    ../../camximagebuffermanager.cpp
    ../../camximagesensordata.cpp
    ../../camximagesensormoduledata.cpp
    ../../camximagesensormoduledatamanager.cpp
    ../../camxmempoolgroup.cpp
    ../../camxmempoolmgr.cpp
    ../../camxmetabuffer.cpp
    ../../camxmetadatapool.cpp
    ../../camxnode.cpp
    ../../camxoverridesettingsfile.cpp
    ../../camxpacket.cpp
    ../../camxpacketbuilder.cpp
    ../../camxpacketresource.cpp
    ../../camxpdafdata.cpp
    ../../camxpipeline.cpp
    ../../camxsession.cpp
    ../../camxsettingsmanager.cpp
    ../../camxstatsparser.cpp
    ../../camxtest.cpp
    ../../camxtuningdatamanager.cpp
    ../../camxvendortags.cpp
    ../../oem/camxcustomization.cpp
    ${CAMX_PATH}/src/core/g_camxproperties.cpp
    ${CAMX_PATH}/src/core/g_camxsettings.cpp
)
SET_SOURCE_FILES_PROPERTIES(${CAMX_PATH}/src/core/g_camxproperties.cpp PROPERTIES GENERATED 1)
SET_SOURCE_FILES_PROPERTIES(${CAMX_PATH}/src/core/g_camxproperties.cpp PROPERTIES GENERATED 1)

# Libraries to compile
target_link_libraries (camxcore camxutils)
target_link_libraries (camxcore camxsensor)
target_link_libraries (camxcore camxhalutils)
target_link_libraries (camxcore camxchi)
target_link_libraries (camxcore camxhwltitan17x)
target_link_libraries (camxcore camxcsl)
target_link_libraries (camxcore camxosutils)

# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (../../oem)
include_directories (${CAMX_CDK_PATH}/chi/)
include_directories (${CAMX_CDK_PATH}/pdlib)
include_directories (${CAMX_CDK_PATH}/fd/)
include_directories (${CAMX_CDK_PATH}/generated/g_chromatix)
include_directories (${CAMX_CDK_PATH}/generated/g_parser)
include_directories (../../../hwl/titan17x/)
include_directories (../../../hwl/titan17x/regmap/titan170)
include_directories (../../ncs)



# Definitions
add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})
# NO DATA FOR SECTION: Custom additions (end)

# Auto-Gen
#add_dependencies(camxcore autogen)

# Dependencies
#add_dependencies(camxcore camxgenerated)

#install
install(FILES ${CAMX_PATH}/src/core/camxtuningdatamanager.h DESTINATION include/camx)

#library linking
#target_link_libraries (camxcore camxgenerated)

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

