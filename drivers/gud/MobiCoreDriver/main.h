/**
 * Header file of MobiCore Driver Kernel Module.
 *
 * @addtogroup MCD_MCDIMPL_KMOD_IMPL
 * @{
 * Internal structures of the McDrvModule
 * @file
 *
 * Header file the MobiCore Driver Kernel Module,
 * its internal structures and defines.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MC_DRV_KMOD_H_
#define _MC_DRV_KMOD_H_

#include <asm/pgtable.h>
#include <linux/semaphore.h>
#include <linux/completion.h>

#include "public/mc_linux.h"
/** Platform specific settings */
#include "platform.h"

#define MC_VERSION(major, minor) \
		(((major & 0x0000ffff) << 16) | (minor & 0x0000ffff))

/**
 * Contiguous buffer allocated to TLCs.
 * These buffers are uses as world shared memory (wsm) and shared with
 * secure world.
 * The virtual kernel address is added for a simpler search algorithm.
 */
struct mc_buffer {
	unsigned int	handle; /* unique handle */
	void		*virt_user_addr; /**< virtual User start address */
	void		*virt_kernel_addr; /**< virtual Kernel start address */
	void		*phys_addr; /**< physical start address */
	unsigned int	order; /**< order of number of pages */
};

/** Maximum number of contiguous buffer allocations that one process can get via
 * mmap. */
#define MC_DRV_KMOD_MAX_CONTG_BUFFERS	16

/** Instance data for MobiCore Daemon and TLCs. */
struct mc_instance {
	struct mutex lock;
	/** unique handle */
	unsigned int handle;
	bool admin;
	/** process that opened this instance */
	pid_t pid_vnr;
	/** buffer list for mmap generated address space and
	 * its virtual client address */
	struct mc_buffer buffers[MC_DRV_KMOD_MAX_CONTG_BUFFERS];
};

/** MobiCore Driver Kernel Module context data. */
struct mc_context {
	/** MobiCore MCI information */
	struct mc_buffer	mci_base;
	/** MobiCore MCP buffer */
	struct mc_mcp_buffer	*mcp;
	/** event completion */
	struct completion	isr_comp;
	/** isr event counter */
	unsigned int		evt_counter;
	atomic_t		isr_counter;
	/** ever incrementing counter */
	atomic_t		unique_counter;
	/** pointer to instance of daemon */
	struct mc_instance	*daemon_inst;
};

struct mc_sleep_mode {
	uint16_t	SleepReq;
	uint16_t	ReadyToSleep;
};

/**< MobiCore is idle. No scheduling required. */
#define SCHEDULE_IDLE		0
/**< MobiCore is non idle, scheduling is required. */
#define SCHEDULE_NON_IDLE	1

/** MobiCore status flags */
struct mc_flags {
	/**< Scheduling hint: if <> SCHEDULE_IDLE, MobiCore should
	 * be scheduled by the NWd */
	uint32_t		schedule;
	/**<  */
	struct mc_sleep_mode	sleep_mode;
       /**< Reserved for future use: Must not be interpreted */
	uint32_t		rfu[2];
};

/** MCP buffer structure */
struct mc_mcp_buffer {
	/**< MobiCore Flags */
	struct mc_flags	flags;
	uint32_t	rfu; /**< MCP message buffer - ignore */
} ;

unsigned int get_unique_id(void);
/* check if caller is MobiCore Daemon */
static inline bool is_daemon(struct mc_instance *instance)
{
	if (!instance)
		return false;
	return instance->admin;
}

#endif /* _MC_DRV_KMOD_H_ */
/** @} */
