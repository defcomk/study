ifneq ($(CAMX_AUTOGEN_MK_INCLUDED),1)
CAMX_AUTOGEN_MK_INCLUDED := 1

LOCAL_PATH := $(abspath $(call my-dir)/../../..)

ifeq ($(CAMX_PATH),)
CAMX_PATH := $(LOCAL_PATH)
endif

CAMX_BUILD_PATH := $(CAMX_PATH)/build/infrastructure/android

include $(CLEAR_VARS)

# Get definitions common to the CAMX project here
include $(CAMX_BUILD_PATH)/common.mk

# Auto-generated files
# Note: using if check for xml file because this can get run in other directories
CAMX_TOOLS_PATH := $(CAMX_BUILD_PATH)/../tools

CAMX_TXTOUT_PATH=$(CAMX_BUILD_PATH)/built/settings

# Note: generating all settings files if any dependency changes to ensure we generate a complete camxoverridesettings.txt

# Core first, then HWLs
CAMX_SETTINGS_INPUTS =                                      \
    $(CAMX_PATH)/src/core/camxsettings.xml                  \
    $(CAMX_PATH)/src/hwl/titan17x/camxtitan17xsettings.xml

CAMX_SETTINGS_OUTPUTS =                                         \
    $(CAMX_PATH)/src/core/g_camxsettings.cpp                    \
    $(CAMX_PATH)/src/core/g_camxsettings.h                      \
    $(CAMX_PATH)/src/hwl/titan17x/g_camxtitan17xsettings.cpp    \
    $(CAMX_PATH)/src/hwl/titan17x/g_camxtitan17xsettings.h      \
    $(CAMX_PATH)/build/built/settings/camxoverridesettings.txt

CAMX_VERSION_OUTPUT = \
    $(CAMX_PATH)/src/core/g_camxversion.h

CAMX_PROPS_INPUT = \
    $(CAMX_PATH)/src/core/camxproperties.xml

CAMX_PROPS_OUTPUT = \
    $(CAMX_PATH)/src/core/g_camxproperties

CAMXPROCESSCONFIGFILETOOL := perl $(CAMX_TOOLS_PATH)/settingsgenerator/settingsgenerator.pl

CAMXVERSIONTOOL := perl $(CAMX_TOOLS_PATH)/version.pl

CAMXPROPSTOOL := perl $(CAMX_TOOLS_PATH)/props.pl

$(info $(shell $(CAMXPROCESSCONFIGFILETOOL) $(CAMX_SETTINGS_INPUTS) $(CAMX_TXTOUT_PATH)/camxoverridesettings.txt))
$(info $(shell $(CAMXVERSIONTOOL) $(CAMX_VERSION_OUTPUT)))
$(info $(shell $(CAMXPROPSTOOL) $(CAMX_PROPS_INPUT) $(CAMX_PROPS_OUTPUT)))

$(CAMX_SETTINGS_OUTPUTS) : $(CAMX_SETTINGS_INPUTS)

$(CAMX_SCOPE_OUTPUTS) : $(CAMX_SCOPE_INPUTS)

$(CAMX_VERSION_OUTPUT) :

endif # CAMX_AUTOGEN_MK_INCLUDED
