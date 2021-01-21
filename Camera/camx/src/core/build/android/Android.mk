ifeq ($(CAMX_PATH),)
LOCAL_PATH := $(abspath $(call my-dir)/../..)
CAMX_PATH := $(abspath $(LOCAL_PATH)/../..)
else
LOCAL_PATH := $(CAMX_PATH)/src/core
endif

include $(CLEAR_VARS)

# Module supports function call tracing via ENABLE_FUNCTION_CALL_TRACE
# Required before including common.mk
SUPPORT_FUNCTION_CALL_TRACE := 1

# Get definitions common to the CAMX project here
include $(CAMX_PATH)/build/infrastructure/android/common.mk

LOCAL_SRC_FILES :=                          \
    camxactuatordata.cpp                    \
    camxcmdbuffer.cpp                       \
    camxcmdbuffermanager.cpp                \
    camxdeferredrequestqueue.cpp            \
    camxeepromdata.cpp                      \
    camxerrorinducer.cpp                    \
    camxoisdata.cpp                         \
    camxflashdata.cpp                       \
    camxhwcontext.cpp                       \
    camxhwenvironment.cpp                   \
    camxhwfactory.cpp                       \
    camximagebuffer.cpp                     \
    camximagebuffermanager.cpp              \
    camximagesensordata.cpp                 \
    camximagesensormoduledata.cpp           \
    camximagesensormoduledatamanager.cpp    \
    camxmempoolgroup.cpp                    \
    camxmempoolmgr.cpp                      \
    camxmetabuffer.cpp                      \
    camxmetadatapool.cpp                    \
    camxnode.cpp                            \
    camxoverridesettingsfile.cpp            \
    camxpacket.cpp                          \
    camxpacketbuilder.cpp                   \
    camxpacketresource.cpp                  \
    camxpdafdata.cpp                        \
    camxpipeline.cpp                        \
    camxsession.cpp                         \
    camxsettingsmanager.cpp                 \
    camxstatsparser.cpp                     \
    camxtest.cpp                            \
    camxtuningdatamanager.cpp               \
    camxvendortags.cpp                      \
    g_camxproperties.cpp                    \
    g_camxsettings.cpp                      \
    oem/camxcustomization.cpp

LOCAL_INC_FILES :=                      \
    camxactuatordata.h                  \
    camxcmdbuffer.h                     \
    camxcmdbuffermanager.h              \
    camxdeferredrequestqueue.h          \
    camxeepromdata.h                    \
    camxerrorinducer.h                  \
    camxfdproperty.h                    \
    camxoisdata.h                       \
    camxflashdata.h                     \
    camxhwcontext.h                     \
    camxhwenvironment.h                 \
    camxhwfactory.h                     \
    camxifeproperty.h                   \
    camximagebuffer.h                   \
    camximagebuffermanager.h            \
    camximagesensordata.h               \
    camximagesensormoduledata.h         \
    camximagesensormoduledatamanager.h  \
    camxjpegproperty.h                  \
    camxmempoolgroup.h                  \
    camxmempoolmgr.h                    \
    camxmetabuffer.h                    \
    camxmetadatapool.h                  \
    camxnode.h                          \
    camxoverridesettingsfile.h          \
    camxoverridesettingsstore.h         \
    camxpacket.h                        \
    camxpacketbuilder.h                 \
    camxpacketresource.h                \
    camxpdafdata.h                      \
    camxpipeline.h                      \
    camxpropertyblob.h                  \
    camxpropertydefs.h                  \
    camxsession.h                       \
    camxsettingsmanager.h               \
    camxstaticcaps.h                    \
    camxstatsinternalproperty.h         \
    camxstatsparser.h                   \
    camxtest.h                          \
    camxtuningdatamanager.h             \
    g_camxproperties.h                  \
    g_camxsettings.h                    \
    g_camxversion.h                     \
    oem/camxcustomization.h

# Put here any libraries that should be linked by CAMX projects
LOCAL_C_LIBS := $(CAMX_C_LIBS)

# Paths to included headers
LOCAL_C_INCLUDES := $(CAMX_C_INCLUDES)              \
    $(CAMX_CDK_PATH)/chi/                           \
    $(CAMX_CDK_PATH)/pdlib                          \
    $(CAMX_CDK_PATH)/fd/                            \
    $(CAMX_CDK_PATH)/generated/g_chromatix          \
    $(CAMX_CDK_PATH)/generated/g_parser             \
    $(CAMX_PATH)/ext                                \
    $(CAMX_PATH)/src/core/ncs                       \
    $(CAMX_PATH)/src/core/oem                       \
    $(CAMX_PATH)/src/hwl/titan17x                   \
    $(CAMX_OUT_HEADERS)/titan17x                    \
    hardware/libhardware/include                    \
    system/media/camera/include                     \
    hardware/qcom/display/include

LOCAL_HEADER_LIBRARIES += display_headers

# Compiler flags
LOCAL_CFLAGS := $(CAMX_CFLAGS)
LOCAL_CPPFLAGS := $(CAMX_CPPFLAGS)

# Binary name
LOCAL_MODULE := libcamxcore

include $(CAMX_BUILD_STATIC_LIBRARY)
-include $(CAMX_CHECK_WHINER)

# COPY below headers to out\target\product\<target>\obj\include\camx
LOCAL_COPY_HEADERS_TO := camx
LOCAL_COPY_HEADERS :=    camxtuningdatamanager.h
include $(BUILD_COPY_HEADERS)
