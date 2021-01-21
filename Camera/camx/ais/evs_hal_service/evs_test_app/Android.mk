LOCAL_PATH:= $(call my-dir)

##################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    evs_app.cpp \
    EvsStateControl.cpp \
    RenderBase.cpp \
    RenderDirectView.cpp \
    RenderTopView.cpp \
    ConfigManager.cpp \
    glError.cpp \
    shader.cpp \
    TexWrapper.cpp \
    VideoTex.cpp \
    StreamHandler.cpp \
    WindowSurface.cpp \
    FormatConvert.cpp \

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    liblog \
    libutils \
    libui \
    libgui \
    libhidlbase \
    libhidltransport \
    libEGL \
    libGLESv2 \
    libhardware \
    libpng \
    libjsoncppso \
    android.hardware.automotive.evs@1.0 \
    android.hardware.automotive.vehicle@2.0 \


LOCAL_STRIP_MODULE := keep_symbols

LOCAL_INIT_RC := evs_app.rc

LOCAL_MODULE:= evs_ais_app
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += -DLOG_TAG=\"EvsApp\"
LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES
LOCAL_CFLAGS += -Wall -Werror -Wunused -Wunreachable-code

#compile time AIS flag
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/qcom/display

include $(BUILD_EXECUTABLE)

#AIS camera config file
include $(CLEAR_VARS)
LOCAL_MODULE := config_qcarcam.json
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/automotive/evs
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := CarFromTop1.png
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/automotive/evs
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := LabeledChecker1.png
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/automotive/evs
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := evs_ais_app_default_resources
LOCAL_REQUIRED_MODULES := \
    CarFromTop1.png \
    LabeledChecker1.png \
    config_qcarcam.json
include $(BUILD_PHONY_PACKAGE)
