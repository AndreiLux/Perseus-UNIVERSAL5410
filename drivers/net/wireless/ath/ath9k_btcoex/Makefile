ath9k_btcoex-y += \
		beacon.o \
		gpio.o \
		init.o \
		main.o \
		recv.o \
		xmit.o \
		antenna.o

ath9k_btcoex-$(CONFIG_ATH9K_BTCOEX_SUPPORT) += mci.o
ath9k_btcoex-$(CONFIG_ATH9K_RATE_CONTROL) += rc.o
ath9k_btcoex-$(CONFIG_ATH9K_PCI) += pci.o
ath9k_btcoex-$(CONFIG_ATH9K_AHB) += ahb.o
ath9k_btcoex-$(CONFIG_ATH9K_DEBUGFS) += debug.o
ath9k_btcoex-$(CONFIG_ATH9K_DFS_DEBUGFS) += dfs_debug.o
ath9k_btcoex-$(CONFIG_ATH9K_DFS_CERTIFIED) += dfs.o

obj-$(CONFIG_ATH9K_BTCOEX) += ath9k_btcoex.o

ath9k_hw_btcoex-y:=	\
		ar9002_hw.o \
		ar9003_hw.o \
		hw.o \
		ar9003_phy.o \
		ar9002_phy.o \
		ar5008_phy.o \
		ar9002_calib.o \
		ar9003_calib.o \
		ar9003_rtt.o \
		calib.o \
		eeprom.o \
		eeprom_def.o \
		eeprom_4k.o \
		eeprom_9287.o \
		ani.o \
		mac.o \
		ar9002_mac.o \
		ar9003_mac.o \
		ar9003_eeprom.o \
		ar9003_paprd.o

ath9k_hw_btcoex-$(CONFIG_ATH9K_BTCOEX_SUPPORT) += btcoex.o \
					   ar9003_mci.o
obj-$(CONFIG_ATH9K_BTCOEX_HW) += ath9k_hw_btcoex.o

obj-$(CONFIG_ATH9K_BTCOEX_COMMON) += ath9k_common_btcoex.o
ath9k_common_btcoex-y:=	common.o

