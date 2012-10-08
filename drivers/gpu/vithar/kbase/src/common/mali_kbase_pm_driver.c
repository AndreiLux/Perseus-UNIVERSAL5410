/*
 *
 * (C) COPYRIGHT 2010-2012 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 */



/**
 * @file mali_kbase_pm_driver.c
 * Base kernel Power Management hardware control
 */

#include <osk/mali_osk.h>
#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_midg_regmap.h>

#include <kbase/src/common/mali_kbase_pm.h>

#if MALI_MOCK_TEST
#define MOCKABLE(function) function##_original
#else
#define MOCKABLE(function) function
#endif /* MALI_MOCK_TEST */

/** Number of milliseconds before we time out on a reset */
#define RESET_TIMEOUT   500

/** Actions that can be performed on a core.
 *
 * This enumeration is private to the file. Its values are set to allow @ref core_type_to_reg function,
 * which decodes this enumeration, to be simpler and more efficient.
 */
typedef enum kbasep_pm_action
{
	ACTION_PRESENT      = 0,
	ACTION_READY        = (SHADER_READY_LO - SHADER_PRESENT_LO),
	ACTION_PWRON        = (SHADER_PWRON_LO - SHADER_PRESENT_LO),
	ACTION_PWROFF       = (SHADER_PWROFF_LO - SHADER_PRESENT_LO),
	ACTION_PWRTRANS     = (SHADER_PWRTRANS_LO - SHADER_PRESENT_LO),
	ACTION_PWRACTIVE    = (SHADER_PWRACTIVE_LO - SHADER_PRESENT_LO)
} kbasep_pm_action;

/** Decode a core type and action to a register.
 *
 * Given a core type (defined by @ref kbase_pm_core_type) and an action (defined by @ref kbasep_pm_action) this 
 * function will return the register offset that will perform the action on the core type. The register returned is 
 * the \c _LO register and an offset must be applied to use the \c _HI register.
 * 
 * @param core_type The type of core
 * @param action    The type of action
 *
 * @return The register offset of the \c _LO register that performs an action of type \c action on a core of type \c 
 * core_type.
 */
static u32 core_type_to_reg(kbase_pm_core_type core_type, kbasep_pm_action action)
{
	return core_type + action;
}

/** Invokes an action on a core set
 *
 * This function performs the action given by \c action on a set of cores of a type given by \c core_type. It is a 
 * static function used by @ref kbase_pm_invoke_power_up and @ref kbase_pm_invoke_power_down.
 *
 * @param kbdev     The kbase device structure of the device
 * @param core_type The type of core that the action should be performed on
 * @param cores     A bit mask of cores to perform the action on (low 32 bits)
 * @param action    The action to perform on the cores
 */
STATIC void kbase_pm_invoke(kbase_device *kbdev, kbase_pm_core_type core_type, u64 cores, kbasep_pm_action action)
{
	u32 reg;
	u32 lo = cores & 0xFFFFFFFF;
	u32 hi = (cores >> 32) & 0xFFFFFFFF;

	reg = core_type_to_reg(core_type, action);

	OSK_ASSERT(reg);

	/* Tracing */
	if ( cores != 0 && core_type == KBASE_PM_CORE_SHADER )
	{
		if (action == ACTION_PWRON )
		{
			KBASE_TRACE_ADD( kbdev, PM_PWRON, NULL, NULL, 0u, lo );
		}
		else if ( action == ACTION_PWROFF )
		{
			KBASE_TRACE_ADD( kbdev, PM_PWROFF, NULL, NULL, 0u, lo );
		}
	}

	if (lo != 0)
	{
		kbase_reg_write(kbdev, GPU_CONTROL_REG(reg), lo, NULL);
	}
	if (hi != 0)
	{
		kbase_reg_write(kbdev, GPU_CONTROL_REG(reg+4), hi, NULL);
	}
}

void kbase_pm_invoke_power_up(kbase_device *kbdev, kbase_pm_core_type type, u64 cores)
{
	OSK_ASSERT( kbdev != NULL );

	switch(type)
	{
		case KBASE_PM_CORE_SHADER:
			{
				u64 prev_desired_shader = kbdev->pm.desired_shader_state;
				kbdev->pm.desired_shader_state |= cores;
				if ( prev_desired_shader != kbdev->pm.desired_shader_state )
				{
					KBASE_TRACE_ADD( kbdev, PM_CORES_CHANGE_DESIRED_ON_POWERUP, NULL, NULL, 0u, (u32)kbdev->pm.desired_shader_state );
				}
			}
			break;
		case KBASE_PM_CORE_TILER:
			kbdev->pm.desired_tiler_state |= cores;
			break;
		default:
			OSK_ASSERT(0);
	}
}
KBASE_EXPORT_TEST_API(kbase_pm_invoke_power_up)

void kbase_pm_invoke_power_down(kbase_device *kbdev, kbase_pm_core_type type, u64 cores)
{
	OSK_ASSERT( kbdev != NULL );

	switch(type)
	{
		case KBASE_PM_CORE_SHADER:
			{
				u64 prev_desired_shader = kbdev->pm.desired_shader_state;
				kbdev->pm.desired_shader_state &= ~cores;
				if ( prev_desired_shader != kbdev->pm.desired_shader_state )
				{
					KBASE_TRACE_ADD( kbdev, PM_CORES_CHANGE_DESIRED_ON_POWERDOWN, NULL, NULL, 0u, (u32)kbdev->pm.desired_shader_state );
				}
			}
			break;
		case KBASE_PM_CORE_TILER:
			kbdev->pm.desired_tiler_state &= ~cores;
			break;
		default:
			OSK_ASSERT(0);
	}
}
KBASE_EXPORT_TEST_API(kbase_pm_invoke_power_down)
/** Get information about a core set
 *
 * This function gets information (chosen by \c action) about a set of cores of a type given by \c core_type. It is a 
 * static function used by @ref kbase_pm_get_present_cores, @ref kbase_pm_get_active_cores, @ref 
 * kbase_pm_get_trans_cores and @ref kbase_pm_get_ready_cores.
 *
 * @param kbdev     The kbase device structure of the device
 * @param core_type The type of core that the should be queried
 * @param action    The property of the cores to query
 *
 * @return A bit mask specifying the state of the cores
 */
static u64 kbase_pm_get_state(kbase_device *kbdev, kbase_pm_core_type core_type, kbasep_pm_action action)
{
	u32 reg;
	u32 lo, hi;

	reg = core_type_to_reg(core_type, action);

	OSK_ASSERT(reg);

	lo = kbase_reg_read(kbdev, GPU_CONTROL_REG(reg), NULL);
	hi = kbase_reg_read(kbdev, GPU_CONTROL_REG(reg+4), NULL);

	return (((u64)hi) << 32) | ((u64)lo);
}

void kbasep_pm_read_present_cores(kbase_device *kbdev)
{
	kbdev->shader_present_bitmap = kbase_pm_get_state(kbdev, KBASE_PM_CORE_SHADER, ACTION_PRESENT);
	kbdev->tiler_present_bitmap = kbase_pm_get_state(kbdev, KBASE_PM_CORE_TILER, ACTION_PRESENT);
	kbdev->l2_present_bitmap = kbase_pm_get_state(kbdev, KBASE_PM_CORE_L2, ACTION_PRESENT);
	kbdev->l3_present_bitmap = kbase_pm_get_state(kbdev, KBASE_PM_CORE_L3, ACTION_PRESENT);

	kbdev->shader_inuse_bitmap = 0;
	kbdev->tiler_inuse_bitmap = 0;
	kbdev->shader_needed_bitmap = 0;
	kbdev->tiler_needed_bitmap = 0;
	kbdev->shader_available_bitmap = 0;
	kbdev->tiler_available_bitmap = 0;

	OSK_MEMSET(kbdev->shader_needed_cnt, 0, sizeof(kbdev->shader_needed_cnt));
	OSK_MEMSET(kbdev->shader_needed_cnt, 0, sizeof(kbdev->tiler_needed_cnt));
}
KBASE_EXPORT_TEST_API(kbasep_pm_read_present_cores)

/** Get the cores that are present
 */
u64 kbase_pm_get_present_cores(kbase_device *kbdev, kbase_pm_core_type type)
{
	OSK_ASSERT( kbdev != NULL );

	switch(type) {
		case KBASE_PM_CORE_L3:
			return kbdev->l3_present_bitmap;
			break;
		case KBASE_PM_CORE_L2:
			return kbdev->l2_present_bitmap;
			break;
		case KBASE_PM_CORE_SHADER:
			return kbdev->shader_present_bitmap;
			break;
		case KBASE_PM_CORE_TILER:
			return kbdev->tiler_present_bitmap;
			break;
	}
	OSK_ASSERT(0);
	return 0;
}
KBASE_EXPORT_TEST_API(kbase_pm_get_present_cores)

/** Get the cores that are "active" (busy processing work)
 */
u64 kbase_pm_get_active_cores(kbase_device *kbdev, kbase_pm_core_type type)
{
	return kbase_pm_get_state(kbdev, type, ACTION_PWRACTIVE);
}
KBASE_EXPORT_TEST_API(kbase_pm_get_active_cores)

/** Get the cores that are transitioning between power states
 */
u64 MOCKABLE(kbase_pm_get_trans_cores)(kbase_device *kbdev, kbase_pm_core_type type)
{
	return kbase_pm_get_state(kbdev, type, ACTION_PWRTRANS);
}
KBASE_EXPORT_TEST_API(kbase_pm_get_trans_cores)
/** Get the cores that are powered on
 */
u64 kbase_pm_get_ready_cores(kbase_device *kbdev, kbase_pm_core_type type)
{
	u64 result;
	result = kbase_pm_get_state(kbdev, type, ACTION_READY);

	if ( type == KBASE_PM_CORE_SHADER )
	{
		KBASE_TRACE_ADD( kbdev, PM_CORES_POWERED, NULL, NULL, 0u, (u32)result);
	}
	return result;
}
KBASE_EXPORT_TEST_API(kbase_pm_get_ready_cores)

/** Is there an active power transition?
 *
 * Returns true if there is a power transition in progress, otherwise false.
 */
mali_bool MOCKABLE(kbase_pm_get_pwr_active)(kbase_device *kbdev)
{
	return ((kbase_reg_read(kbdev, GPU_CONTROL_REG(GPU_STATUS), NULL) & (1<<1)) != 0);
}
KBASE_EXPORT_TEST_API(kbase_pm_get_pwr_active)

/** Perform power transitions for a particular core type.
 *
 * This function will perform any available power transitions to make the actual hardware state closer to the desired
 * state. If a core is currently transitioning then changes to the power state of that call cannot be made until the
 * transition has finished. Cores which are not present in the hardware are ignored if they are specified in the
 * desired_state bitmask, however the return value will always be 0 in this case.
 *
 * @param kbdev             The kbase device
 * @param type              The core type to perform transitions for
 * @param desired_state     A bit mask of the desired state of the cores
 * @param in_use            A bit mask of the cores that are currently running jobs.
 *                          These cores have to be kept powered up because there are jobs
 *                          running (or about to run) on them.
 * @param[out] available    Receives a bit mask of the cores that the job scheduler can use to submit jobs to.
 *                          May be NULL if this is not needed.
 *
 * @return MALI_TRUE if the desired state has been reached, MALI_FALSE otherwise
 */

STATIC mali_bool kbase_pm_transition_core_type(kbase_device *kbdev, kbase_pm_core_type type, u64 desired_state,
                                         u64 in_use, u64 *available)
{
	u64 present;
	u64 ready;
	u64 trans;
	u64 powerup;
	u64 powerdown;

	/* Get current state */
	present = kbase_pm_get_present_cores(kbdev, type);
	trans = kbase_pm_get_trans_cores(kbdev, type);
	ready = kbase_pm_get_ready_cores(kbdev, type);

	if (available != NULL)
	{
		*available = ready & desired_state;
	}
	
	/* Update desired state to include the in-use cores. These have to be kept powered up because there are jobs 
	 * running or about to run on these cores
	 */
	desired_state |= in_use;

	/* Workaround for MIDBASE-1258 (L2 usage should be refcounted).
	 * Keep the L2 from being turned off.
	 */
	if (type == KBASE_PM_CORE_L2)
	{
		desired_state = present;
	}

	if (desired_state == ready && trans == 0)
	{
		return MALI_TRUE;
	}

	/* Restrict the cores to those that are actually present */
	powerup = desired_state & present;
	powerdown = (~desired_state) & present;

	/* Restrict to cores that are not already in the desired state */
	powerup &= ~ready;
	powerdown &= ready;

	/* Don't transition any cores that are already transitioning */
	powerup &= ~trans;
	powerdown &= ~trans;

	/* Perform transitions if any */
	kbase_pm_invoke(kbdev, type, powerup, ACTION_PWRON);
	kbase_pm_invoke(kbdev, type, powerdown, ACTION_PWROFF);

	return MALI_FALSE;
}
KBASE_EXPORT_TEST_API(kbase_pm_transition_core_type)

/** Determine which caches should be on for a particular core state.
 *
 * This function takes a bit mask of the present caches and the cores (or caches) that are attached to the caches that
 * will be powered. It then computes which caches should be turned on to allow the cores requested to be powered up.
 *
 * @param present       The bit mask of present caches
 * @param cores_powered A bit mask of cores (or L2 caches) that are desired to be powered
 *
 * @return A bit mask of the caches that should be turned on
 */
STATIC u64 get_desired_cache_status(u64 present, u64 cores_powered)
{
	u64 desired = 0;

	while (present)
	{
		/* Find out which is the highest set bit */
		u64 bit = 63-osk_clz_64(present);
		u64 bit_mask = 1ull << bit;
		/* Create a mask which has all bits from 'bit' upwards set */

		u64 mask = ~(bit_mask-1);

		/* If there are any cores powered at this bit or above (that haven't previously been processed) then we need
		 * this core on */
		if (cores_powered & mask)
		{
			desired |= bit_mask;
		}

		/* Remove bits from cores_powered and present */
		cores_powered &= ~mask;
		present &= ~bit_mask;
	}
	
	return desired;
}
KBASE_EXPORT_TEST_API(get_desired_cache_status)
static mali_bool kbasep_pm_unrequest_cores_nolock(kbase_device *kbdev, u64 shader_cores, u64 tiler_cores)
{
	mali_bool change_gpu_state = MALI_FALSE;


	OSK_ASSERT( kbdev != NULL );

	while (shader_cores)
	{
		int bitnum = 63 - osk_clz_64(shader_cores);
		u64 bit = 1ULL << bitnum;
		int cnt;

		OSK_ASSERT(kbdev->shader_needed_cnt[bitnum] > 0);

		cnt = --kbdev->shader_needed_cnt[bitnum];

		if (0 == cnt)
		{
			kbdev->shader_needed_bitmap &= ~bit;
			change_gpu_state = MALI_TRUE;
		}

		shader_cores &= ~bit;
	}

	if ( change_gpu_state )
	{
		KBASE_TRACE_ADD( kbdev, PM_UNREQUEST_CHANGE_SHADER_NEEDED, NULL, NULL, 0u, (u32)kbdev->shader_needed_bitmap );
	}

	while (tiler_cores)
	{
		int bitnum = 63 - osk_clz_64(tiler_cores);
		u64 bit = 1ULL << bitnum;
		int cnt;

		OSK_ASSERT(kbdev->tiler_needed_cnt[bitnum] > 0);

		cnt = --kbdev->tiler_needed_cnt[bitnum];

		if (0 == cnt)
		{
			kbdev->tiler_needed_bitmap &= ~bit;
			change_gpu_state = MALI_TRUE;
		}

		tiler_cores &= ~bit;
	}

	return change_gpu_state;
}

mali_error kbase_pm_request_cores(kbase_device *kbdev, u64 shader_cores, u64 tiler_cores)
{
	u64 cores;

	mali_bool change_gpu_state = MALI_FALSE;

	OSK_ASSERT( kbdev != NULL );

	osk_spinlock_irq_lock(&kbdev->pm.power_change_lock);

	cores = shader_cores;
	while (cores)
	{
		int bitnum = 63 - osk_clz_64(cores);
		u64 bit = 1ULL << bitnum;

		int cnt = ++kbdev->shader_needed_cnt[bitnum];

		if (0 == cnt)
		{
			/* Wrapped, undo everything we've done so far */

			kbdev->shader_needed_cnt[bitnum]--;
			kbasep_pm_unrequest_cores_nolock(kbdev, cores ^ shader_cores, 0);

			osk_spinlock_irq_unlock(&kbdev->pm.power_change_lock);
			return MALI_ERROR_FUNCTION_FAILED;
		}

		if (1 == cnt)
		{
			kbdev->shader_needed_bitmap |= bit;
			change_gpu_state = MALI_TRUE;
		}

		cores &= ~bit;
	}


	if ( change_gpu_state )
	{
		KBASE_TRACE_ADD( kbdev, PM_REQUEST_CHANGE_SHADER_NEEDED, NULL, NULL, 0u, (u32)kbdev->shader_needed_bitmap );
	}

	cores = tiler_cores;
	while (cores)
	{
		int bitnum = 63 - osk_clz_64(cores);
		u64 bit = 1ULL << bitnum;

		int cnt = ++kbdev->tiler_needed_cnt[bitnum];

		if (0 == cnt)
		{
			/* Wrapped, undo everything we've done so far */
			
			kbdev->tiler_needed_cnt[bitnum]--;
			kbasep_pm_unrequest_cores_nolock(kbdev, shader_cores, cores ^ tiler_cores);

			osk_spinlock_irq_unlock(&kbdev->pm.power_change_lock);
			return MALI_ERROR_FUNCTION_FAILED;
		}

		if (1 == cnt)
		{
			kbdev->tiler_needed_bitmap |= bit;
			change_gpu_state = MALI_TRUE;
		}

		cores &= ~bit;
	}

	if (change_gpu_state)
	{
		kbase_pm_send_event(kbdev, KBASE_PM_EVENT_CHANGE_GPU_STATE);
	}

	osk_spinlock_irq_unlock(&kbdev->pm.power_change_lock);

	return MALI_ERROR_NONE;
}
KBASE_EXPORT_TEST_API(kbase_pm_request_cores)

void kbase_pm_unrequest_cores(kbase_device *kbdev, u64 shader_cores, u64 tiler_cores)
{
	mali_bool change_gpu_state = MALI_FALSE;


	OSK_ASSERT( kbdev != NULL );

	osk_spinlock_irq_lock(&kbdev->pm.power_change_lock);

	change_gpu_state = kbasep_pm_unrequest_cores_nolock(kbdev, shader_cores, tiler_cores);

	if (change_gpu_state)
	{
		kbase_pm_send_event(kbdev, KBASE_PM_EVENT_CHANGE_GPU_STATE);
	}

	osk_spinlock_irq_unlock(&kbdev->pm.power_change_lock);
}
KBASE_EXPORT_TEST_API(kbase_pm_unrequest_cores)

mali_bool kbase_pm_register_inuse_cores(kbase_device *kbdev, u64 shader_cores, u64 tiler_cores)
{
	u64 prev_shader_needed; /* Just for tracing */
	u64 prev_shader_inuse; /* Just for tracing */

	osk_spinlock_irq_lock(&kbdev->pm.power_change_lock);

	prev_shader_needed = kbdev->shader_needed_bitmap;
	prev_shader_inuse = kbdev->shader_inuse_bitmap;

	if ((kbdev->shader_available_bitmap & shader_cores) != shader_cores ||
	    (kbdev->tiler_available_bitmap & tiler_cores) != tiler_cores)
	{
		osk_spinlock_irq_unlock(&kbdev->pm.power_change_lock);
		return MALI_FALSE;
	}

	while (shader_cores)
	{
		int bitnum = 63 - osk_clz_64(shader_cores);
		u64 bit = 1ULL << bitnum;
		int cnt;

		OSK_ASSERT(kbdev->shader_needed_cnt[bitnum] > 0);

		cnt = --kbdev->shader_needed_cnt[bitnum];

		if (0 == cnt)
		{
			kbdev->shader_needed_bitmap &= ~bit;
		}

		/* shader_inuse_cnt should not overflow because there can only be a
		 * very limited number of jobs on the h/w at one time */

		kbdev->shader_inuse_cnt[bitnum]++;
		kbdev->shader_inuse_bitmap |= bit;

		shader_cores &= ~bit;
	}

	if ( prev_shader_needed != kbdev->shader_needed_bitmap )
	{
		KBASE_TRACE_ADD( kbdev, PM_REGISTER_CHANGE_SHADER_NEEDED, NULL, NULL, 0u, (u32)kbdev->shader_needed_bitmap );
	}
	if ( prev_shader_inuse != kbdev->shader_inuse_bitmap )
	{
		KBASE_TRACE_ADD( kbdev, PM_REGISTER_CHANGE_SHADER_INUSE, NULL, NULL, 0u, (u32)kbdev->shader_inuse_bitmap );
	}

	while (tiler_cores)
	{
		int bitnum = 63 - osk_clz_64(tiler_cores);
		u64 bit = 1ULL << bitnum;
		int cnt;

		OSK_ASSERT(kbdev->tiler_needed_cnt[bitnum] > 0);

		cnt = --kbdev->tiler_needed_cnt[bitnum];

		if (0 == cnt)
		{
			kbdev->tiler_needed_bitmap &= ~bit;
		}

		/* tiler_inuse_cnt should not overflow because there can only be a
		 * very limited number of jobs on the h/w at one time */

		kbdev->tiler_inuse_cnt[bitnum]++;
		kbdev->tiler_inuse_bitmap |= bit;

		tiler_cores &= ~bit;
	}

	osk_spinlock_irq_unlock(&kbdev->pm.power_change_lock);

	return MALI_TRUE;
}
KBASE_EXPORT_TEST_API(kbase_pm_register_inuse_cores)

void kbase_pm_release_cores(kbase_device *kbdev, u64 shader_cores, u64 tiler_cores)
{
	mali_bool change_gpu_state = MALI_FALSE;

	OSK_ASSERT( kbdev != NULL );

	osk_spinlock_irq_lock(&kbdev->pm.power_change_lock);

	while (shader_cores)
	{
		int bitnum = 63 - osk_clz_64(shader_cores);
		u64 bit = 1ULL << bitnum;
		int cnt;

		OSK_ASSERT(kbdev->shader_inuse_cnt[bitnum] > 0);

		cnt = --kbdev->shader_inuse_cnt[bitnum];

		if (0 == cnt)
		{
			kbdev->shader_inuse_bitmap &= ~bit;
			change_gpu_state = MALI_TRUE;
		}

		shader_cores &= ~bit;
	}

	if ( change_gpu_state )
	{
		KBASE_TRACE_ADD( kbdev, PM_RELEASE_CHANGE_SHADER_INUSE, NULL, NULL, 0u, (u32)kbdev->shader_inuse_bitmap  );
	}

	while (tiler_cores)
	{
		int bitnum = 63 - osk_clz_64(tiler_cores);
		u64 bit = 1ULL << bitnum;
		int cnt;

		OSK_ASSERT(kbdev->tiler_inuse_cnt[bitnum] > 0);

		cnt = --kbdev->tiler_inuse_cnt[bitnum];

		if (0 == cnt)
		{
			kbdev->tiler_inuse_bitmap &= ~bit;
			change_gpu_state = MALI_TRUE;
		}

		tiler_cores &= ~bit;
	}
	
	if (change_gpu_state)
	{
		kbase_pm_send_event(kbdev, KBASE_PM_EVENT_CHANGE_GPU_STATE);
	}

	osk_spinlock_irq_unlock(&kbdev->pm.power_change_lock);
}
KBASE_EXPORT_TEST_API(kbase_pm_release_cores)

void MOCKABLE(kbase_pm_check_transitions)(kbase_device *kbdev)
{
	mali_bool in_desired_state = MALI_TRUE;
	u64 desired_l2_state;
	u64 desired_l3_state;
	u64 cores_powered;
	u64 tiler_available_bitmap;
	u64 shader_available_bitmap;

	OSK_ASSERT( NULL != kbdev );

	osk_spinlock_irq_lock(&kbdev->pm.power_change_lock);

	cores_powered = (kbdev->pm.desired_shader_state | kbdev->pm.desired_tiler_state);

	/* We need to keep the inuse cores powered */
	cores_powered |= kbdev->shader_inuse_bitmap | kbdev->tiler_inuse_bitmap;

	desired_l2_state = get_desired_cache_status(kbdev->l2_present_bitmap, cores_powered);
	desired_l3_state = get_desired_cache_status(kbdev->l3_present_bitmap, desired_l2_state);

	in_desired_state &= kbase_pm_transition_core_type(kbdev, KBASE_PM_CORE_L3, desired_l3_state, 0, NULL);
	in_desired_state &= kbase_pm_transition_core_type(kbdev, KBASE_PM_CORE_L2, desired_l2_state, 0, NULL);

	if (in_desired_state)
	{
		in_desired_state &= kbase_pm_transition_core_type(kbdev, KBASE_PM_CORE_TILER,
		                                                  kbdev->pm.desired_tiler_state, kbdev->tiler_inuse_bitmap,
		                                                  &tiler_available_bitmap);
		in_desired_state &= kbase_pm_transition_core_type(kbdev, KBASE_PM_CORE_SHADER,
		                                                  kbdev->pm.desired_shader_state, kbdev->shader_inuse_bitmap,
		                                                  &shader_available_bitmap);

		/* If we reached the desired state, or we powered off a core, then update the available
		 * This is because:
		 * - powering down happens immediately, so we must make the cores unavailable immediately
		 * - powering up may not bring all cores up together at once, so we must wait until we
		 * reach the desired state before making the cores available */
		if ( in_desired_state )
		{
			if ( kbdev->shader_available_bitmap != shader_available_bitmap )
			{
				KBASE_TRACE_ADD( kbdev, PM_CORES_CHANGE_AVAILABLE, NULL, NULL, 0u, (u32)shader_available_bitmap );
			}
			kbdev->shader_available_bitmap = shader_available_bitmap;
			kbdev->tiler_available_bitmap = tiler_available_bitmap;
		}
		else
		{
			/* Calculate the cores that were previously available and are still available now (i.e.
			 * take account of cores that powered down, but ignore those that powered up) */
			u64 remaining_shader_available = kbdev->shader_available_bitmap & shader_available_bitmap;
			u64 remaining_tiler_available = kbdev->tiler_available_bitmap & tiler_available_bitmap;
			if ( kbdev->shader_available_bitmap != remaining_shader_available )
			{
				KBASE_TRACE_ADD( kbdev, PM_CORES_CHANGE_AVAILABLE, NULL, NULL, 0u, (u32)remaining_shader_available );
			}
			kbdev->shader_available_bitmap = remaining_shader_available;
			kbdev->tiler_available_bitmap = remaining_tiler_available;
		}
	}

	if (in_desired_state)
	{
		kbase_pm_send_event(kbdev, KBASE_PM_EVENT_GPU_STATE_CHANGED);
	}

	osk_spinlock_irq_unlock(&kbdev->pm.power_change_lock);
}
KBASE_EXPORT_TEST_API(kbase_pm_check_transitions)

void MOCKABLE(kbase_pm_enable_interrupts)(kbase_device *kbdev)
{

	OSK_ASSERT( NULL != kbdev );
	/*
	 * Clear all interrupts,
	 * and unmask them all.
	 */
	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_IRQ_CLEAR), GPU_IRQ_REG_ALL, NULL);
	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_IRQ_MASK), GPU_IRQ_REG_ALL, NULL);

	kbase_reg_write(kbdev, JOB_CONTROL_REG(JOB_IRQ_CLEAR), 0xFFFFFFFF, NULL);
	kbase_reg_write(kbdev, JOB_CONTROL_REG(JOB_IRQ_MASK), 0xFFFFFFFF, NULL);

	kbase_reg_write(kbdev, MMU_REG(MMU_IRQ_CLEAR), 0xFFFFFFFF, NULL);
	kbase_reg_write(kbdev, MMU_REG(MMU_IRQ_MASK), 0xFFFFFFFF, NULL);
}
KBASE_EXPORT_TEST_API(kbase_pm_enable_interrupts)

void MOCKABLE(kbase_pm_disable_interrupts)(kbase_device *kbdev)
{

	OSK_ASSERT( NULL != kbdev );
	/*
	 * Mask all interrupts,
	 * and clear them all.
	 */
	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_IRQ_MASK), 0, NULL);
	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_IRQ_CLEAR), GPU_IRQ_REG_ALL, NULL);

	kbase_reg_write(kbdev, JOB_CONTROL_REG(JOB_IRQ_MASK), 0, NULL);
	kbase_reg_write(kbdev, JOB_CONTROL_REG(JOB_IRQ_CLEAR), 0xFFFFFFFF, NULL);

	kbase_reg_write(kbdev, MMU_REG(MMU_IRQ_MASK), 0, NULL);
	kbase_reg_write(kbdev, MMU_REG(MMU_IRQ_CLEAR), 0xFFFFFFFF, NULL);
}
KBASE_EXPORT_TEST_API(kbase_pm_disable_interrupts)

/*
 * pmu layout:
 * 0x0000: PMU TAG (RO) (0xCAFECAFE)
 * 0x0004: PMU VERSION ID (RO) (0x00000000)
 * 0x0008: CLOCK ENABLE (RW) (31:1 SBZ, 0 CLOCK STATE)
 */
static void kbase_pm_hw_issues(kbase_device *kbdev);
void MOCKABLE(kbase_pm_clock_on)(kbase_device *kbdev)
{
	OSK_ASSERT( NULL != kbdev );

	if (kbdev->pm.gpu_powered)
	{
		/* Already turned on */
		return;
	}

	/* The GPU is going to transition, so unset the wait queues until the policy
	 * informs us that the transition is complete */
	osk_waitq_clear(&kbdev->pm.power_up_waitqueue);
	osk_waitq_clear(&kbdev->pm.power_down_waitqueue);

	if (kbase_device_has_feature(kbdev, KBASE_FEATURE_HAS_MODEL_PMU))
		kbase_os_reg_write(kbdev, 0x4008, 1);

	if (kbdev->pm.callback_power_on && kbdev->pm.callback_power_on(kbdev))
	{
		/* GPU state was lost, reset GPU to ensure it is in a consistent state */
		kbase_pm_init_hw(kbdev);
	}

	osk_spinlock_irq_lock(&kbdev->pm.gpu_powered_lock);
	kbdev->pm.gpu_powered = MALI_TRUE;
	osk_spinlock_irq_unlock(&kbdev->pm.gpu_powered_lock);
}
KBASE_EXPORT_TEST_API(kbase_pm_clock_on)

void MOCKABLE(kbase_pm_clock_off)(kbase_device *kbdev)
{
	OSK_ASSERT( NULL != kbdev );

	if (!kbdev->pm.gpu_powered)
	{
		/* Already turned off */
		return;
	}

	/* Ensure that any IRQ handlers have finished */
	kbase_synchronize_irqs(kbdev);

	/* The GPU power may be turned off from this point */
	osk_spinlock_irq_lock(&kbdev->pm.gpu_powered_lock);
	kbdev->pm.gpu_powered = MALI_FALSE;
	osk_spinlock_irq_unlock(&kbdev->pm.gpu_powered_lock);

	if (kbdev->pm.callback_power_off)
	{
		kbdev->pm.callback_power_off(kbdev);
	}

	if (kbase_device_has_feature(kbdev, KBASE_FEATURE_HAS_MODEL_PMU))
		kbase_os_reg_write(kbdev, 0x4008, 0);
}
KBASE_EXPORT_TEST_API(kbase_pm_clock_off)

struct kbasep_reset_timeout_data
{
	mali_bool timed_out;
	kbase_device *kbdev;
};

static void kbasep_reset_timeout(void *data)
{
	struct kbasep_reset_timeout_data *rtdata = (struct kbasep_reset_timeout_data*)data;

	rtdata->timed_out = 1;

	/* Set the wait queue to wake up kbase_pm_init_hw even though the reset hasn't completed */
	kbase_pm_reset_done(rtdata->kbdev);
}

static void kbase_pm_hw_issues(kbase_device *kbdev)
{
	uint32_t gpu_id = kbase_reg_read(kbdev, GPU_CONTROL_REG(GPU_ID), NULL);
	if (gpu_id == 0x69560000)
	{
		/* Needed due to MIDBASE-748 */
		kbase_reg_write(kbdev, GPU_CONTROL_REG(PWR_KEY), 0x2968A819, NULL);
		kbase_reg_write(kbdev, GPU_CONTROL_REG(PWR_OVERRIDE0), 0x80, NULL);

		/* Needed due to MIDBASE-1092 */
		kbase_reg_write(kbdev, GPU_CONTROL_REG(L2_MMU_CONFIG), 0x80000000, NULL);
	}
	if (gpu_id == 0x69560000 || gpu_id == 0x69560001)
	{
		/* Needed due to MIDBASE-1494: LS_PAUSEBUFFER_DISABLE. See PRLAM-8443. */
		kbase_reg_write(kbdev, GPU_CONTROL_REG(SHADER_CONFIG), 0x00010000, NULL);
	}
}

mali_error kbase_pm_init_hw(kbase_device *kbdev)
{
	uint32_t gpu_id;
	osk_timer timer;
	struct kbasep_reset_timeout_data rtdata;
	osk_error osk_err;

	OSK_ASSERT( NULL != kbdev );

	/* Ensure the clock is on before attempting to access the hardware */
	if (!kbdev->pm.gpu_powered)
	{
		/* The GPU is going to transition, so unset the wait queues until the policy
		 * informs us that the transition is complete */
		osk_waitq_clear(&kbdev->pm.power_up_waitqueue);
		osk_waitq_clear(&kbdev->pm.power_down_waitqueue);

		if (kbase_device_has_feature(kbdev, KBASE_FEATURE_HAS_MODEL_PMU))
			kbase_os_reg_write(kbdev, 0x4008, 1);

		if (kbdev->pm.callback_power_on)
			kbdev->pm.callback_power_on(kbdev);

		osk_spinlock_irq_lock(&kbdev->pm.gpu_powered_lock);
		kbdev->pm.gpu_powered = MALI_TRUE;
		osk_spinlock_irq_unlock(&kbdev->pm.gpu_powered_lock);
	}
	/* Read the ID register */
	gpu_id = kbase_reg_read(kbdev, GPU_CONTROL_REG(GPU_ID), NULL);
	OSK_PRINT_INFO(OSK_BASE_PM, "GPU identified as '%c%c' r%dp%d v%d",
	            (gpu_id>>16) & 0xFF,
	            (gpu_id>>24) & 0xFF,
	            (gpu_id>>12) & 0xF,
	            (gpu_id>>4) & 0xFF,
	            (gpu_id) & 0xF);

	/* Only support for T60x or T65x at present. */
	if ( ((gpu_id >> 16) != 0x6956 ) && ((gpu_id >> 16) != 0x3456 ) )
	{
		OSK_PRINT_ERROR(OSK_BASE_PM, "This GPU is not supported by this driver (GPU_ID=0x%lx)\n", gpu_id);
		return MALI_ERROR_FUNCTION_FAILED;
	}

	/* Ensure interrupts are off to begin with, this also clears any outstanding interrupts */
	kbase_pm_disable_interrupts(kbdev);

	/* Soft reset the GPU */
	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_COMMAND), 1, NULL);

	/* Unmask the reset complete interrupt only */
	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_IRQ_MASK), (1<<8), NULL);

	/* If the GPU never asserts the reset interrupt we just assume that the reset has completed */
	if (kbase_device_has_feature(kbdev, KBASE_FEATURE_LACKS_RESET_INT))
	{
		goto out;
	}
	
	/* Initialize a structure for tracking the status of the reset */
	rtdata.kbdev = kbdev;
	rtdata.timed_out = 0;

	/* Create a timer to use as a timeout on the reset */
	osk_err = osk_timer_on_stack_init(&timer);
	if (OSK_ERR_NONE != osk_err)
	{
		return MALI_ERROR_FUNCTION_FAILED;
	}

	osk_timer_callback_set(&timer, kbasep_reset_timeout, &rtdata);
	osk_err = osk_timer_start(&timer, RESET_TIMEOUT);
	if (OSK_ERR_NONE != osk_err)
	{
		osk_timer_on_stack_term(&timer);
		return MALI_ERROR_FUNCTION_FAILED;
	}
	/* Wait for the RESET_COMPLETED interrupt to be raised,
	 * we use the "power up" waitqueue since it isn't in use yet */
	osk_waitq_wait(&kbdev->pm.power_up_waitqueue);

	if (rtdata.timed_out == 0)
	{
		/* GPU has been reset */
		osk_timer_stop(&timer);
		osk_timer_on_stack_term(&timer);

		goto out;
	}

	/* No interrupt has been received - check if the RAWSTAT register says the reset has completed */
	if (kbase_reg_read(kbdev, GPU_CONTROL_REG(GPU_IRQ_RAWSTAT), NULL) & (1<<8))
	{
		/* The interrupt is set in the RAWSTAT; this suggests that the interrupts are not getting to the CPU */
		OSK_PRINT_WARN(OSK_BASE_PM, "Reset interrupt didn't reach CPU. Check interrupt assignments.\n");
		/* If interrupts aren't working we can't continue. */
		osk_timer_on_stack_term(&timer);
		goto out;
	}

	/* The GPU doesn't seem to be responding to the reset so try a hard reset */
	OSK_PRINT_WARN(OSK_BASE_PM, "Failed to soft reset GPU, attempting a hard reset\n");
	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_COMMAND), 2, NULL);

	/* Restart the timer to wait for the hard reset to complete */
	rtdata.timed_out = 0;
	osk_err = osk_timer_start(&timer, RESET_TIMEOUT);
	if (OSK_ERR_NONE != osk_err)
	{
		osk_timer_on_stack_term(&timer);
		return MALI_ERROR_FUNCTION_FAILED;
	}

	/* Wait for the RESET_COMPLETED interrupt to be raised,
	 * we use the "power up" waitqueue since it isn't in use yet */
	osk_waitq_wait(&kbdev->pm.power_up_waitqueue);

	if (rtdata.timed_out == 0)
	{
		/* GPU has been reset */
		osk_timer_stop(&timer);
		osk_timer_on_stack_term(&timer);

		goto out;
	}

	osk_timer_on_stack_term(&timer);

	OSK_PRINT_ERROR(OSK_BASE_PM, "Failed to reset the GPU\n");

	/* The GPU still hasn't reset, give up */
	return MALI_ERROR_FUNCTION_FAILED;

out:
	/* If cycle counter was in use-re enable it */
	osk_spinlock_irq_lock( &kbdev->pm.gpu_cycle_counter_requests_lock );

	if ( kbdev->pm.gpu_cycle_counter_requests )
	{
		kbase_reg_write( kbdev, GPU_CONTROL_REG(GPU_COMMAND), GPU_COMMAND_CYCLE_COUNT_START, NULL );
	}

	osk_spinlock_irq_unlock( &kbdev->pm.gpu_cycle_counter_requests_lock );

	kbase_pm_hw_issues(kbdev);

	return MALI_ERROR_NONE;
}
KBASE_EXPORT_TEST_API(kbase_pm_init_hw)

void kbase_pm_request_gpu_cycle_counter( kbase_device *kbdev )
{
	OSK_ASSERT( kbdev != NULL );
	
	OSK_ASSERT( kbdev->pm.gpu_powered );

	osk_spinlock_irq_lock( &kbdev->pm.gpu_cycle_counter_requests_lock );

	OSK_ASSERT( kbdev->pm.gpu_cycle_counter_requests < INT_MAX );

	++kbdev->pm.gpu_cycle_counter_requests;

	if ( 1 == kbdev->pm.gpu_cycle_counter_requests )
	{
		kbase_reg_write( kbdev, GPU_CONTROL_REG(GPU_COMMAND), GPU_COMMAND_CYCLE_COUNT_START, NULL );
	}
	osk_spinlock_irq_unlock( &kbdev->pm.gpu_cycle_counter_requests_lock );
}
KBASE_EXPORT_TEST_API(kbase_pm_request_gpu_cycle_counter)

void kbase_pm_release_gpu_cycle_counter( kbase_device *kbdev )
{
	OSK_ASSERT( kbdev != NULL );

	osk_spinlock_irq_lock( &kbdev->pm.gpu_cycle_counter_requests_lock );

	OSK_ASSERT( kbdev->pm.gpu_cycle_counter_requests > 0 );

	--kbdev->pm.gpu_cycle_counter_requests;

	if ( 0 == kbdev->pm.gpu_cycle_counter_requests )
	{
		kbase_reg_write( kbdev, GPU_CONTROL_REG(GPU_COMMAND), GPU_COMMAND_CYCLE_COUNT_STOP, NULL );
	}
	osk_spinlock_irq_unlock( &kbdev->pm.gpu_cycle_counter_requests_lock );
}
KBASE_EXPORT_TEST_API(kbase_pm_release_gpu_cycle_counter)

