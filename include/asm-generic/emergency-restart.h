#ifndef _ASM_GENERIC_EMERGENCY_RESTART_H
#define _ASM_GENERIC_EMERGENCY_RESTART_H

static inline void machine_emergency_restart(void)
{
	/* Keep the boot_mode when device lockup or panic case.*/
	machine_restart("emergency");

	/* machine_restart(NULL); */
}

#endif /* _ASM_GENERIC_EMERGENCY_RESTART_H */
