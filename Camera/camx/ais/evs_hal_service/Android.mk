LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# Include the sub-makefiles
include $(call all-makefiles-under,$(LOCAL_PATH))
