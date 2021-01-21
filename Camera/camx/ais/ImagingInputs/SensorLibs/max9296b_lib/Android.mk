#======================================================================
#makefile for libais_max9296b.so
#======================================================================
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MY_AIS_ROOT := $(LOCAL_PATH)/../../..

LOCAL_C_INCLUDES += \
	$(MY_AIS_ROOT)/API/inc \
	$(MY_AIS_ROOT)/CameraDevice/inc \
	$(MY_AIS_ROOT)/CameraEventLog/inc \
	$(MY_AIS_ROOT)/CameraOSServices/CameraOSServices/inc \
	$(MY_AIS_ROOT)/CameraOSServices/CameraOSServicesMMOSAL/inc \
	$(MY_AIS_ROOT)/Common/inc \
	$(MY_AIS_ROOT)/ImagingInputs/ImagingInputDriver/inc \
	$(MY_AIS_ROOT)/ImagingInputs/ImagingInputDriver/inc/$(CHROMATIX_VERSION)

LOCAL_SRC_DIR :=\
	$(LOCAL_PATH)/src

LOCAL_SRC_FILES += $(shell find $(LOCAL_SRC_DIR) -name '*.c' | sed s:^$(LOCAL_PATH)::g )

LOCAL_CFLAGS := $(ais_compile_cflags)

LOCAL_LDFLAGS :=

ifeq ($(call is-platform-sdk-version-at-least,28),true)
LOCAL_HEADER_LIBRARIES := libmmosal_proprietary_headers
endif

LOCAL_SHARED_LIBRARIES:= libais libais_log

LOCAL_MODULE := libais_max9296b

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
