# common.mk - Makefile for the CamX driver
#
# Things in this file are global to all CamX sub-projects. Consider adding things like
# include paths and linked libraries to individual sub-projects instead.
#
ifeq ($(CAMX_PATH),)
$(error CAMX_PATH should have been defined!)
else
CAMX_BUILD_PATH := $(CAMX_PATH)/build/infrastructure/android
endif # ($(CAMX_PATH),)

ifeq ($(CAMX_CHICDK_PATH),)
    CAMX_CHICDK_PATH := $(CAMX_PATH)/../chi-cdk
endif

# Inherit definitions from CHI-CDK
include $(CAMX_CHICDK_PATH)/vendor/common.mk

# Set to enable function call profiling on gcc for supported modules
# ANDed with SUPPORTS_FUNCTION_CALL_TRACE in each makefile that includes common.mk
ENABLE_FUNCTION_CALL_TRACE := 0

# CAMX_OS can be linux or win32
CAMX_OS := linux

CAMX_LIB := camera.qcom

LOCAL_MODULE_PATH_32 := $(TARGET_OUT_VENDOR)/lib
LOCAL_MODULE_PATH_64 := $(TARGET_OUT_VENDOR)/lib64

CAMX_LIB_OUTPUT_PATH := camera/components
CAMX_BIN_OUTPUT_PATH := camera

# Helper functions to check for CDK
ifeq ($(CAMX_PATH_PREFIX),)
CAMX_PATH_PREFIX := vendor/qcom/proprietary
endif

CAMX_CDK_PATH          := $(CAMX_PATH_PREFIX)/chi-cdk/cdk
CAMX_VENDOR_PATH       := $(CAMX_PATH_PREFIX)/chi-cdk/vendor
CAMX_SYSTEM_PATH       := $(CAMX_PATH_PREFIX)/camx-lib/system
CAMX_SYSTEM_STATS_PATH := $(CAMX_PATH_PREFIX)/camx-lib-stats/system
CAMX_SYSTEM_3A_PATH    := $(CAMX_PATH_PREFIX)/camx-lib-3a/system
CAMX_OEM1IQ_PATH       := $(CAMX_PATH_PREFIX)/oemiq-ss/oem1iq
CAMX_EXT_PATH          := $(CAMX_PATH_PREFIX)/ext
CAMX_TEST_PATH         := $(CAMX_PATH_PREFIX)/test

ifeq ($(CAMX_OUT_HEADERS),)
CAMX_OUT_HEADERS := $(TARGET_OUT_HEADERS)/camx
endif

# Inherit C Includes from CDK and add others
CAMX_C_INCLUDES :=                          \
    $(CAMX_C_INCLUDES)                      \
    $(CAMX_PATH)/src/core                   \
    $(CAMX_PATH)/src/core/chi               \
    $(CAMX_PATH)/src/core/hal               \
    $(CAMX_PATH)/src/core/halutils          \
    $(CAMX_PATH)/src/csl                    \
    $(CAMX_PATH)/src/osutils                \
    $(CAMX_PATH)/src/sdk                    \
    $(CAMX_PATH)/src/swl                    \
    $(CAMX_PATH)/src/swl/jpeg               \
    $(CAMX_PATH)/src/swl/sensor             \
    $(CAMX_PATH)/src/swl/stats              \
    $(CAMX_PATH)/src/utils                  \
    $(CAMX_PATH)/src/utils/scope            \
    $(CAMX_OUT_HEADERS)                     \
    $(CAMX_OUT_HEADERS)/titan17x            \
    $(CAMX_OUT_HEADERS)/fd/fdengine         \
    $(CAMX_OUT_HEADERS)/swprocessalgo       \
    $(CAMX_OUT_HEADERS)/localhistogramalgo

ifeq ($(IQSETTING),OEM1)
ifeq ($(CAMX_OEM1IQ_PATH),)
    $(error CAMX_OEM1IQ_PATH is not defined!)
endif
CAMX_C_INCLUDES := $(CAMX_C_INCLUDES) \
    $(CAMX_OEM1IQ_PATH)/chromatix/g_chromatix
else
CAMX_C_INCLUDES := $(CAMX_C_INCLUDES) \
    $(CAMX_CDK_PATH)/generated/g_chromatix
endif

# Always include the system paths last
CAMX_C_INCLUDES += $(CAMX_SYSTEM_INCLUDES)

LLVM_VER4 = $(findstring 4.0,$(SDCLANG_PATH))
ifneq (,$(strip $(LLVM_VER4)))
        CAMX_CFLAGS += -Wno-address-of-packed-member
endif

ifeq ($(IQSETTING),OEM1)
CAMX_CFLAGS +=                              \
    -DOEM1IQ=1
endif

ifeq ($(CAMX_PREBUILT_LINK),1)
LOCAL_LDFLAGS := $(CAMX_ADDITIONAL_LDFLAGS)
endif #ifeq ($(CAMX_PREBUILT_LINK),1)

# This is a hack to just override the -D flags the compiler passes into CAMX. Unfortunately,
# if you just change TARGET_BUILD_TYPE on the command line like you're "supposed" to, it
# will require a full Android debug build to link against the debug C libs..
ifneq ($(CAMXDEBUG),)
    # Add the flags we want. -Wno-psabi is due to a strange GCC warning that can't otherwise
    # be supressed.
    CAMX_CFLAGS += -DDEBUG -UNDEBUG -Wno-type-limits -Wno-sign-compare
endif # CAMXDEBUG

ifeq ($(CAMXMEMSPY),1)
    CAMX_CFLAGS += -DCAMX_USE_MEMSPY=1
endif # CAMXMEMSPY

CAMX_CFLAGS += -fcxx-exceptions
CAMX_CFLAGS += -fexceptions

ifeq ($(CAMX_EXT_VBUILD),)
# Linux build always uses SDLLVM
LOCAL_SDCLANG := true
endif

# Release builds vs debug builds
ifeq ($(CAMXDEBUG),)
    # Use the highest optimization level with SDLLVM and
    # use the latest version (>=4.0)
    ifeq ($(LOCAL_SDCLANG), true)
        LOCAL_SDCLANG_OFAST := true
        SDCLANG_FLAG_DEFS := $(CAMX_CHICDK_PATH)/vendor/sdllvm-flag-defs.mk
        SDCLANG_VERSION_DEFS := $(CAMX_CHICDK_PATH)/vendor/sdllvm-selection.mk
        -include $(SDCLANG_VERSION_DEFS)
    endif # ($(LOCAL_SDCLANG), true)
endif # ($(CAMXDEBUG),)

LOCAL_SHARED_LIBRARIES += libcdsprpc
LOCAL_SHARED_LIBRARIES += libqdMetaData
LOCAL_SHARED_LIBRARIES += libsnsapi libqmi_common_so libqmi_cci libqmi_encdec libprotobuf-cpp-full libhardware

LOCAL_STATIC_LIBRARIES += libcamxgenerated

LOCAL_WHINER_RULESET := camx
CAMX_CHECK_WHINER := $(CAMX_BUILD_PATH)/check-whiner.mk

# Compile all HWLs in VGDB builds
ifneq ($(CAMX_EXT_VBUILD),)
CAMX_BUILD_EMULATED_SENSOR := 1
endif # ($(CAMX_EXT_VBUILD),)

# These are wrappers for commonly used build rules
CAMX_BUILD_STATIC_LIBRARY := $(CAMX_BUILD_PATH)/camx-build-static-library.mk
CAMX_BUILD_SHARED_LIBRARY := $(CAMX_BUILD_PATH)/camx-build-shared-library.mk
CAMX_BUILD_EXECUTABLE     := $(CAMX_BUILD_PATH)/camx-build-executable.mk
