LOCAL_PATH:= $(call my-dir)

##################################
include $(CLEAR_VARS)

LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/bin

LOCAL_SRC_FILES := \
    gl_wrapper.cpp \
    video_capture.cpp \
    buffer_copy.cpp \

#compile AIS wrapper
LOCAL_SRC_FILES +=ais_evs_gldisplay.cpp ais_service.cpp ais_evs_enumerator.cpp ais_evs_camera.cpp

LOCAL_SHARED_LIBRARIES := \
    android.hardware.automotive.evs@1.0 \
    libui \
    libEGL \
    libGLESv2 \
    libbase \
    libbinder \
    libcutils \
    libhardware \
    libhidlbase \
    libhidltransport \
    liblog \
    libutils \

#Android P and above
ifeq ($(call is-platform-sdk-version-at-least,28),true)
LOCAL_SHARED_LIBRARIES += libgui_vendor
else
LOCAL_SHARED_LIBRARIES += libgui
endif

LOCAL_INIT_RC := android.hardware.automotive.evs@1.0-ais.rc

#link AIS libraries
LOCAL_SHARED_LIBRARIES += libais_client libdl
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/qcarcam
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/qcom/display

LOCAL_MODULE := android.hardware.automotive.evs@1.0-ais

LOCAL_MODULE_TAGS := optional
LOCAL_STRIP_MODULE := keep_symbols

LOCAL_CFLAGS += -DLOG_TAG=\"EvsAISDriver\"
LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES
LOCAL_CFLAGS += -Wall -Werror -Wunused -Wunreachable-code

#RGBA conversion
LOCAL_CFLAGS += -DENABLE_RGBA_CONVERSION

# NOTE:  It can be helpful, while debugging, to disable optimizations
#LOCAL_CFLAGS += -O0 -g

include $(BUILD_EXECUTABLE)
