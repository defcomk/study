ifeq ($(TARGET_ARCH),$(filter $(TARGET_ARCH),arm arm64))
  #include $(call all-subdir-makefiles)
  MY_PATH := $(call my-dir)
  include $(MY_PATH)/ov490_lib/Android.mk
endif
