ifeq ($(CAMX_PATH),)
LOCAL_PATH := $(abspath $(call my-dir)/../..)
CAMX_PATH := $(abspath $(LOCAL_PATH)/../../..)
else
LOCAL_PATH := $(CAMX_PATH)/src/core/ncs
endif

include $(CLEAR_VARS)

# Module supports function call tracing via ENABLE_FUNCTION_CALL_TRACE
# Required before including common.mk
SUPPORT_FUNCTION_CALL_TRACE := 1

# Get definitions common to the CAMX project here
include $(CAMX_PATH)/build/infrastructure/android/common.mk

LOCAL_SRC_FILES :=             \
    camxncsservice.cpp         \
    camxncssensor.cpp          \
    camxncssensordata.cpp      \
    camxncsintfqsee.cpp        \
    camxtofsensorintf.cpp      \
    camxncssscutils.cpp        \
    camxncssscconnection.cpp

LOCAL_INC_FILES :=           \
    camxncsintf.h            \
    camxncsservice.h         \
    camxncssensor.h          \
    camxncssensordata.h      \
    camxncsintfqsee.h        \
    camxtofsensorintf.h      \
    camxncssscutils.h        \
    camxncssscconnection.h   \
    sns_client_api_v01.h     \
    worker.h


# Put here any libraries that should be linked by CAMX projects
LOCAL_C_LIBS := $(CAMX_C_LIBS)

# Paths to included headers
LOCAL_C_INCLUDES := $(CAMX_C_INCLUDES)       \
    hardware/libhardware/include             \
    system/media/camera/include              \
    $(CAMX_CDK_PATH)/chi/                    \
    $(CAMX_CDK_PATH)/pdlib/                  \
    $(TARGET_OUT_HEADERS)/qmi-framework/inc  \
    $(TARGET_OUT_HEADERS)/sensors/nanopb/inc \
    $(TARGET_OUT_INTERMEDIATES)/../gen/SHARED_LIBRARIES/libsnsapi_intermediates/proto

# Compiler flags
LOCAL_CFLAGS := $(CAMX_CFLAGS)
LOCAL_CPPFLAGS := $(CAMX_CPPFLAGS)

LOCAL_CFLAGS += -Werror -Wall -Wno-unused-parameter -fexceptions

LOCAL_SHARED_LIBRARIES := libutils  \
    libprotobuf-cpp-full            \
    libsensorslog                   \
    libcutils                       \
    liblog                          \
    libqmi_common_so                \
    libqmi_cci                      \
    libqmi_encdec                   \
    libsnsapi                       \
    libsync

LOCAL_STATIC_LIBRARIES += libcamxcore  \
    libcamxcsl

LOCAL_WHOLE_STATIC_LIBRARIES := \
    libcamxosutils              \
    libcamxutils                \
    libcamxgenerated

# Binary name
LOCAL_MODULE := libcamxncs

include $(CAMX_BUILD_STATIC_LIBRARY)
-include $(CAMX_CHECK_WHINER)
