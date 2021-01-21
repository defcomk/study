#======================================================================
#makefile for libais_ov490.so
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

LOCAL_SRC_DIR :=\
	$(LOCAL_PATH)/src

LOCAL_SRC_FILES += $(shell find $(LOCAL_SRC_DIR) -name '*.c' | sed s:^$(LOCAL_PATH)::g )

LOCAL_CFLAGS := $(ais_compile_cflags) -DOV490_SCCB_SLAVE_BOOTUP_ENABLE -DOV490_SCCB_SLAVE_BOOTUP_FREERUN -DOV490_HOTPLUG_FEATURE_ENABLE

LOCAL_LDFLAGS :=

ifeq ($(call is-platform-sdk-version-at-least,28),true)
LOCAL_HEADER_LIBRARIES := libmmosal_proprietary_headers
endif

LOCAL_SHARED_LIBRARIES:= libais libais_log

LOCAL_MODULE := libais_ov490

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

#ov490 bin
#LOCAL_PATH:= $(call my-dir)
#include $(CLEAR_VARS)

#move to path:device/qcom/sm6150_au/conti/d01/firmware/camera
#LOCAL_MODULE       := firmware_freerun.bin
#LOCAL_MODULE_TAGS  := optional
#LOCAL_MODULE_CLASS := ETC
#LOCAL_MODULE_PATH  := $(TARGET_OUT_ETC)/firmware
#LOCAL_SRC_FILES    := $(LOCAL_MODULE)

#include $(BUILD_PREBUILT)
