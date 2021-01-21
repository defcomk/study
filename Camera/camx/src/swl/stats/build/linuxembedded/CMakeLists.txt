# User Variables
set (SUPPORT_FUNCTION_CALL_TRACE 1)

# NO DATA FOR SECTION: Custom additions (begin)

# Common Include
include (${CAMX_PATH}/build/infrastructure/linuxembedded/common.cmake)

# Files and Build Type
add_library( camxstats
    ../../camxaecengine.cpp
    ../../camxautofocusnode.cpp
    ../../camxawbnode.cpp
    ../../camxafstatemachine.cpp
    ../../camxcaecstatsprocessor.cpp
    ../../camxcafalgorithmhandler.cpp
    ../../camxcafdalgorithmhandler.cpp
    ../../camxcafioutil.cpp
    ../../camxcafdstatsprocessor.cpp
    ../../camxcafdiohandler.cpp
    ../../camxcafstatsprocessor.cpp
    ../../camxcasdstatsprocessor.cpp
    ../../camxcawbioutil.cpp
    ../../camxcawbstatsprocessor.cpp
    ../../camxhistogramprocessnode.cpp
    ../../camxmultistatsoperator.cpp
    ../../camxstatscommon.cpp
    ../../camxstatsdebug.cpp
    ../../camxstatsparsenode.cpp
    ../../camxstatsprocessingnode.cpp
    ../../camxstatsprocessormanager.cpp
)

# Libraries to compile
target_link_libraries (camxstats camxncs)

# Include Paths
include_directories (${CAMX_C_INCLUDES})
include_directories (../..)
include_directories (${CAMX_CDK_PATH}/stats)
include_directories (${CAMX_CDK_PATH}/pdlib)
include_directories (${CAMX_CDK_PATH}/fd)
include_directories (../../../../hwl/iqinterpolation)
include_directories (../../../../hwl/iqsetting)
include_directories (../../../../hwl/titan17x)
include_directories (../../../../hwl/titan17x/regmap/titan170)
include_directories (../../../../core/ncs)
include_directories (${CAMX_CDK_PATH}/generated/g_chromatix)
include_directories (${CAMX_CDK_PATH}/generated/g_parser)

# Dependencies


# Definitions
add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})
# NO DATA FOR SECTION: Custom additions (end)

# Auto-Gen
add_dependencies(camxstats autogen)

if (CONFIG STREQUAL "Debug")
    add_definitions( -g )
endif()

