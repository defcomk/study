
#camera test lib
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)


LOCAL_LDFLAGS :=

LOCAL_SRC_FILES:= \
				  src/QCarCamInfo.cpp \
				  src/QCarCamBusiness.cpp \
				  src/QCarCamDisplay.cpp \
				  src/QCarCamLogic.cpp
				  
LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../../API/inc \
	$(LOCAL_PATH)/../../Common/inc \
	$(LOCAL_PATH)/../../CameraOSServices/CameraOSServices/inc \
	$(LOCAL_PATH)/../../CameraQueue/CameraQueue/inc \
	$(LOCAL_PATH)/../test_util/inc \
	$(LOCAL_PATH)/inc \
	$(TARGET_OUT_HEADERS)/mm-osal/include \
	$(TARGET_OUT_HEADERS)/common/inc

LOCAL_HEADER_LIBRARIES := libmmosal_headers

ifneq ($(TARGET_USES_GRALLOC1), true)
    LOCAL_C_INCLUDES  += $(TOP)/hardware/qcom/display/libgralloc
else
    LOCAL_C_INCLUDES  += $(TOP)/hardware/qcom/display/libgralloc1
endif #TARGET_USES_GRALLOC1

LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include/

LOCAL_CFLAGS :=-Werror -D_ANDROID_ -DC2D_DISABLED\
	-Wno-unused-variable -Wno-unused-parameter -Wno-missing-field-initializers


LOCAL_SHARED_LIBRARIES:= libais_client libais_test_util \
	libmmosal_proprietary liblog \
	libutils \
	libhidlbase libhidltransport
	
LOCAL_MODULE := libcameratest

LOCAL_PRELINK_MODULE:= false

LOCAL_MODULE_OWNER := qti
LOCAL_PROPRIETARY_MODULE := true


ifeq ($(AIS_32_BIT_FLAG), true)
LOCAL_32_BIT_ONLY := true
endif

include $(BUILD_SHARED_LIBRARY)


#
# qcarcam_test
#
# LOCAL_PATH := $(call my-dir)
# include $(CLEAR_VARS)


# LOCAL_LDFLAGS :=

# LOCAL_SRC_FILES:= main.cpp \
# 				  src/QCarCamInfo.cpp \
# 				  src/QCarCamBusiness.cpp \
# 				  src/QCarCamDisplay.cpp \
# 				  src/QCarCamLogic.cpp

# LOCAL_C_INCLUDES:= \
# 	$(LOCAL_PATH)/../../API/inc \
# 	$(LOCAL_PATH)/../../Common/inc \
# 	$(LOCAL_PATH)/../../CameraOSServices/CameraOSServices/inc \
# 	$(LOCAL_PATH)/../../CameraQueue/CameraQueue/inc \
# 	$(LOCAL_PATH)/../test_util/inc \
# 	$(LOCAL_PATH)/inc \
# 	$(TARGET_OUT_HEADERS)/mm-osal/include \
# 	$(TARGET_OUT_HEADERS)/common/inc

# LOCAL_HEADER_LIBRARIES := libmmosal_headers

# ifneq ($(TARGET_USES_GRALLOC1), true)
#     LOCAL_C_INCLUDES  += $(TOP)/hardware/qcom/display/libgralloc
# else
#     LOCAL_C_INCLUDES  += $(TOP)/hardware/qcom/display/libgralloc1
# endif #TARGET_USES_GRALLOC1

# LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include/

# LOCAL_CFLAGS :=-Werror -D_ANDROID_ -DC2D_DISABLED\
# 	-Wno-unused-variable -Wno-unused-parameter -Wno-missing-field-initializers


# LOCAL_SHARED_LIBRARIES:= libais_client_vendor libais_test_util_vendor \
# 	libmmosal_proprietary liblog \
# 	libutils android.hardware.automotive.vehicle@2.0 \
# 	libhidlbase libhidltransport
	
# LOCAL_MODULE:= cameratest
# LOCAL_MODULE_TAGS := optional
# LOCAL_MODULE_OWNER := qti
# LOCAL_PROPRIETARY_MODULE := true

# #LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/bin
# LOCAL_PRELINK_MODULE:= false

# ifeq ($(AIS_32_BIT_FLAG), true)
# LOCAL_32_BIT_ONLY := true
# endif

# include $(BUILD_EXECUTABLE)

