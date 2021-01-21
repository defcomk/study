#======================================================================
#makefile for libais.so
#======================================================================
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MY_AIS_ROOT := $(LOCAL_PATH)

LOCAL_C_INCLUDES += \
	$(MY_AIS_ROOT)/API/inc \
	$(MY_AIS_ROOT)/CameraOSServices/CameraOSServices/inc \
	$(MY_AIS_ROOT)/CameraOSServices/CameraOSServicesMMOSAL/inc \
	$(MY_AIS_ROOT)/CameraQueue/CameraQueue/inc \
	$(MY_AIS_ROOT)/Common/inc \
	$(MY_AIS_ROOT)/Engine/inc \
	$(MY_AIS_ROOT)/CameraEventLog/inc \
	$(MY_AIS_ROOT)/CameraMulticlient/common/inc \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include \
	$(TARGET_OUT_HEADERS)/mm-osal/include \
	$(TARGET_OUT_HEADERS)/common/inc \
	$(TARGET_OUT_HEADERS)/qcom/display \
	system/media/camera/include/system

LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include/

ifeq ($(AIS_BUILD_CAMX),false)
#Standalone AIS
LOCAL_C_INCLUDES += \
	$(MY_AIS_ROOT)/CameraConfig/inc \
	$(MY_AIS_ROOT)/CameraDevice/inc \
	$(MY_AIS_ROOT)/CameraPlatform/inc \
	$(MY_AIS_ROOT)/CameraPlatform/linux \
	$(MY_AIS_ROOT)/HWDrivers/API \
	$(MY_AIS_ROOT)/HWDrivers/API/IFEDriver \
	$(MY_AIS_ROOT)/HWDrivers/API/MIPICSIDriver \
	$(MY_AIS_ROOT)/ImagingInputs/ImagingInputDriver/inc \
	$(MY_AIS_ROOT)/ImagingInputs/ImagingInputDriver/inc/$(CHROMATIX_VERSION)

LOCAL_SRC_DIR :=\
	$(MY_AIS_ROOT)/Engine/src \
	$(MY_AIS_ROOT)/CameraDevice/src \
	$(MY_AIS_ROOT)/CameraEventLog/src \
	$(MY_AIS_ROOT)/CameraQueue/CameraQueueSCQ/src \
	$(MY_AIS_ROOT)/CameraOSServices/CameraOSServicesMMOSAL/src \
	$(MY_AIS_ROOT)/CameraPlatform/linux \
	$(MY_AIS_ROOT)/ImagingInputs/ImagingInputDriver/src \
	$(MY_AIS_ROOT)/ImagingInputs/ImagingInputDriver/src/linux

LOCAL_CFLAGS := $(ais_compile_cflags) \
	-DALOG_ENABLED \
	-Wno-unused-parameter
LOCAL_CFLAGS += -DTAEGET_CONTI_DIDI_B01_BOARD

else
#CAMX
LOCAL_C_INCLUDES += \
	$(MY_AIS_ROOT)/Engine/chi \
	$(MY_AIS_ROOT)/Engine/chi/inc \
	$(MY_AIS_ROOT)/../../chi-cdk/cdk/chi \
	$(MY_AIS_ROOT)/../../chi-cdk/cdk \
	$(MY_AIS_ROOT)/../../chi-cdk/cdk/common \
	$(MY_AIS_ROOT)/../../camx/src/core/hal \
	$(MY_AIS_ROOT)/../../camx/src/csl \
	$(MY_AIS_ROOT)/../../camx/src/utils \
	$(MY_AIS_ROOT)/../../camx/src/osutils

LOCAL_SRC_DIR :=\
	$(MY_AIS_ROOT)/Engine/chi \
	$(MY_AIS_ROOT)/Engine/chi/src \
	$(MY_AIS_ROOT)/CameraEventLog/src \
	$(MY_AIS_ROOT)/CameraOSServices/CameraOSServicesMMOSAL/src \
	$(MY_AIS_ROOT)/CameraQueue/CameraQueueSCQ/src

LOCAL_CFLAGS := $(ais_compile_cflags) \
	-DALOG_ENABLED \
	-D_LINUX \
	-DOS_ANDROID \
	-DUSE_CHI \
	-Wno-unused-variable -Wno-unused-parameter \
	-Wno-missing-field-initializers

LOCAL_CPPFLAGS += -std=c++14
endif

LOCAL_HEADER_LIBRARIES := libmmosal_proprietary_headers


LOCAL_C_INCLUDES += $(LOCAL_SRC_DIR)

LOCAL_SRC_FILES += $(shell find $(LOCAL_SRC_DIR) -maxdepth 1 -name '*.c' | sed s:^$(LOCAL_PATH)::g )
LOCAL_SRC_FILES += $(shell find $(LOCAL_SRC_DIR) -maxdepth 1 -name '*.cpp' | sed s:^$(LOCAL_PATH)::g )


LOCAL_LDFLAGS :=

LOCAL_CPPFLAGS +=

LOCAL_SHARED_LIBRARIES := libmmosal_proprietary liblog libais_log
ifeq ($(AIS_BUILD_CAMX),true)
LOCAL_SHARED_LIBRARIES += libdl libcutils libui \
	libutils \
	libcamera_metadata
endif

ifeq ($(AIS_BUILD_STATIC),true)
LOCAL_WHOLE_STATIC_LIBRARIES  += libais_log
ifeq ($(AIS_BUILD_CAMX),false)
LOCAL_WHOLE_STATIC_LIBRARIES  += libais_config libais_max9296 libais_max9296b
endif
endif

LOCAL_MODULE := libais

LOCAL_COPY_HEADERS_TO := qcarcam
LOCAL_COPY_HEADERS += API/inc/qcarcam.h
LOCAL_COPY_HEADERS += API/inc/qcarcam_types.h

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

############################################################
########## Build the library for header files ##############
############################################################
include $(CLEAR_VARS)
LOCAL_MODULE := libais_headers
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/./API/inc
include $(BUILD_HEADER_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libais_proprietary_headers
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/./API/inc
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_HEADER_LIBRARY)
