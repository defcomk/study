#======================================================================
#makefile for libais_config.so
#======================================================================
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MY_AIS_ROOT := $(LOCAL_PATH)

LOCAL_C_INCLUDES += \
	$(MY_AIS_ROOT)/API/inc \
	$(MY_AIS_ROOT)/CameraConfig/inc \
	$(MY_AIS_ROOT)/CameraDevice/inc \
	$(MY_AIS_ROOT)/CameraOSServices/CameraOSServices/inc \
	$(MY_AIS_ROOT)/CameraPlatform/inc \
	$(MY_AIS_ROOT)/CameraPlatform/linux \
	$(MY_AIS_ROOT)/Common/inc \
	$(MY_AIS_ROOT)/ImagingInputs/ImagingInputDriver/inc \
	$(TARGET_OUT_HEADERS)/mm-osal/include \

LOCAL_SRC_DIR := \
	$(MY_AIS_ROOT)/CameraConfig/src

LOCAL_SRC_FILES += $(shell find $(LOCAL_SRC_DIR) -maxdepth 1 -name '*.c' | sed s:^$(LOCAL_PATH)::g )

LOCAL_CFLAGS := $(ais_compile_cflags) -Wno-unused-parameter
LOCAL_CFLAGS += -DTAEGET_CONTI_DIDI_B01_BOARD


LOCAL_LDFLAGS :=

ifeq ($(call is-platform-sdk-version-at-least,28),true)
LOCAL_HEADER_LIBRARIES := libmmosal_proprietary_headers
else
LOCAL_HEADER_LIBRARIES := libmmosal_headers
endif

LOCAL_SHARED_LIBRARIES:= libais libais_log

LOCAL_MODULE := libais_config

LOCAL_MODULE_OWNER := qti
LOCAL_PROPRIETARY_MODULE := true

ifeq ($(AIS_32_BIT_FLAG), true)
LOCAL_32_BIT_ONLY := true
endif

ifeq ($(AIS_BUILD_STATIC),true)
include $(BUILD_STATIC_LIBRARY)
else
include $(BUILD_SHARED_LIBRARY)
endif

