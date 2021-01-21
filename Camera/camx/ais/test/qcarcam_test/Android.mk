#
# qcarcam_test
#
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)


LOCAL_LDFLAGS :=

LOCAL_SRC_FILES:= src/qcarcam_test.cpp

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../../API/inc \
	$(LOCAL_PATH)/../../Common/inc \
	$(LOCAL_PATH)/../../CameraOSServices/CameraOSServices/inc \
	$(LOCAL_PATH)/../test_util/inc \
	$(TARGET_OUT_HEADERS)/mm-osal/include \
	$(TARGET_OUT_HEADERS)/common/inc

ifneq ($(TARGET_USES_GRALLOC1), true)
    LOCAL_C_INCLUDES  += $(TOP)/hardware/qcom/display/libgralloc
else
    LOCAL_C_INCLUDES  += $(TOP)/hardware/qcom/display/libgralloc1
endif #TARGET_USES_GRALLOC1

LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include/

LOCAL_CFLAGS :=-Werror \
	-D_ANDROID_ \
	-DC2D_DISABLED \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers

ifeq ($(call is-platform-sdk-version-at-least,28),true)
LOCAL_HEADER_LIBRARIES := libmmosal_proprietary_headers
endif

LOCAL_SHARED_LIBRARIES:= libais_client libais_test_util \
	libmmosal_proprietary liblog \
	libutils

LOCAL_MODULE:= qcarcam_test
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional

LOCAL_PRELINK_MODULE:= false

ifeq ($(AIS_32_BIT_FLAG), true)
LOCAL_32_BIT_ONLY := true
endif

include $(BUILD_EXECUTABLE)
