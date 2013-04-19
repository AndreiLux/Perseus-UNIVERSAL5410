# arch/arm/mach-exynos/Makefile
#
# Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2

obj-y				:=
obj-m				:=
obj-n				:=
obj-				:=

# Core

obj-$(CONFIG_ARCH_EXYNOS)	+= common.o
obj-$(CONFIG_ARCH_EXYNOS4)	+= clock-exynos4.o asv.o asv-4x12.o
obj-$(CONFIG_ARM_TRUSTZONE)	+= irq-sgi.o
obj-$(CONFIG_CPU_EXYNOS4210)	+= clock-exynos4210.o
obj-$(CONFIG_SOC_EXYNOS4212)	+= clock-exynos4212.o
obj-$(CONFIG_SOC_EXYNOS5250)	+= clock-exynos5250.o
obj-$(CONFIG_SOC_EXYNOS5410)	+= clock-exynos5410.o asv-exynos.o asv-exynos5410.o cci.o exynos-power-mode.o
obj-$(CONFIG_SOC_EXYNOS5410)	+= exynos-interface.o

obj-$(CONFIG_EXYNOS5_DEV_BTS)		+= dev-bts.o
obj-$(CONFIG_EXYNOS5410_BTS)		+= bts-exynos5410.o

obj-$(CONFIG_PM)		+= pm.o
obj-$(CONFIG_PM_GENERIC_DOMAINS) += pm_domains.o

ifeq ($(CONFIG_SOC_EXYNOS5250),y)
obj-$(CONFIG_CPU_IDLE)         += cpuidle-exynos5250.o
else
obj-$(CONFIG_CPU_IDLE)         += cpuidle.o
endif

obj-$(CONFIG_SOC_EXYNOS5250)	+= ori-asv-exynos.o ori-abb-exynos.o ori-asv-exynos5250.o

obj-$(CONFIG_ARCH_EXYNOS)	+= pmu.o

obj-$(CONFIG_SMP)		+= platsmp.o headsmp.o

obj-$(CONFIG_EXYNOS4_MCT)	+= mct.o

obj-$(CONFIG_HOTPLUG_CPU)	+= hotplug.o

obj-$(CONFIG_ARCH_EXYNOS)	+= clock-audss.o

obj-$(CONFIG_EXYNOS_FIQ_DEBUGGER)	+= exynos_fiq_debugger.o

obj-$(CONFIG_EXYNOS_BUSFREQ_OPP)     += ppmu.o busfreq_opp_exynos4.o busfreq_opp_4x12.o

obj-$(CONFIG_EXYNOS5_CORESIGHT)		+= coresight-exynos5.o
obj-$(CONFIG_EXYNOS_PERSISTENT_CLOCK)	+= persistent_clock.o

obj-$(CONFIG_EXYNOS5410_DEBUG)		+= debug_exynos5410.o

obj-$(CONFIG_ARM_TRUSTZONE)		+= smc.o
plus_sec := $(call as-instr,.arch_extension sec,+sec)
AFLAGS_smc.o :=-Wa,-march=armv7-a$(plus_sec)

# machine support

obj-$(CONFIG_MACH_SMDKC210)		+= mach-smdkv310.o
obj-$(CONFIG_MACH_SMDKV310)		+= mach-smdkv310.o
obj-$(CONFIG_MACH_ARMLEX4210)		+= mach-armlex4210.o
obj-$(CONFIG_MACH_UNIVERSAL_C210)	+= mach-universal_c210.o
obj-$(CONFIG_MACH_NURI)			+= mach-nuri.o
obj-$(CONFIG_MACH_ORIGEN)		+= mach-origen.o

obj-$(CONFIG_MACH_SMDK4212)		+= mach-smdk4x12.o
obj-$(CONFIG_MACH_SMDK4412)		+= mach-smdk4x12.o
obj-$(CONFIG_MACH_SMDK4412)		+= board-smdk4x12-mmc.o
obj-$(CONFIG_MACH_SMDK4412)		+= board-smdk4x12-audio.o
obj-$(CONFIG_MACH_SMDK4412)		+= board-smdk4x12-display.o
obj-$(CONFIG_MACH_SMDK4412)		+= board-smdk4x12-usb.o
obj-$(CONFIG_MACH_SMDK4412)		+= board-smdk4x12-media.o
obj-$(CONFIG_MACH_SMDK4412)		+= board-smdk4x12-power.o

obj-$(CONFIG_MACH_SMDK5410)		+= mach-smdk5410.o
obj-$(CONFIG_MACH_SMDK5410)		+= board-smdk5410-mmc.o
obj-$(CONFIG_MACH_SMDK5410)		+= board-smdk5410-power.o
obj-$(CONFIG_MACH_SMDK5410)		+= board-smdk5410-usb.o
obj-$(CONFIG_MACH_SMDK5410)		+= board-smdk5410-audio.o
obj-$(CONFIG_MACH_SMDK5410)		+= board-smdk5410-input.o
obj-$(CONFIG_MACH_SMDK5410)		+= board-smdk5410-clock.o
obj-$(CONFIG_MACH_SMDK5410)		+= board-smdk5410-media.o
obj-$(CONFIG_MACH_SMDK5410)		+= board-smdk5410-display.o

obj-$(CONFIG_MACH_UNIVERSAL5410)	+= mach-universal5410.o
obj-$(CONFIG_MACH_UNIVERSAL5410)	+= board-universal5410-mmc.o
obj-$(CONFIG_MACH_UNIVERSAL5410)	+= board-universal5410-thermistor.o
ifeq ($(CONFIG_MACH_HA),y)
obj-$(CONFIG_MACH_HA)	+= board-ha-power.o
else
obj-$(CONFIG_MACH_UNIVERSAL5410)	+= board-universal5410-power.o
endif
ifeq ($(CONFIG_REGULATOR_S2MPS11),y)
obj-$(CONFIG_MACH_JA)	+= board-universal5410-pmic-sec.o
else ifeq ($(CONFIG_REGULATOR_MAX77802),y)
obj-$(CONFIG_MACH_JA)	+= board-universal5410-pmic-max.o
obj-$(CONFIG_MACH_V1)	+= board-v1-pmic-max.o
else
obj-$(CONFIG_MACH_UNIVERSAL5410)	+= board-universal5410-pmic.o
endif
obj-$(CONFIG_MACH_UNIVERSAL5410)	+= board-universal5410-usb.o
ifeq ($(CONFIG_MACH_UNIVERSAL5410),y)
obj-$(CONFIG_MACH_V1)			+= board-viennalte-display.o
obj-$(CONFIG_MACH_JA)			+= board-universal5410-display.o
endif
ifeq ($(CONFIG_MACH_UNIVERSAL5410),y)
ifeq ($(CONFIG_MACH_V1),y)
obj-$(CONFIG_MACH_V1)				+= board-v1-input.o
else
obj-$(CONFIG_MACH_UNIVERSAL5410)	+= board-universal5410-input.o
endif
endif
obj-$(CONFIG_MACH_UNIVERSAL5410)	+= board-universal5410-clock.o
obj-$(CONFIG_MACH_UNIVERSAL5410)	+= board-universal5410-media.o
obj-$(CONFIG_MACH_JA)	+= board-universal5410-led.o
obj-$(CONFIG_MACH_UNIVERSAL5410)	+= board-universal5410-vibrator.o
obj-$(CONFIG_MACH_UNIVERSAL5410)	+= board-universal5410-mfd.o
ifeq ($(CONFIG_TARGET_LOCALE_KOR),y)
obj-$(CONFIG_TARGET_LOCALE_KOR)	+= board-j-kor-gpio.o
else
ifeq ($(CONFIG_MACH_HA),y)
obj-$(CONFIG_MACH_HA)	+= board-ha-gpio.o
else
obj-$(CONFIG_MACH_JA)	+= board-universal5410-gpio.o
obj-$(CONFIG_MACH_V1)	+= board-v1-gpio.o
endif
endif
ifeq ($(CONFIG_MACH_V1),y)
obj-$(CONFIG_BATTERY_SAMSUNG)		+= board-vienna-battery.o
else
obj-$(CONFIG_BATTERY_SAMSUNG)		+= board-universal5410-battery.o
endif
obj-$(CONFIG_SND_SOC_ADONISUNIV_WM1811)	+= board-universal5410-audio-w.o
obj-$(CONFIG_SND_SOC_ADONISUNIV_WM5102)	+= board-universal5410-audio-w.o
obj-$(CONFIG_SND_SOC_ADONISUNIV_YMU831)	+= board-universal5410-audio-y.o
obj-$(CONFIG_SND_SOC_ADONISDUOS_YMU831)	+= board-universal5410-audio-y.o
obj-$(CONFIG_SND_SOC_V1_YMU831)	+= board-v1-audio.o
obj-$(CONFIG_2MIC_AUDIENCE_ES325)	+= board-universal5410-2mic.o
ifeq ($(CONFIG_NFC_DEVICES),y)
obj-$(CONFIG_MACH_UNIVERSAL5410)	+= board-universal5410-nfc.o
endif
ifeq ($(CONFIG_MACH_HA),y)
obj-$(CONFIG_MACH_HA)	+= board-ha-sensor.o
else
obj-$(CONFIG_MACH_UNIVERSAL5410)	+= board-universal5410-sensor.o
endif
obj-$(CONFIG_SAMSUNG_MHL_8240)	+= board-universal5410-mhl.o

ifneq ($(CONFIG_BCM4335),)
obj-$(CONFIG_MACH_UNIVERSAL5410)	+= board-universal5410-wlan.o
obj-$(CONFIG_MACH_UNIVERSAL5410)	+= board-universal5410-bt.o
endif

obj-$(CONFIG_MACH_V1)		+= board-v1-adc.o

obj-$(CONFIG_MACH_UNIVERSAL5410)	+= sec-common.o
obj-$(CONFIG_ICE4_FPGA)	+= board-universal5410-fpga.o
obj-$(CONFIG_IR_REMOCON_MC96)	+= board-v1-irda.o
obj-$(CONFIG_BT_BCM4335)		+= board-bluetooth-bcm4335.o

ifeq ($(CONFIG_EXYNOS_C2C),y)
obj-$(CONFIG_MACH_UNIVERSAL5410)	+= board-universal5410-c2c.o
endif

obj-$(CONFIG_MACH_EXYNOS4_DT)		+= mach-exynos4-dt.o
obj-$(CONFIG_MACH_EXYNOS5_DT)		+= mach-exynos5-dt.o
obj-$(CONFIG_SND_SOC_SAMSUNG)		+= mach-exynos5-audio.o

obj-$(CONFIG_MACH_SMDK5250)		+= mach-smdk5250.o

# device support

obj-y					+= dev-uart.o
obj-$(CONFIG_ARCH_EXYNOS)		+= dev-audio.o
obj-$(CONFIG_EXYNOS4_DEV_AHCI)		+= dev-ahci.o
obj-$(CONFIG_EXYNOS_DEV_DWMCI)		+= dev-dwmci.o
obj-$(CONFIG_EXYNOS4_DEV_FIMC_IS)	+= dev-fimc-is.o
obj-$(CONFIG_EXYNOS5_DEV_FIMC_IS)	+= dev-fimc-is.o
obj-$(CONFIG_EXYNOS4_DEV_FIMC_LITE)	+= dev-fimc-lite.o
obj-$(CONFIG_EXYNOS_DEV_C2C)		+= dev-c2c.o
obj-$(CONFIG_EXYNOS5_DEV_GSC)		+= dev-gsc.o
obj-$(CONFIG_EXYNOS5_DEV_SCALER)	+= dev-scaler.o
obj-$(CONFIG_EXYNOS_DEV_ROTATOR)	+= dev-rotator.o
obj-$(CONFIG_EXYNOS_DEV_SYSMMU)		+= dev-sysmmu.o
obj-$(CONFIG_EXYNOS_DEV_DMA)		+= dma.o
obj-$(CONFIG_EXYNOS_DEV_USB_SWITCH)	+= dev-usb-switch.o
obj-$(CONFIG_EXYNOS4_DEV_USB_OHCI)	+= dev-ohci.o
obj-$(CONFIG_EXYNOS5_DEV_USB3_DRD)	+= dev-usb3-drd.o
obj-$(CONFIG_EXYNOS5_DEV_JPEG)			+= dev-jpeg.o
obj-$(CONFIG_EXYNOS5_DEV_JPEG_HX)			+= dev-jpeg-hx.o
obj-$(CONFIG_EXYNOS_DEV_TMU)		+= dev-tmu.o
obj-$(CONFIG_EXYNOS5_DEV_HSI2C0)	+= dev-hs-i2c0.o
obj-$(CONFIG_EXYNOS5_DEV_HSI2C1)	+= dev-hs-i2c1.o
obj-$(CONFIG_EXYNOS5_DEV_HSI2C2)	+= dev-hs-i2c2.o
obj-$(CONFIG_EXYNOS5_DEV_HSI2C3)	+= dev-hs-i2c3.o

obj-$(CONFIG_ARCH_EXYNOS)		+= setup-i2c0.o
obj-$(CONFIG_EXYNOS4_SETUP_FIMC)	+= setup-fimc.o
obj-$(CONFIG_EXYNOS4_SETUP_CSIS)	+= setup-csis.o
obj-$(CONFIG_EXYNOS5_SETUP_GSC)		+= setup-gsc.o
obj-$(CONFIG_EXYNOS4_SETUP_FIMD0)	+= setup-fimd0.o
obj-$(CONFIG_EXYNOS_SETUP_FIMD1)	+= setup-fimd1.o
obj-$(CONFIG_EXYNOS_SETUP_DP)		+= setup-dp.o
obj-$(CONFIG_FB_MIPI_DSIM)		+= setup-mipidsim.o
obj-$(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION) += secmem.o
obj-$(CONFIG_EXYNOS4_SETUP_FIMC_IS)	+= setup-fimc-is.o
obj-$(CONFIG_EXYNOS5_SETUP_FIMC_IS)	+= setup-fimc-is.o
obj-$(CONFIG_VISION_MODE)		+= setup-fimc-is-sensor.o
obj-$(CONFIG_EXYNOS4_SETUP_I2C1)	+= setup-i2c1.o
obj-$(CONFIG_EXYNOS4_SETUP_I2C2)	+= setup-i2c2.o
obj-$(CONFIG_EXYNOS4_SETUP_I2C3)	+= setup-i2c3.o
obj-$(CONFIG_EXYNOS4_SETUP_I2C4)	+= setup-i2c4.o
obj-$(CONFIG_EXYNOS4_SETUP_I2C5)	+= setup-i2c5.o
obj-$(CONFIG_EXYNOS4_SETUP_I2C6)	+= setup-i2c6.o
obj-$(CONFIG_EXYNOS4_SETUP_I2C7)	+= setup-i2c7.o
obj-$(CONFIG_EXYNOS5_SETUP_HSI2C0)	+= setup-hs-i2c0.o
obj-$(CONFIG_EXYNOS5_SETUP_HSI2C1)	+= setup-hs-i2c1.o
obj-$(CONFIG_EXYNOS5_SETUP_HSI2C2)	+= setup-hs-i2c2.o
obj-$(CONFIG_EXYNOS5_SETUP_HSI2C3)	+= setup-hs-i2c3.o
obj-$(CONFIG_EXYNOS4_SETUP_KEYPAD)	+= setup-keypad.o
obj-$(CONFIG_EXYNOS4_SETUP_MFC)		+= setup-mfc.o
obj-$(CONFIG_EXYNOS4_SETUP_SDHCI_GPIO)	+= setup-sdhci-gpio.o
obj-$(CONFIG_EXYNOS4_SETUP_USB_PHY)	+= setup-usb-phy.o
obj-$(CONFIG_EXYNOS_SETUP_SPI)		+= setup-spi.o
obj-$(CONFIG_EXYNOS5_SETUP_TVOUT)	+= setup-tvout.o
obj-$(CONFIG_EXYNOS_SETUP_ADC)		+= setup-adc.o
obj-$(CONFIG_ION_EXYNOS)		+= dev-ion.o
obj-$(CONFIG_CMA)			+= reserve-mem.o
obj-$(CONFIG_EXYNOS5_SETUP_JPEG)			+= setup-jpeg.o
obj-$(CONFIG_EXYNOS5_SETUP_JPEG_HX)			+= setup-jpeg-hx.o
obj-$(CONFIG_ARCH_EXYNOS5)		+= resetreason.o
obj-$(CONFIG_EXYNOS_TMU)		+= tmu-exynos.o
obj-$(CONFIG_MACH_JA)	+= sec-switch.o
obj-$(CONFIG_MFD_MAX77693)	+= sec-switch-max77693.o
obj-$(CONFIG_30PIN_CONN)	+= sec-switch-v1.o

obj-$(CONFIG_BL_SWITCHER)		+= bL_control.o bL_setup.o

obj-$(CONFIG_MDM_HSIC_PM)               += mdm_hsic_pm.o
obj-$(CONFIG_QC_MODEM_MDM9X15)		+= mdm2.o mdm_common.o mdm_device.o
obj-$(CONFIG_SEC_MODEM_M0_XMM6360)	+= board-m0-xmm6360-modems.o
obj-$(CONFIG_SEC_MODEM_J_SKT)		+= board-j-skt-modems.o
obj-$(CONFIG_SEC_MODEM_J_KTT)		+= board-j-skt-modems.o
obj-$(CONFIG_SEC_MODEM_J_CTC)		+= board-j-ctc-modems.o
obj-$(CONFIG_SEC_MODEM_J_CU)		+= board-j-cu-modems.o
obj-$(CONFIG_MSM_SUBSYSTEM_RESTART)     += subsystem_restart.o
obj-$(CONFIG_SEC_DEBUG)			+= sec_debug.o sec_getlog.o sec_gaf.o
obj-$(CONFIG_SEC_WATCHDOG_RESET)	+= sec_watchdog.o
obj-$(CONFIG_SEC_LOG)			+= sec_log.o
obj-$(CONFIG_CORESIGHT_ETM) += coresight.o coresight-etb.o coresight-tpiu.o coresight-funnel.o coresight-etm.o board-universal5410-coresight.o
obj-$(CONFIG_SEC_PM)			+= sec_gpio.o
obj-$(CONFIG_MACH_UNIVERSAL5410)	+= sec-reboot.o
obj-$(CONFIG_SEC_THERMISTOR)		+= sec_thermistor.o
obj-$(CONFIG_MACH_V1)	+= board-v1-motor.o

ifeq ($(CONFIG_GPS_BCMxxxxx),y)
obj-$(CONFIG_GPS_BCM47511)		+= sec_gps_bcm47511.o
obj-$(CONFIG_GPS_BCM4752)		+= sec_gps_bcm4752.o
obj-$(CONFIG_GPS_BCM47521)		+= sec_gps_bcm47521.o
endif

ifeq ($(CONFIG_GPS_CSRxT),y)
obj-$(CONFIG_GPS_GSD4T)			+= sec_gps_gsd4t.o
obj-$(CONFIG_GPS_GSD5T)			+= sec_gps_gsd5t.o
endif
