/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <osk/mali_osk.h>

oskp_debug_assert_cb oskp_debug_assert_registered_cb =
{
	NULL,
	NULL
};

void oskp_debug_print(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	oskp_validate_format_string(format);
	vprintk(format, args);
	va_end(args);
}

s32 osk_snprintf(char *str, size_t size, const char *format, ...)
{
	va_list args;
	s32 ret;
	va_start(args, format);
	oskp_validate_format_string(format);
	ret = vsnprintf(str, size, format, args);
	va_end(args);
	return ret;
}

void osk_debug_assert_register_hook( osk_debug_assert_hook *func, void *param )
{
	oskp_debug_assert_registered_cb.func = func;
	oskp_debug_assert_registered_cb.param = param;
}

void oskp_debug_assert_call_hook( void )
{
	if ( oskp_debug_assert_registered_cb.func != NULL )
	{
		oskp_debug_assert_registered_cb.func( oskp_debug_assert_registered_cb.param );
	}
}
