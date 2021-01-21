LOCAL_PATH := $(call my-dir)
AIS_ROOT_DIR_PATH := $(call my-dir)
include $(CLEAR_VARS)

#chromatix version 309 for 8996
CHROMATIX_VERSION := 0309

#AIS metal or through CAMX
AIS_BUILD_CAMX := false

#build 64bit by default
AIS_32_BIT_FLAG := false

#Option to compile all libs statically into ais_server
AIS_BUILD_STATIC := false

ais_compile_cflags := -D_ANDROID_ -I$(TARGET_OUT_HEADERS)/mm-osal/include

ifeq ($(AIS_BUILD_STATIC),true)
ais_compile_cflags += \
	-DAIS_BUILD_STATIC_DEVICES \
	-DAIS_BUILD_STATIC_CONFIG
endif

ifeq ($(TARGET_USES_TV_TUNER),true)
ais_compile_cflags += -DENABLE_TV_TUNER
endif

ais_client_compile_cflags := $(ais_compile_cflags)
ais_compile_cflags += -Werror -fvisibility=hidden

#************* libais_log ************#
LOCAL_PATH := $(AIS_ROOT_DIR_PATH)
include $(LOCAL_PATH)/libais_log.mk

#************* libais_client ************#
LOCAL_PATH := $(AIS_ROOT_DIR_PATH)
include $(LOCAL_PATH)/libais_client.mk

#************* tests ************#
LOCAL_PATH := $(AIS_ROOT_DIR_PATH)
include $(LOCAL_PATH)/test/Android.mk

#ENABLE_HYP get set in device/qcom/msm8996_gvmq/msm8996_gvmq.mk
ifneq ($(ENABLE_HYP), true)
ifneq ($(AIS_BUILD_CAMX), true)
#************* libcamera_config ************#
LOCAL_PATH := $(AIS_ROOT_DIR_PATH)
include $(LOCAL_PATH)/libais_config.mk

#************* input libs ************#
LOCAL_PATH := $(AIS_ROOT_DIR_PATH)
include $(LOCAL_PATH)/ImagingInputs/Android.mk
endif
#************* libais ************#
LOCAL_PATH := $(AIS_ROOT_DIR_PATH)
include $(LOCAL_PATH)/libais.mk

#************* ais_server ************#
LOCAL_PATH := $(AIS_ROOT_DIR_PATH)
include $(LOCAL_PATH)/ais_server.mk
endif #ENABLE_HYP

#************* EVS HAL ************#
LOCAL_PATH := $(AIS_ROOT_DIR_PATH)
include $(LOCAL_PATH)/evs_hal_service/Android.mk
