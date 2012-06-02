/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_kbase_8401_workaround.h
 * Functions related to working around BASE_HW_ISSUE_8401
 */

#ifndef _KBASE_8401_WORKAROUND_H_
#define _KBASE_8401_WORKAROUND_H_

mali_error kbasep_8401_workaround_init(kbase_device *kbdev);
void kbasep_8401_workaround_term(kbase_device *kbdev);
void kbasep_8401_submit_dummy_job(kbase_device *kbdev, int js);
mali_bool kbasep_8401_is_workaround_job(kbase_jd_atom *katom);

#endif /* _KBASE_8401_WORKAROUND_H_ */
