# User Variables
set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( camxiqinterpolation
    ../../anr10interpolation.cpp
    ../../asf30interpolation.cpp
    ../../bls12interpolation.cpp
    ../../bpsabf40interpolation.cpp
    ../../bpsgic30interpolation.cpp
    ../../bpslinearization34interpolation.cpp
    ../../bpspdpc20interpolation.cpp
    ../../cac22interpolation.cpp
    ../../cc13interpolation.cpp
    ../../cv12interpolation.cpp
    ../../demosaic36interpolation.cpp
    ../../demosaic37interpolation.cpp
    ../../gamma15interpolation.cpp
    ../../gamma16interpolation.cpp
    ../../gra10interpolation.cpp
    ../../gtm10interpolation.cpp
    ../../hdr22interpolation.cpp
    ../../hdr23interpolation.cpp
    ../../hnr10interpolation.cpp
    ../../ica10interpolation.cpp
    ../../ica20interpolation.cpp
    ../../ifeabf34interpolation.cpp
    ../../ifebpcbcc50interpolation.cpp
    ../../ifecc12interpolation.cpp
    ../../ifehdr20interpolation.cpp
    ../../ifelinearization33interpolation.cpp
    ../../ifepdpc11interpolation.cpp
    ../../ipe2dlut10interpolation.cpp
    ../../ipecs20interpolation.cpp
    ../../lsc34interpolation.cpp
    ../../ltm13interpolation.cpp
    ../../pedestal13interpolation.cpp
    ../../sce11interpolation.cpp
    ../../tf10interpolation.cpp
    ../../tintless20interpolation.cpp
    ../../tmc10interpolation.cpp
    ../../tmc11interpolation.cpp
    ../../upscale20interpolation.cpp
)

# Libraries to compile


# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (${CAMX_CDK_PATH}/generated/g_chromatix)
include_directories (${CAMX_CDK_PATH}/generated/g_parser)
include_directories (../../../iqsetting)
include_directories (../../../../../../../camx-lib/system/nclib)

# Dependencies


# Definitions
add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})
# NO DATA FOR SECTION: Custom additions (end)

# Auto-Gen
#add_dependencies(camxiqinterpolation autogen)

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

