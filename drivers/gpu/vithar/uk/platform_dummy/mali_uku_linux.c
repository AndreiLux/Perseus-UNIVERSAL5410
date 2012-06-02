/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <errno.h>
#include <linux/version.h>

#include <uk/mali_uku.h>
#include <cdbg/mali_cdbg.h>
#include <cutils/cstr/mali_cutils_cstr.h>
#include <cutils/linked_list/mali_cutils_slist.h>
#include <stdlib/mali_stdlib.h>

#define LINUX_MALI_DEVICE_NAME "/dev/mali"

#if CSTD_OS_ANDROID == 0 /* Android 2.3.x doesn't support pthread_atfork */

/** Datastructures to keep track of all open file descriptors to UK clients for this process */
struct _fd_admin
{
	pthread_mutex_t fd_admin_mutex;   /** protects access to this datastructure */
	mali_bool atfork_registered;      /** MALI_TRUE when an atfork handler has been installed for this process */
	cutils_slist fd_list;             /** list tracking all open file descriptors to UK clients for this process */
};
STATIC struct _fd_admin fd_admin =
{
	PTHREAD_MUTEX_INITIALIZER,
	MALI_FALSE,
	{{NULL,NULL}}
};
typedef struct fd_list_item
{
	cutils_slist_item link;
	int fd;
} fd_list_item;


/** atfork handler called in child's context to close all open file descriptors to UK clients in the child */
STATIC void ukup_fd_child_atfork_handler(void)
{
	fd_list_item *item;

	/* close all file descriptors registered with ukup_add_file_descriptor() */
	CUTILS_SLIST_FOREACH(&fd_admin.fd_list, fd_list_item, link, item)
	{
		close(item->fd);
	}
}

/** removes and closes a file descriptor added to the list of open file descriptors to UK clients
 *  by ukup_fd_add() earlier
 */
STATIC void ukup_fd_remove_and_close(int fd)
{
	int rc;
	fd_list_item *item = NULL;

	rc = pthread_mutex_lock(&fd_admin.fd_admin_mutex);
	if (rc != 0)
	{
		CDBG_PRINT_INFO(CDBG_BASE, "can't lock file descriptor list, error %d\n", errno);
		goto exit_mutex_lock;
	}

	CUTILS_SLIST_FOREACH(&fd_admin.fd_list, fd_list_item, link, item)
	{
		if (item->fd == fd)
		{
			CUTILS_SLIST_REMOVE(&fd_admin.fd_list, item, link);
			stdlib_free(item);
			close(fd);
			break;
		}
	}

	pthread_mutex_unlock(&fd_admin.fd_admin_mutex);

	CDBG_ASSERT_MSG(CUTILS_SLIST_IS_VALID(item, link), "file descriptor %d not found on list!\n", fd);

exit_mutex_lock:
	return;
}


/** add a file descriptor to the list of open file descriptors to UK clients */
STATIC mali_bool ukup_fd_add(int fd)
{
	int rc;
	fd_list_item *item = NULL;

	rc = pthread_mutex_lock(&fd_admin.fd_admin_mutex);
	if (rc != 0)
	{
		CDBG_PRINT_INFO(CDBG_BASE, "can't lock file descriptor list, error %d\n", errno);
		goto exit_mutex_lock;
	}

	if (MALI_FALSE == fd_admin.atfork_registered)
	{
		CUTILS_SLIST_INIT(&fd_admin.fd_list);

		rc = pthread_atfork(NULL, NULL, ukup_fd_child_atfork_handler);
		if (rc != 0)
		{
			CDBG_PRINT_INFO(CDBG_BASE, "pthread_atfork failed, error %d\n", errno);
			goto exit;
		}

		fd_admin.atfork_registered = MALI_TRUE;
	}

	item = stdlib_malloc(sizeof(fd_list_item));
	if (NULL == item)
	{
		CDBG_PRINT_INFO(CDBG_BASE, "allocating file descriptor list item failed\n");
		goto exit;
	}

	item->fd = fd;

	CUTILS_SLIST_PUSH_FRONT(&fd_admin.fd_list, item, fd_list_item, link);

	pthread_mutex_unlock(&fd_admin.fd_admin_mutex);

	return MALI_TRUE;

exit:
	if (NULL != item)
	{
		stdlib_free(item);
	}

	pthread_mutex_unlock(&fd_admin.fd_admin_mutex);
exit_mutex_lock:
	return MALI_FALSE;
}

#endif /* CSTD_OS_ANDROID == 0 */


uku_open_status uku_open(uk_client_id id, u32 instance, uku_client_version *version, uku_context *uku_ctx)
{
	const char *linux_device_name;
	char format_device_name[16];
	struct stat filestat;
	int fd;
	uku_version_check_args version_check_args;
	mali_error err;

	CDBG_ASSERT_POINTER(version);
	CDBG_ASSERT_POINTER(uku_ctx);

	if(CDBG_SIMULATE_FAILURE(CDBG_BASE))
	{
		return UKU_OPEN_FAILED;
	}

	switch(id)
	{
		case UK_CLIENT_MALI_T600_BASE:
			cutils_cstr_snprintf(format_device_name, sizeof(format_device_name), "%s%d", LINUX_MALI_DEVICE_NAME, instance);
			linux_device_name = format_device_name;
			break;
		default:
			CDBG_ASSERT_MSG(MALI_FALSE, "invalid uk_client_id value (%d)\n", id);
			return UKU_OPEN_FAILED;
	}

	/* open the kernel device driver */
	fd = open(linux_device_name, O_RDWR|O_CLOEXEC);

	if (-1 == fd)
	{
		CDBG_PRINT_INFO(CDBG_BASE, "failed to open device file %s\n", linux_device_name);
		return UKU_OPEN_FAILED;
	}

	/* query the file for information */
	if (0 != fstat(fd, &filestat))
	{
		close(fd);
		CDBG_PRINT_INFO(CDBG_BASE, "failed to query device file %s for type information\n", linux_device_name);
		return UKU_OPEN_FAILED;
	}

	/* verify that it is a character special file */
	if (0 == S_ISCHR(filestat.st_mode))
	{
		close(fd);
		CDBG_PRINT_INFO(CDBG_BASE, "file %s is not a character device file", linux_device_name);
		return UKU_OPEN_FAILED;
	}

	/* use the internal UK call UKP_FUNC_ID_CHECK_VERSION to verify versions */
	version_check_args.header.id = UKP_FUNC_ID_CHECK_VERSION;
	version_check_args.major = version->major;
	version_check_args.minor = version->minor;

	uku_ctx->ukup_internal_struct.fd = fd;
	err = uku_call(uku_ctx, &version_check_args, sizeof(version_check_args));
	if (MALI_ERROR_NONE == err && MALI_ERROR_NONE == version_check_args.header.ret)
	{
		mali_bool incompatible =
		 ( (version->major != version_check_args.major) || (version->minor > version_check_args.minor) );

		if (incompatible)
		{
			CDBG_PRINT_INFO(CDBG_BASE, "file %s is not of a compatible version (user %d.%d, kernel %d.%d)\n",
			    linux_device_name, version->major, version->minor, version_check_args.major, version_check_args.minor);
		}

		/* output kernel-side version */
		version->major = version_check_args.major;
		version->minor = version_check_args.minor;

		if (incompatible)
		{
			uku_ctx->ukup_internal_struct.fd = -1;
			close(fd);
			return UKU_OPEN_INCOMPATIBLE;
		}
	}
	else
	{
		close(fd);
		return UKU_OPEN_FAILED;
	}

#ifdef MALI_DEBUG
	uku_ctx->ukup_internal_struct.canary = MALI_UK_CANARY_VALUE;
#endif

#if CSTD_OS_ANDROID == 0
	/* track all open file descriptors to UK clients */
	if (!ukup_fd_add(fd))
	{
		close(fd);
		return UKU_OPEN_FAILED;
	}
#endif

	return UKU_OPEN_OK;
}


void *uku_driver_context(uku_context *uku_ctx)
{
	CDBG_ASSERT_POINTER(uku_ctx);
	return (void *)&uku_ctx->ukup_internal_struct.fd;
}

void uku_close(uku_context *uku_ctx)
{
	CDBG_ASSERT_POINTER(uku_ctx);
#ifdef MALI_DEBUG
	CDBG_ASSERT(uku_ctx->ukup_internal_struct.canary == MALI_UK_CANARY_VALUE);
	uku_ctx->ukup_internal_struct.canary = 0;
#endif
#if CSTD_OS_ANDROID == 0
	ukup_fd_remove_and_close(uku_ctx->ukup_internal_struct.fd);
#else
	close(uku_ctx->ukup_internal_struct.fd);
#endif
}

mali_error uku_call(uku_context *uku_ctx, void *args, u32 args_size)
{
	uk_header *header = (uk_header *)args;
	u32 cmd;

	CDBG_ASSERT_POINTER(uku_ctx);
	CDBG_ASSERT_POINTER(args);
	CDBG_ASSERT_MSG(args_size >= sizeof(uk_header), "argument structure not large enough to contain required uk_header\n");

	if(CDBG_SIMULATE_FAILURE(CDBG_BASE))
	{
		return MALI_ERROR_FUNCTION_FAILED;
	}

	cmd = _IOC(_IOC_READ|_IOC_WRITE, LINUX_UK_BASE_MAGIC, header->id, args_size);

	/* call ioctl handler of driver */
	if (0 != ioctl(uku_ctx->ukup_internal_struct.fd, cmd, args))
	{
		return MALI_ERROR_FUNCTION_FAILED;
	}
	else
	{
		return MALI_ERROR_NONE;
	}
}
