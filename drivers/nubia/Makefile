#
# Makefile for the nubia drivers.
#
obj-$(CONFIG_NUBIA_HW_VERSION)  += nubia_hw_version/
obj-$(CONFIG_NUBIA_VENDOR_HOOK)  += nubia_vendor_hook/
obj-$(CONFIG_NUBIA_SENSORS_SENS)  += sensors_sensitivity/
obj-$(CONFIG_TOUCHSCREEN_GOODIX_BRL_SPI) += goodix_berlin_driver/
obj-$(CONFIG_NUBIA_FINGERPRINT)  += fingerprint/
obj-$(CONFIG_TOUCHSCREEN_GOODIX_BRL_SPI) += aw96205/
obj-$(CONFIG_NUBIA_KEYBOARD_GAMESWITCH)		+= nubia_gpio_keys/
obj-$(CONFIG_AWINIC_HAPTIC_HV_DOUBLE)		+= aw86927_double/
#nubia led start
obj-$(CONFIG_NUBIA_LED)			+= nubia_led/
#nubia led end
obj-m                                            += nblog/
obj-$(CONFIG_SOC_FAN) += fan/
obj-$(CONFIG_NFC_S3NSN4V) += samsung/
obj-$(CONFIG_NUBIA_BOOST_TASK) += nb_boost/
