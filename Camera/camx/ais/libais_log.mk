#======================================================================
#makefile for libais_log.so
#======================================================================
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MY_AIS_ROOT := $(LOCAL_PATH)

LOCAL_CFLAGS := $(ais_log_compile_cflags) -Wno-unused-variable -Wno-unused-parameter -fvisibility=hidden

LOCAL_C_INCLUDES += \
	$(MY_AIS_ROOT)/Common/inc \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include \
	$(TARGET_OUT_HEADERS)/mm-osal/include

LOCAL_HEADER_LIBRARIES := libmmosal_headers

LOCAL_SRC_DIR :=\
	$(MY_AIS_ROOT)/Common/src

LOCAL_SHARED_LIBRARIES := libmmosal_proprietary liblog

LOCAL_SRC_FILES += $(shell find $(LOCAL_SRC_DIR) -maxdepth 1 -name '*.c' | sed s:^$(LOCAL_PATH)::g )
LOCAL_LDLIBS := -L$(SYSROOT)/usr/lib -llog

LOCAL_LDFLAGS :=

LOCAL_MODULE := libais_log

LOCAL_MODULE_OWNER := qti
LOCAL_PROPRIETARY_MODULE := true

ifeq ($(AIS_32_BIT_FLAG), true)
LOCAL_32_BIT_ONLY := true
endif

include $(BUILD_SHARED_LIBRARY)
