   zreladdr-y	+= 0x00008000
params_phys-y	:= 0x00000100
initrd_phys-y	:= 0x00800000

dtb-$(CONFIG_MACH_VERSATILE_DT) += versatile-pb.dtb
dtb-$(CONFIG_MACH_VERSATILE_DT) += versatile-ab.dtb
