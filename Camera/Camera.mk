#camera service
PRODUCT_COPY_FILES += \
	device/continental/WATT2/Camera/firmware/firmware_ispov490.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/camera/firmware_ispov490.bin

#AIS conti impl
PRODUCT_PACKAGES += libais_ov490
PRODUCT_PACKAGES += libcameratest

