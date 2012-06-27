/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */


#include <osk/mali_osk.h>
#include "mali_osk_compile_asserts.h"

/**
 * @brief Contains the module names (modules in the same order as for the osk_module enumeration)
 * @sa oskp_module_to_str
 */
static const char* CONST oskp_str_modules[] =
{
	"UNKNOWN",     /**< Unknown module */
	"OSK",         /**< OSK */
	"UKK",         /**< UKK */
	"BASE_MMU",    /**< Base MMU */
	"BASE_JD",     /**< Base Job Dispatch */
	"BASE_JM",     /**< Base Job Manager */
	"BASE_CORE",   /**< Base Core */
	"BASE_MEM",    /**< Base Memory */
	"BASE_EVENT",  /**< Base Event */
	"BASE_CTX",    /**< Base Context */
	"BASE_PM",     /**< Base Power Management */
	"UMP",         /**< UMP */
};

#define MODULE_STRING_ARRAY_SIZE (sizeof(oskp_str_modules)/sizeof(oskp_str_modules[0]))

INLINE void oskp_cmn_compile_time_assertions(void)
{
	/*
	 * If this assert triggers you have forgotten to update oskp_str_modules
	 * when you added a module to the osk_module enum
	 * */
	CSTD_COMPILE_TIME_ASSERT(OSK_MODULES_ALL == MODULE_STRING_ARRAY_SIZE );
}

const char* oskp_module_to_str(const osk_module module)
{
	if( MODULE_STRING_ARRAY_SIZE <= module)
	{
		return "";
	}
	return oskp_str_modules[module];
}

void oskp_validate_format_string(const char *format, ...)
{
#if MALI_DEBUG
	char c;
	static const char *supported[] =
	{
		"d", "ld", "lld",
		"x", "lx", "llx",
		"X", "lX", "llX",
		"u", "lu", "llu",
		"p",
		"c",
		"s",
	};
	static const unsigned char sizes[] = { 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 1, 1 };

	unsigned int i;

	/* %[flags][width][.precision][length]specifier  */

	while ( (c = *format++) )
	{
		if (c == '%')
		{
			c = *format;

			if (c == '\0')
			{
				/* Unsupported format */
				OSK_PRINT_WARN(OSK_OSK, "OSK Format specification not complete (%% not followed by anything)\n");
				return;
			}
			else if (c != '%')
			{
				/* Skip to the [length]specifier part assuming it starts with
				 * an alphabetic character and flags, width, precision do not
				 * contain alphabetic characters.
				 */
				do
				{
					if ((c >= 'a' && c <= 'z') || c == 'X')
					{
						/* Match supported formats with current position in format string */
						for (i = 0; i < NELEMS(supported); i++)
						{
							if (strncmp(format, supported[i], sizes[i]) == 0)
							{
								/* Supported format */
								break;
							}
						}

						if (i == NELEMS(supported))
						{
							/* Unsupported format */
							OSK_PRINT_WARN(OSK_OSK, "OSK Format string specifier not supported (starting at '%s')\n", format);
							return;
						}

						/* Start looking for next '%' */
						break;
					}
				} while ( (c = *++format) );
			}
		}
	}
#else
	CSTD_UNUSED(format);
#endif
}
