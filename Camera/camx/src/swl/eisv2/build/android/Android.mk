ifeq ($(CAMX_PATH),)
LOCAL_PATH := $(abspath $(call my-dir)/../..)
CAMX_PATH := $(abspath $(LOCAL_PATH)/../../..)
else
LOCAL_PATH := $(CAMX_PATH)/src/swl/eisv2
endif

include $(CLEAR_VARS)

# Module supports function call tracing via ENABLE_FUNCTION_CALL_TRACE
# Required before including common.mk
SUPPORT_FUNCTION_CALL_TRACE := 1

# Get definitions common to the CAMX project here
include $(CAMX_PATH)/build/infrastructure/android/common.mk

LOCAL_INC_FILES :=              \
    camxchinodeeisv2.h

LOCAL_SRC_FILES :=              \
    camxchinodeeisv2.cpp

LOCAL_C_LIBS := $(CAMX_C_LIBS)

LOCAL_C_INCLUDES := $(CAMX_C_INCLUDES)     \
    system/media/camera/include            \
    $(CAMX_CDK_PATH)/node                  \
    $(CAMX_PATH)/ext                       \
    $(CAMX_PATH)/system/isalgo/common      \
    $(CAMX_PATH)/system/isalgo/eisv2algo   \
    $(CAMX_CDK_PATH)/generated/g_chromatix \
    $(CAMX_CDK_PATH)/generated/g_parser

# Compiler flags
LOCAL_CFLAGS := $(CAMX_CFLAGS)
LOCAL_CPPFLAGS := $(CAMX_CPPFLAGS)

CHROMATIX_VERSION := 0x0310

LOCAL_SHARED_LIBRARIES +=      \
    libcamera_metadata         \
    libcom.qti.chinodeutils    \
    libcutils                  \
    libsync

LOCAL_WHOLE_STATIC_LIBRARIES := \
    libcamxgenerated            \
    libcamxosutils              \
    libcamxutils

ifeq ($(IQSETTING),OEM1)
LOCAL_WHOLE_STATIC_LIBRARIES += \
    libcamxoem1chromatix
endif

LOCAL_MODULE := com.qti.node.eisv2

LOCAL_MODULE_RELATIVE_PATH := $(CAMX_LIB_OUTPUT_PATH)

include $(CAMX_BUILD_SHARED_LIBRARY)
