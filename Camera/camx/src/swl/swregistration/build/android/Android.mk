ifeq ($(CAMX_PATH),)
LOCAL_PATH := $(abspath $(call my-dir)/../..)
CAMX_PATH := $(abspath $(LOCAL_PATH)/../../..)
else
LOCAL_PATH := $(CAMX_PATH)/src/swl/swregistration
endif

# For compatibility with the component repo directory structure,
# override LOCAL_PATH if CAMX_VENDOR_PATH exists
ifneq ($(CAMX_VENDOR_PATH),)
LOCAL_PATH := $(CAMX_PATH)/src/swl/swregistration
else
CAMX_VENDOR_PATH = $(CAMX_PATH)/vendor
endif

include $(CLEAR_VARS)

# Module supports function call tracing via ENABLE_FUNCTION_CALL_TRACE
# Required before including common.mk
SUPPORT_FUNCTION_CALL_TRACE := 1

# Get definitions common to the CAMX project here
include $(CAMX_VENDOR_PATH)/common.mk

LOCAL_SRC_FILES :=            \
    camxchinodeswregistration.cpp

LOCAL_INC_FILES :=            \
    camxchinodeswregistration.h

# Put here any libraries that should be linked by CAMX projects
LOCAL_C_LIBS := $(CAMX_C_LIBS)

# Paths to included headers
LOCAL_C_INCLUDES := $(CAMX_C_INCLUDES)            \
    $(CAMX_PATH)/src/swl/swregistration           \
    $(CAMX_PATH)/src/core                         \
    $(CAMX_PATH)/src/core/hal                     \
    $(CAMX_CDK_PATH)/node                         \
    $(CAMX_PATH)/ext                              \
    $(CAMX_OUT_HEADERS)/registrationalgo/v1.x

# Compiler flags
LOCAL_CFLAGS := $(CAMX_CFLAGS)
LOCAL_CPPFLAGS := $(CAMX_CPPFLAGS)

LOCAL_SHARED_LIBRARIES :=      \
    libcamera_metadata         \
    libcom.qti.chinodeutils

LOCAL_STATIC_LIBRARIES :=      \
    libcamxgenerated

# Binary name
LOCAL_MODULE := com.qti.node.swregistration
LOCAL_MODULE_RELATIVE_PATH := $(CAMX_LIB_OUTPUT_PATH)

include $(BUILD_SHARED_LIBRARY)

