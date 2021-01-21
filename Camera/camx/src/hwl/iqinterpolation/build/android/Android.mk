ifeq ($(CAMX_PATH),)
LOCAL_PATH := $(abspath $(call my-dir)/../..)
CAMX_PATH := $(abspath $(LOCAL_PATH)/../../..)
else
LOCAL_PATH := $(CAMX_PATH)/src/hwl/iqinterpolation
endif

include $(CLEAR_VARS)

# Module supports function call tracing via ENABLE_FUNCTION_CALL_TRACE
# Required before including common.mk
SUPPORT_FUNCTION_CALL_TRACE := 1

# Get definitions common to the CAMX project here
include $(CAMX_PATH)/build/infrastructure/android/common.mk

LOCAL_SRC_FILES :=                         \
    anr10interpolation.cpp                 \
    asf30interpolation.cpp                 \
    bls12interpolation.cpp                 \
    bpsabf40interpolation.cpp              \
    bpsgic30interpolation.cpp              \
    hdr22interpolation.cpp                 \
    hdr23interpolation.cpp                 \
    bpslinearization34interpolation.cpp    \
    bpspdpc20interpolation.cpp             \
    cac22interpolation.cpp                 \
    cc13interpolation.cpp                  \
    cv12interpolation.cpp                  \
    demosaic36interpolation.cpp            \
    demosaic37interpolation.cpp            \
    gamma15interpolation.cpp               \
    gamma16interpolation.cpp               \
    gra10interpolation.cpp                 \
    gtm10interpolation.cpp                 \
    hnr10interpolation.cpp                 \
    ica10interpolation.cpp                 \
    ica20interpolation.cpp                 \
    ifeabf34interpolation.cpp              \
    ifebpcbcc50interpolation.cpp           \
    ifecc12interpolation.cpp               \
    ifehdr20interpolation.cpp              \
    ifelinearization33interpolation.cpp    \
    ifepdpc11interpolation.cpp             \
    ipe2dlut10interpolation.cpp            \
    ipecs20interpolation.cpp               \
    lsc34interpolation.cpp                 \
    ltm13interpolation.cpp                 \
    pedestal13interpolation.cpp            \
    sce11interpolation.cpp                 \
    tf10interpolation.cpp                  \
    tintless20interpolation.cpp            \
    tmc10interpolation.cpp                 \
    tmc11interpolation.cpp                 \
    upscale20interpolation.cpp

LOCAL_INC_FILES :=                         \
    anr10interpolation.h                   \
    asf30interpolation.h                   \
    bls12interpolation.h                   \
    bpsabf40interpolation.h                \
    bpsgic30interpolation.h                \
    hdr22interpolation.h                   \
    hdr23interpolation.h                   \
    bpslinearization34interpolation.h      \
    bpspdpc20interpolation.h               \
    cac22interpolation.h                   \
    cc13interpolation.h                    \
    cv12interpolation.h                    \
    demosaic36interpolation.h              \
    demosaic37interpolation.h              \
    gamma15interpolation.h                 \
    gamma16interpolation.h                 \
    gra10interpolation.h                   \
    gtm10interpolation.h                   \
    hnr10interpolation.h                   \
    ica10interpolation.h                   \
    ica20interpolation.h                   \
    ifeabf34interpolation.h                \
    ifebpcbcc50interpolation.h             \
    ifecc12interpolation.h                 \
    ifehdr20interpolation.h                \
    ifelinearization33interpolation.h      \
    ifepdpc11interpolation.h               \
    ipecs20interpolation.h                 \
    ipe2dlut10interpolation.h              \
    iqcommondefs.h                         \
    lsc34interpolation.h                   \
    ltm13interpolation.h                   \
    pedestal13interpolation.h              \
    sce11interpolation.h                   \
    tf10interpolation.h                    \
    tintless20interpolation.h              \
    tmc10interpolation.h                   \
    tmc11interpolation.h                   \
    upscale20interpolation.h

# Put here any libraries that should be linked by CAMX projects
LOCAL_C_LIBS := $(CAMX_C_LIBS)

# Paths to included headers
LOCAL_C_INCLUDES := $(CAMX_C_INCLUDES)     \
    $(CAMX_CDK_PATH)/generated/g_chromatix \
    $(CAMX_CDK_PATH)/generated/g_parser    \
    $(CAMX_PATH)/src/hwl/iqsetting

# Compiler flags
LOCAL_CFLAGS := $(CAMX_CFLAGS)
LOCAL_CPPFLAGS := $(CAMX_CPPFLAGS)

# Binary name
LOCAL_MODULE := libcamxiqinterpolation

include $(CAMX_BUILD_STATIC_LIBRARY)
-include $(CAMX_CHECK_WHINER)
