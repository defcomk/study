#===================
#libais_test_util.so
#===================
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/inc \
	$(LOCAL_PATH)/../../API/inc \
	$(LOCAL_PATH)/../../Common/inc \
	$(LOCAL_PATH)/../../CameraOSServices/CameraOSServices/inc \
	$(TARGET_OUT_HEADERS)/mm-osal/include \
	$(TARGET_OUT_HEADERS)/qcom/display \
	$(TARGET_OUT_HEADERS)/common/inc \
	external/libxml2/include \
	external/icu/icu4c/source/common

LOCAL_HEADER_LIBRARIES := libmmosal_headers

LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include/
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/mm-osal/include

LOCAL_SRC_FILES := src/test_util.cpp src/test_util_standalone.cpp src/la/test_util_la.cpp

LOCAL_CFLAGS := -D_ANDROID_ \
	-DC2D_DISABLED \
	-Werror -Wno-unused-parameter

ifeq ($(call is-platform-sdk-version-at-least,28),true)
	LOCAL_CFLAGS += -DTESTUTIL_ANDROID_P
endif

LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := libais_client \
	libmmosal_proprietary liblog \
	libxml2 \
	libdl libcutils libEGL libGLESv2 libui  \
	libutils

ifeq ($(call is-platform-sdk-version-at-least,28),true)
LOCAL_SHARED_LIBRARIES += libgui_vendor
else
LOCAL_SHARED_LIBRARIES += libgui_vendor
endif

LOCAL_MODULE := libais_test_util
LOCAL_PROPRIETARY_MODULE := true

LOCAL_PRELINK_MODULE:= false

ifeq ($(AIS_32_BIT_FLAG), true)
LOCAL_32_BIT_ONLY := true
endif

include $(BUILD_SHARED_LIBRARY)


#===================
#config files
#===================
include $(CLEAR_VARS)
LOCAL_MODULE:= 1cam.xml
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := config/1cam.xml
LOCAL_MODULE_TAGS := optional
#LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/bin
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)
include $(CLEAR_VARS)
LOCAL_MODULE:= 8cam.xml
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := config/8cam.xml
LOCAL_MODULE_TAGS := optional
#LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/bin
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)
