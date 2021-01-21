ifeq ($(CAMX_PATH),)
LOCAL_PATH := $(abspath $(call my-dir)/../..)
CAMX_PATH := $(abspath $(LOCAL_PATH)/../../..)
else
LOCAL_PATH := $(CAMX_PATH)/src/hwl/ispiqmodule
endif

include $(CLEAR_VARS)

# Module supports function call tracing via ENABLE_FUNCTION_CALL_TRACE
# Required before including common.mk
SUPPORT_FUNCTION_CALL_TRACE := 1

# Get definitions common to the CAMX project here
include $(CAMX_PATH)/build/infrastructure/android/common.mk

LOCAL_SRC_FILES :=                  \
    camxbpsabf40.cpp                \
    camxbpsawbbgstats14.cpp         \
    camxbpsbpcpdpc20.cpp            \
    camxbpscc13.cpp                 \
    camxbpscst12.cpp                \
    camxbpsdemosaic36.cpp           \
    camxbpsdemux13.cpp              \
    camxbpsgamma16.cpp              \
    camxbpsgic30.cpp                \
    camxbpsgtm10.cpp                \
    camxbpshdr22.cpp                \
    camxbpshdrbhiststats13.cpp      \
    camxbpshnr10.cpp                \
    camxbpslinearization34.cpp      \
    camxbpslsc34.cpp                \
    camxbpspedestal13.cpp           \
    camxbpswb13.cpp                 \
    camxifeabf34.cpp                \
    camxifeawbbgstats14.cpp         \
    camxifebfstats23.cpp            \
    camxifebfstats24.cpp            \
    camxifebhiststats14.cpp         \
    camxifebls12.cpp                \
    camxifebpcbcc50.cpp             \
    camxifecamif.cpp                \
    camxifecamiflite.cpp            \
    camxifecc12.cpp                 \
    camxifecrop10.cpp               \
    camxifecst12.cpp                \
    camxifecsstats14.cpp            \
    camxifedemosaic36.cpp           \
    camxifedemosaic37.cpp           \
    camxifedemux13.cpp              \
    camxifeds410.cpp                \
    camxifedualpd10.cpp             \
    camxifegamma16.cpp              \
    camxifegtm10.cpp                \
    camxifehdr20.cpp                \
    camxifehdr22.cpp                \
    camxifehdr23.cpp                \
    camxifehdrbestats15.cpp         \
    camxifehdrbhiststats13.cpp      \
    camxifehvx.cpp                  \
    camxifeihiststats12.cpp         \
    camxifelinearization33.cpp      \
    camxifelsc34.cpp                \
    camxifemnds16.cpp               \
    camxifepdpc11.cpp               \
    camxifepedestal13.cpp           \
    camxifeprecrop10.cpp            \
    camxifer2pd10.cpp               \
    camxiferoundclamp11.cpp         \
    camxifersstats14.cpp            \
    camxifetintlessbgstats15.cpp    \
    camxifewb12.cpp                 \
    camxipe2dlut10.cpp              \
    camxipeanr10.cpp                \
    camxipeasf30.cpp                \
    camxipecac22.cpp                \
    camxipechromaenhancement12.cpp  \
    camxipechromasuppression20.cpp  \
    camxipecolortransform12.cpp     \
    camxipecolorcorrection13.cpp    \
    camxipegamma15.cpp              \
    camxipegrainadder10.cpp         \
    camxipeica.cpp                  \
    camxipeltm13.cpp                \
    camxipesce11.cpp                \
    camxipetf10.cpp                 \
    camxipeupscaler12.cpp           \
    camxipeupscaler20.cpp           \
    camxiqinterface.cpp             \
    camxswtmc11.cpp

LOCAL_INC_FILES :=                  \
    camxbpsabf40.h                  \
    camxbpsawbbgstats14.h           \
    camxbpsbpcpdpc20.h              \
    camxbpscc13.h                   \
    camxbpscst12.h                  \
    camxbpsdemosaic36.h             \
    camxbpsdemux13.h                \
    camxbpsgamma16.h                \
    camxbpsgic30.h                  \
    camxbpsgtm10.h                  \
    camxbpshdr22.h                  \
    camxbpshnr10.h                  \
    camxbpshdrbhiststats13.h        \
    camxbpslinearization34.h        \
    camxbpslsc34.h                  \
    camxbpspedestal13.h             \
    camxbpswb13.h                   \
    camxifeabf34.h                  \
    camxifeawbbgstats14.h           \
    camxifebfstats23.h              \
    camxifebfstats24.h              \
    camxifebhiststats14.h           \
    camxifebls12.h                  \
    camxifebpcbcc50.h               \
    camxifecamif.h                  \
    camxifecamiflite.h              \
    camxifecc12.h                   \
    camxifecrop10.h                 \
    camxifecsstats14.h              \
    camxifecst12.h                  \
    camxifedemosaic36.h             \
    camxifedemosaic37.h             \
    camxifedemux13.h                \
    camxifeds410.h                  \
    camxifedualpd10.h               \
    camxifegamma16.h                \
    camxifegtm10.h                  \
    camxifehdr20.h                  \
    camxifehdr23.h                  \
    camxifehdrbestats15.h           \
    camxifehdrbhiststats13.h        \
    camxifehvx.h                    \
    camxifeihiststats12.h           \
    camxifelinearization33.h        \
    camxifelsc34.h                  \
    camxifemnds16.h                 \
    camxifepdpc11.h                 \
    camxifepedestal13.h             \
    camxifeprecrop10.h              \
    camxifer2pd10.h                 \
    camxiferoundclamp11.h           \
    camxifersstats14.h              \
    camxifetintlessbgstats15.h      \
    camxifewb12.h                   \
    camxipe2dlut10.h                \
    camxipeanr10.h                  \
    camxipeasf30.h                  \
    camxipecac22.h                  \
    camxipechromaenhancement12.h    \
    camxipechromasuppression20.h    \
    camxipecolortransform12.h       \
    camxipecolorcorrection13.h      \
    camxipegamma15.h                \
    camxipegrainadder10.h           \
    camxipeica.h                    \
    camxipeicatestdata.h            \
    camxipeltm13.h                  \
    camxipesce11.h                  \
    camxipetf10.h                   \
    camxipeupscaler12.h             \
    camxipeupscaler20.h             \
    camxiqinterface.h               \
    camxispiqmodule.h               \
    camxswtmc11.h

# Put here any libraries that should be linked by CAMX projects
LOCAL_C_LIBS := $(CAMX_C_LIBS)

# Paths to included headers

LOCAL_C_INCLUDES := $(CAMX_C_INCLUDES)                              \
    $(CAMX_PATH)/src/core                                           \
    $(CAMX_CDK_PATH)/generated/g_parser                             \
    $(CAMX_PATH)/src/hwl/dspinterfaces                              \
    $(CAMX_PATH)/src/hwl/ispiqmodule                                \
    $(CAMX_PATH)/src/hwl/ife                                        \
    $(CAMX_PATH)/src/hwl/ipe                                        \
    $(CAMX_PATH)/src/hwl/iqsetting                                  \
    $(CAMX_PATH)/src/hwl/titan17x                                   \
    $(CAMX_OUT_HEADERS)                                             \
    $(CAMX_OUT_HEADERS)/titan17x

ifeq ($(IQSETTING),OEM1)
LOCAL_C_INCLUDES +=                                                 \
    $(CAMX_OEM1IQ_PATH)/iqsetting                                   \
    $(CAMX_OEM1IQ_PATH)/chromatix/g_chromatix
else
LOCAL_C_INCLUDES +=                                                 \
    $(CAMX_PATH)/src/hwl/iqinterpolation                            \
    $(CAMX_CDK_PATH)/generated/g_chromatix
endif

# Compiler flags
LOCAL_CFLAGS := $(CAMX_CFLAGS)
LOCAL_CPPFLAGS := $(CAMX_CPPFLAGS)

ifeq ($(CAMX_EXT_VBUILD),)
LOCAL_CFLAGS += -Wno-address-of-packed-member
LOCAL_CPPFLAGS += -Wno-address-of-packed-member
endif

# Binary name
LOCAL_MODULE := libcamxhwliqmodule

include $(CAMX_BUILD_STATIC_LIBRARY)
-include $(CAMX_CHECK_WHINER)
