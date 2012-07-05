/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_kbase_pm_always_on.c
 * "Always on" power management policy
 */

#include <osk/mali_osk.h>
#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_kbase_pm.h>


/** Function to handle a GPU state change for the always_on power policy.
 *
 * This function is called whenever the GPU has transitioned to another state. It first checks that the transition is
 * complete and then moves the state machine to the next state.
 *
 * @param kbdev     The kbase device structure for the device
 */
static void always_on_state_changed(kbase_device *kbdev)
{
	kbasep_pm_policy_always_on *data = &kbdev->pm.policy_data.always_on;

	switch(data->state)
	{
	case KBASEP_PM_ALWAYS_ON_STATE_POWERING_UP:
		if (kbase_pm_get_pwr_active(kbdev))
		{
			/* Cores still transitioning */
			return;
		}
		/* All cores have transitioned, inform the OS */
		kbase_pm_power_up_done(kbdev);
		data->state = KBASEP_PM_ALWAYS_ON_STATE_POWERED_UP;

		break;
	case KBASEP_PM_ALWAYS_ON_STATE_POWERING_DOWN:
		if (kbase_pm_get_pwr_active(kbdev))
		{
			/* Cores still transitioning */
			return;
		}
		/* All cores have transitioned, turn the clock and interrupts off */
		kbase_pm_disable_interrupts(kbdev);
		kbase_pm_clock_off(kbdev);

		/* Inform the OS */
		kbase_pm_power_down_done(kbdev);

		data->state = KBASEP_PM_ALWAYS_ON_STATE_POWERED_DOWN;

		break;
	case KBASEP_PM_ALWAYS_ON_STATE_CHANGING_POLICY:
		if (kbase_pm_get_pwr_active(kbdev))
		{
			/* Cores still transitioning */
			return;
		}
		/* All cores have transitioned, inform the system we can change policy*/
		kbase_pm_change_policy(kbdev);

		break;
	default:
		break;
	}
}

/** Function to handle the @ref KBASE_PM_EVENT_SYSTEM_SUSPEND message for the always_on power policy.
 *
 * This function is called when a @ref KBASE_PM_EVENT_SYSTEM_SUSPEND message is received. It instructs the GPU to turn off
 * all cores.
 *
 * @param kbdev     The kbase device structure for the device
 */
static void always_on_suspend(kbase_device *kbdev)
{
	u64 cores;

	/* Inform the system that the transition has started */
	kbase_pm_power_transitioning(kbdev);

	/* Turn the cores off */
	cores = kbase_pm_get_present_cores(kbdev, KBASE_PM_CORE_SHADER);
	kbase_pm_invoke_power_down(kbdev, KBASE_PM_CORE_SHADER, cores);

	cores = kbase_pm_get_present_cores(kbdev, KBASE_PM_CORE_TILER);
	kbase_pm_invoke_power_down(kbdev, KBASE_PM_CORE_TILER, cores);

	kbase_pm_check_transitions(kbdev);

	kbdev->pm.policy_data.always_on.state = KBASEP_PM_ALWAYS_ON_STATE_POWERING_DOWN;

	/* Ensure that the OS is informed even if we didn't do anything */
	always_on_state_changed(kbdev);
}

/** Function to handle the @ref KBASE_PM_EVENT_SYSTEM_RESUME message for the always_on power policy.
 *
 * This function is called when a @ref KBASE_PM_EVENT_SYSTEM_RESUME message is received. It instructs the GPU to turn on all
 * the cores.
 *
 * @param kbdev     The kbase device structure for the device
 */
static void always_on_resume(kbase_device *kbdev)
{
	u64 cores;

	/* Inform the system that the transition has started */
	kbase_pm_power_transitioning(kbdev);

	/* Turn the clock on */
	kbase_pm_clock_on(kbdev);
	/* Enable interrupts */
	kbase_pm_enable_interrupts(kbdev);

	/* Turn the cores on */
	cores = kbase_pm_get_present_cores(kbdev, KBASE_PM_CORE_SHADER);
	kbase_pm_invoke_power_up(kbdev, KBASE_PM_CORE_SHADER, cores);

	cores = kbase_pm_get_present_cores(kbdev, KBASE_PM_CORE_TILER);
	kbase_pm_invoke_power_up(kbdev, KBASE_PM_CORE_TILER, cores);

	kbase_pm_check_transitions(kbdev);

	kbdev->pm.policy_data.always_on.state = KBASEP_PM_ALWAYS_ON_STATE_POWERING_UP;

	/* Ensure that the OS is informed even if we didn't do anything */
	always_on_state_changed(kbdev);
}

/** The event callback function for the always_on power policy.
 *
 * This function is called to handle the events for the power policy. It calls the relevant handler function depending
 * on the type of the event.
 *
 * @param kbdev     The kbase device structure for the device
 * @param event     The event that should be processed
 */
static void always_on_event(kbase_device *kbdev, kbase_pm_event event)
{
	kbasep_pm_policy_always_on *data = &kbdev->pm.policy_data.always_on;

	switch(event)
	{
	case KBASE_PM_EVENT_SYSTEM_SUSPEND:
		always_on_suspend(kbdev);
		break;
	case KBASE_PM_EVENT_POLICY_INIT: /* Init is the same as resume for this policy */
	case KBASE_PM_EVENT_SYSTEM_RESUME:
		always_on_resume(kbdev);
		break;
	case KBASE_PM_EVENT_GPU_STATE_CHANGED:
		always_on_state_changed(kbdev);
		break;
	case KBASE_PM_EVENT_POLICY_CHANGE:
		if (data->state == KBASEP_PM_ALWAYS_ON_STATE_POWERED_UP ||
		    data->state == KBASEP_PM_ALWAYS_ON_STATE_POWERED_DOWN)
		{
			kbase_pm_change_policy(kbdev);
		}
		else
		{
			data->state = KBASEP_PM_ALWAYS_ON_STATE_CHANGING_POLICY;
		}
		break;
	case KBASE_PM_EVENT_GPU_ACTIVE:
	case KBASE_PM_EVENT_GPU_IDLE:
		break;
	case KBASE_PM_EVENT_CHANGE_GPU_STATE:
		/*
		 * Note that the GPU is always kept on, however we still may
		 * be required to update anyone waiting for power up events.
		 */
		kbase_pm_check_transitions(kbdev);
		break;
	default:
		/* Unrecognised event - this should never happen */
		OSK_ASSERT(0);
	}
}

/** Initialize the always_on power policy
 *
 * This sets up the private @ref kbase_pm_device_data.policy_data field of the device for use with the always_on power
 * policy.
 *
 * @param kbdev     The kbase device structure for the device
 */
static void always_on_init(kbase_device *kbdev)
{
	kbasep_pm_policy_always_on *data = &kbdev->pm.policy_data.always_on;

	data->state = KBASEP_PM_ALWAYS_ON_STATE_POWERING_UP;
}

/** Terminate the always_on power policy
 *
 * This frees the resources that were allocated by @ref always_on_init.
 *
 * @param kbdev     The kbase device structure for the device
 */
static void always_on_term(kbase_device *kbdev)
{
	CSTD_UNUSED(kbdev);
}

/** The @ref kbase_pm_policy structure for the always_on power policy
 *
 * This is the extern structure that defines the always_on power policy's callback and name.
 */
const kbase_pm_policy kbase_pm_always_on_policy_ops =
{
	"always_on",                /* name */
	always_on_init,             /* init */
	always_on_term,             /* term */
	always_on_event,            /* event */
};

KBASE_EXPORT_TEST_API(kbase_pm_always_on_policy_ops)
