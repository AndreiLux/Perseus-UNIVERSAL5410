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
 * @file osk/src/common/mali_osk_compile_asserts.h
 *
 * Private definitions of compile time asserts.
 **/

/**
 * Unreachable function needed to check values at compile-time, in both debug
 * and release builds
 */
void oskp_cmn_compile_time_assertions(void);
