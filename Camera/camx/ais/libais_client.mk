#======================================================================
#makefile for libais_client.so
#======================================================================
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MY_AIS_ROOT := $(LOCAL_PATH)

LOCAL_CFLAGS := $(ais_client_compile_cflags) \
	-Wno-unused-parameter \
	-fvisibility=hidden
LOCAL_CFLAGS += -DTAEGET_CONTI_DIDI_B01_BOARD

LOCAL_C_INCLUDES += \
	$(MY_AIS_ROOT)/API/inc \
	$(MY_AIS_ROOT)/CameraMulticlient/common/inc \
	$(MY_AIS_ROOT)/CameraMulticlient/client/inc \
	$(MY_AIS_ROOT)/CameraOSServices/CameraOSServices/inc \
	$(MY_AIS_ROOT)/CameraOSServices/CameraOSServicesMMOSAL/inc \
	$(MY_AIS_ROOT)/CameraQueue/CameraQueue/inc \
	$(MY_AIS_ROOT)/CameraEventLog/inc \
	$(MY_AIS_ROOT)/Common/inc \
	$(MY_AIS_ROOT)/Engine/inc \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include \
	$(TARGET_OUT_HEADERS)/mm-osal/include

ifneq ($(ENABLE_HYP), true)
	LOCAL_SRC_DIR :=\
		$(MY_AIS_ROOT)/CameraMulticlient/common/src/linux \
		$(MY_AIS_ROOT)/CameraMulticlient/common/src \
		$(MY_AIS_ROOT)/CameraMulticlient/client/src \
		$(MY_AIS_ROOT)/CameraOSServices/CameraOSServicesMMOSAL/src \
		$(MY_AIS_ROOT)/CameraQueue/CameraQueueSCQ/src \
		$(MY_AIS_ROOT)/Common/src
else
	LOCAL_C_INCLUDES += \
		$(TARGET_OUT_HEADERS)/mm-hab

	LOCAL_SRC_DIR :=\
		$(MY_AIS_ROOT)/CameraMulticlient/common/src/hypervisor \
		$(MY_AIS_ROOT)/CameraMulticlient/common/src \
		$(MY_AIS_ROOT)/CameraMulticlient/client/src \
		$(MY_AIS_ROOT)/CameraOSServices/CameraOSServicesMMOSAL/src \
		$(MY_AIS_ROOT)/CameraQueue/CameraQueueSCQ/src \
		$(MY_AIS_ROOT)/Common/src

	LOCAL_SHARED_LIBRARIES := libuhab liblog
	LOCAL_CFLAGS += -DUSE_HYP
endif

LOCAL_SRC_FILES += $(shell find $(LOCAL_SRC_DIR) -maxdepth 1 -name '*.c' | sed s:^$(LOCAL_PATH)::g )

LOCAL_LDFLAGS :=

LOCAL_HEADER_LIBRARIES := libmmosal_proprietary_headers

LOCAL_SHARED_LIBRARIES += libmmosal_proprietary liblog

LOCAL_MODULE_OWNER := qti
LOCAL_PROPRIETARY_MODULE := true

LOCAL_MODULE := libais_client

LOCAL_PRELINK_MODULE:= false

ifeq ($(AIS_32_BIT_FLAG), true)
LOCAL_32_BIT_ONLY := true
endif

include $(BUILD_SHARED_LIBRARY)
