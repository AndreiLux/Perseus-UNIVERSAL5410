# Makefile of modem_if

EXTRA_CFLAGS += -Idrivers/misc/modem_v2

obj-y += sipc5_modem.o sipc5_io_device.o
obj-y += sipc4_modem.o sipc4_io_device.o
obj-y += modem_net_flowcontrol_device.o modem_utils.o

obj-$(CONFIG_UMTS_MODEM_XMM6360) += modem_modemctl_device_xmm6262.o

obj-$(CONFIG_LINK_DEVICE_HSIC) += modem_link_device_hsic_ncm.o link_usb_cdc_ncm.o
