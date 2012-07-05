/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_kbase_pm_always_on.h
 * "Always on" power management policy
 */

#ifndef MALI_KBASE_PM_ALWAYS_ON_H
#define MALI_KBASE_PM_ALWAYS_ON_H

/** The states that the always_on policy can enter.
 *
 * The diagram below should the states that the always_on policy can enter and the transitions that can occur between
 * the states:
 *
 * @dot
 * digraph always_on_states {
 *      node [fontsize=10];
 *      edge [fontsize=10];
 *
 *      POWERING_UP     [label="STATE_POWERING_UP"
 *                      URL="\ref kbasep_pm_always_on_state.KBASEP_PM_ALWAYS_ON_STATE_POWERING_UP"];
 *      POWERING_DOWN   [label="STATE_POWERING_DOWN"
 *                      URL="\ref kbasep_pm_always_on_state.KBASEP_PM_ALWAYS_ON_STATE_POWERING_DOWN"];
 *      POWERED_UP      [label="STATE_POWERED_UP"
 *                      URL="\ref kbasep_pm_always_on_state.KBASEP_PM_ALWAYS_ON_STATE_POWERED_UP"];
 *      POWERED_DOWN    [label="STATE_POWERED_DOWN"
 *                      URL="\ref kbasep_pm_always_on_state.KBASEP_PM_ALWAYS_ON_STATE_POWERED_DOWN"];
 *      CHANGING_POLICY [label="STATE_CHANGING_POLICY"
 *                      URL="\ref kbasep_pm_always_on_state.KBASEP_PM_ALWAYS_ON_STATE_CHANGING_POLICY"];
 *
 *      init            [label="init"                   URL="\ref KBASE_PM_EVENT_INIT"];
 *      change_policy   [label="change_policy"          URL="\ref kbase_pm_change_policy"];
 *
 *      init -> POWERING_UP [ label = "Policy init" ];
 *
 *      POWERING_UP -> POWERED_UP [label = "Power state change" URL="\ref KBASE_PM_EVENT_STATE_CHANGED"];
 *      POWERING_DOWN -> POWERED_DOWN [label = "Power state change" URL="\ref KBASE_PM_EVENT_STATE_CHANGED"];
 *      CHANGING_POLICY -> change_policy [label = "Power state change" URL="\ref KBASE_PM_EVENT_STATE_CHANGED"];
 *
 *      POWERED_UP -> POWERING_DOWN [label = "Suspend" URL="\ref KBASE_PM_EVENT_SUSPEND"];
 *
 *      POWERED_DOWN -> POWERING_UP [label = "Resume" URL="\ref KBASE_PM_EVENT_RESUME"];
 *
 *      POWERING_UP -> CHANGING_POLICY [label = "Change policy" URL="\ref KBASE_PM_EVENT_CHANGE_POLICY"];
 *      POWERING_DOWN -> CHANGING_POLICY [label = "Change policy" URL="\ref KBASE_PM_EVENT_CHANGE_POLICY"];
 *      POWERED_UP -> change_policy [label = "Change policy" URL="\ref KBASE_PM_EVENT_CHANGE_POLICY"];
 *      POWERED_DOWN -> change_policy [label = "Change policy" URL="\ref KBASE_PM_EVENT_CHANGE_POLICY"];
 * }
 * @enddot
 */
typedef enum kbasep_pm_always_on_state
{
	KBASEP_PM_ALWAYS_ON_STATE_POWERING_UP,      /**< The GPU is powering up */
	KBASEP_PM_ALWAYS_ON_STATE_POWERING_DOWN,    /**< The GPU is powering down */
	KBASEP_PM_ALWAYS_ON_STATE_POWERED_UP,       /**< The GPU is powered up and jobs can execute */
	KBASEP_PM_ALWAYS_ON_STATE_POWERED_DOWN,     /**< The GPU is powered down and the system can suspend */
	KBASEP_PM_ALWAYS_ON_STATE_CHANGING_POLICY   /**< The power policy is about to change */
} kbasep_pm_always_on_state;

/** Private structure for policy instance data.
 *
 * This contains data that is private to the particular power policy that is active.
 */
typedef struct kbasep_pm_policy_always_on
{
	kbasep_pm_always_on_state state;  /**< The current state of the policy */
} kbasep_pm_policy_always_on;

#endif
