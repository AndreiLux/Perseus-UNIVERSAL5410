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
 * @file mali_kbase_security.h
 * Base kernel security capability APIs
 */

#ifndef _KBASE_SECURITY_H_
#define _KBASE_SECURITY_H_

/* Security flags */
#define KBASE_SEC_FLAG_NOAUDIT (0u << 0)              /* Silently handle privilege failure */
#define KBASE_SEC_FLAG_AUDIT   (1u << 0)              /* Write audit message on privilege failure */
#define KBASE_SEC_FLAG_MASK    (KBASE_SEC_FLAG_AUDIT) /* Mask of all valid flag bits */

/* List of unique capabilities that have security access privileges */
typedef enum {
		/* Instrumentation Counters access privilege */
        KBASE_SEC_INSTR_HW_COUNTERS_COLLECT = 1,
        KBASE_SEC_MODIFY_PRIORITY
		/* Add additional access privileges here */
} kbase_security_capability;


/**
 * kbase_security_has_capability - determine whether a task has a particular effective capability
 * @param[in]   kctx    The task context.
 * @param[in]   cap     The capability to check for.
 * @param[in]   flags   Additional configuration information
 *                      Such as whether to write an audit message or not.
 * @return MALI_TRUE if success (capability is allowed), MALI_FALSE otherwise.
 */

mali_bool kbase_security_has_capability(kbase_context *kctx, kbase_security_capability cap, u32 flags);

#endif /* _KBASE_SECURITY_H_ */
