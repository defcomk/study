#======================================================================
#makefile for ais_server
#======================================================================
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/bin

MY_AIS_ROOT := $(LOCAL_PATH)

LOCAL_LDFLAGS :=

LOCAL_SRC_FILES:= \
	CameraMulticlient/common/src/linux/ais_conn.c \
	CameraMulticlient/common/src/ais_event_queue.c \
	CameraMulticlient/server/src/ais_server.c

LOCAL_C_INCLUDES:= \
	$(MY_AIS_ROOT)/API/inc \
	$(MY_AIS_ROOT)/CameraMulticlient/common/inc \
	$(MY_AIS_ROOT)/CameraMulticlient/server/inc \
	$(MY_AIS_ROOT)/CameraOSServices/CameraOSServices/inc \
	$(MY_AIS_ROOT)/CameraOSServices/CameraOSServicesMMOSAL/inc \
	$(MY_AIS_ROOT)/CameraQueue/CameraQueue/inc \
	$(MY_AIS_ROOT)/Common/inc \
	$(MY_AIS_ROOT)/Engine/inc

LOCAL_HEADER_LIBRARIES := libmmosal_headers

LOCAL_CFLAGS := $(ais_compile_cflags) \
	-Wno-unused-parameter -Wno-sign-compare

LOCAL_CFLAGS += -DTAEGET_CONTI_DIDI_B01_BOARD
ifeq ($(AIS_BUILD_STATIC),true)
LOCAL_WHOLE_STATIC_LIBRARIES  += libais libais_log
else
LOCAL_SHARED_LIBRARIES:= libais libais_log
endif

LOCAL_MODULE:= ais_server
LOCAL_MODULE_TAGS := optional

ifeq ($(AIS_32_BIT_FLAG), true)
LOCAL_32_BIT_ONLY := true
endif

include $(BUILD_EXECUTABLE)
