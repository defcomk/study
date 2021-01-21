# Top-level CAMX driver's CMakeLists.txt

cmake_minimum_required(VERSION 2.6)

project (camx)

set (CAMX_PATH ${CMAKE_CURRENT_LIST_DIR}/../../..)
set (CAMX_OUT_PATH ${PROJECT_BINARY_DIR})
set (CAMX_CDK_PATH ${CAMX_PATH}/../chi-cdk/cdk)
set (CAMX_VENDOR_PATH ${CAMX_PATH}/../chi-cdk/vendor)
set (CAMX_TOOLS_PATH ${CAMX_PATH}/build/infrastructure/tools)
set (CAMX_SYSTEM_STATS_PATH ${CAMX_PATH}/../camx-lib-stats/system)
set (CAMX_SYSTEM_PATH ${CAMX_PATH}/../camx-lib/system)
set (CAMX_SYSTEM_3A_PATH ${CAMX_PATH}/../camx-lib-3a/system)
set (ANDROID_API 27)


if(${ANDROID_API} EQUAL 28)
add_definitions (-DCAMX_ANDROID_API=28)
else()
add_definitions (-DCAMX_ANDROID_API=27)
endif()


set (CAMX_LIB_OUTPUT_PATH camera/components)
# settings and SCOPE generation
# If adding outputs, add to custom_target autogen as well
add_custom_command (
        OUTPUT
                ${CAMX_PATH}/src/core/g_camxsettings.cpp
                ${CAMX_PATH}/src/core/g_camxsettings.h
                ${CAMX_PATH}/src/hwl/titan17x/g_camxtitan17xsettings.cpp
                ${CAMX_PATH}/src/hwl/titan17x/g_camxtitan17xsettings.h
                ${CAMX_PATH}/src/core/g_camxversion.h
                ${CAMX_PATH}/src/core/g_camxproperties.h
        DEPENDS
                ${CAMX_PATH}/src/core/camxsettings.xml
                ${CAMX_PATH}/src/hwl/titan17x/camxtitan17xsettings.xml
                ${CAMX_PATH}/src/core/camxproperties.xml
        COMMAND perl ${CAMX_TOOLS_PATH}/settingsgenerator/settingsgenerator.pl
                        ${CAMX_PATH}/src/core/camxsettings.xml
                        ${CAMX_PATH}/src/hwl/titan17x/camxtitan17xsettings.xml
                        ${CAMX_PATH}/build/built/settings/camxoverridesettings.txt
        COMMAND perl ${CAMX_TOOLS_PATH}/version.pl ${CAMX_PATH}/src/core/g_camxversion.h
        COMMAND perl ${CAMX_TOOLS_PATH}/props.pl ${CAMX_PATH}/src/core/camxproperties.xml ${CAMX_PATH}/src/core/g_camxproperties
        COMMENT "Generate Settings, SCOPE, and version"
        )


add_custom_target (autogen
        DEPENDS
                ${CAMX_PATH}/src/core/g_camxsettings.cpp
                ${CAMX_PATH}/src/core/g_camxsettings.h
                ${CAMX_PATH}/src/hwl/titan17x/g_camxtitan17xsettings.cpp
                ${CAMX_PATH}/src/hwl/titan17x/g_camxtitan17xsettings.h
                ${CAMX_PATH}/src/core/g_camxversion.h
                ${CAMX_PATH}/src/core/g_camxproperties.h
        )

add_subdirectory (${CAMX_PATH}/src/core/build/linuxembedded ${CAMX_OUT_PATH}/core)
add_subdirectory (${CAMX_PATH}/src/core/chi/build/linuxembedded ${CAMX_OUT_PATH}/chi)
add_subdirectory (${CAMX_PATH}/src/core/hal/build/linuxembedded ${CAMX_OUT_PATH}/hal)
add_subdirectory (${CAMX_PATH}/src/core/halutils/build/linuxembedded ${CAMX_OUT_PATH}/halutils)
add_subdirectory (${CAMX_PATH}/src/core/ncs/build/linuxembedded ${CAMX_OUT_PATH}/ncs)
add_subdirectory (${CAMX_PATH}/src/csl/build/linuxembedded ${CAMX_OUT_PATH}/csl)
add_subdirectory (${CAMX_PATH}/src/hwl/bps/build/linuxembedded ${CAMX_OUT_PATH}/bps)
add_subdirectory (${CAMX_PATH}/src/hwl/fd/build/linuxembedded ${CAMX_OUT_PATH}/fd)
add_subdirectory (${CAMX_PATH}/src/hwl/dspinterfaces/build/linuxembedded ${CAMX_OUT_PATH}/dspinterfaces)
add_subdirectory (${CAMX_PATH}/src/hwl/ife/build/linuxembedded ${CAMX_OUT_PATH}/ife)
add_subdirectory (${CAMX_PATH}/src/hwl/ipe/build/linuxembedded ${CAMX_OUT_PATH}/ipe)
add_subdirectory (${CAMX_PATH}/src/hwl/iqinterpolation/build/linuxembedded ${CAMX_OUT_PATH}/iqinterpolation)
add_subdirectory (${CAMX_PATH}/src/hwl/iqsetting/build/linuxembedded ${CAMX_OUT_PATH}/iqsetting)
add_subdirectory (${CAMX_PATH}/src/hwl/ispiqmodule/build/linuxembedded ${CAMX_OUT_PATH}/ispiqmodule)
add_subdirectory (${CAMX_PATH}/src/hwl/jpeg/build/linuxembedded ${CAMX_OUT_PATH}/jpeghw)
add_subdirectory (${CAMX_PATH}/src/hwl/lrme/build/linuxembedded ${CAMX_OUT_PATH}/lrme)
add_subdirectory (${CAMX_PATH}/src/hwl/statsparser/build/linuxembedded ${CAMX_OUT_PATH}/statsparser)
add_subdirectory (${CAMX_PATH}/src/hwl/titan17x/build/linuxembedded ${CAMX_OUT_PATH}/titan17x_hwl)
add_subdirectory (${CAMX_PATH}/src/osutils/build/linuxembedded ${CAMX_OUT_PATH}/src/osutils)
add_subdirectory (${CAMX_PATH}/src/swl/fd/fdmanager/build/linuxembedded ${CAMX_OUT_PATH}/fdmanager)
add_subdirectory (${CAMX_PATH}/src/swl/jpeg/build/linuxembedded ${CAMX_OUT_PATH}/jpeg)
add_subdirectory (${CAMX_PATH}/src/swl/offlinestats/build/linuxembedded ${CAMX_OUT_PATH}/offlinestats)
add_subdirectory (${CAMX_PATH}/src/swl/ransac/build/linuxembedded ${CAMX_OUT_PATH}/ransac)
add_subdirectory (${CAMX_PATH}/src/swl/sensor/build/linuxembedded ${CAMX_OUT_PATH}/sensor)
add_subdirectory (${CAMX_PATH}/src/swl/stats/build/linuxembedded ${CAMX_OUT_PATH}/stats)
add_subdirectory (${CAMX_PATH}/src/utils/build/linuxembedded ${CAMX_OUT_PATH}/utils)
