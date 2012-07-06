#ifndef FIMC_IS_INTERFACE_H
#define FIMC_IS_INTERFACE_H

#include "fimc-is-framemgr.h"
#include "fimc-is-video.h"

#define LOWBIT_OF(num)	(num >= 32 ? 0 : (u32)1<<num)
#define HIGHBIT_OF(num)	(num >= 32 ? (u32)1<<(num-32) : 0)

enum fimc_is_interface_state {
	IS_IF_STATE_IDLE,
	IS_IF_STATE_BLOCK_IO,
	IS_IF_STATE_NBLOCK_IO
};

struct fimc_is_interface_msg {
	u32	id;
	u32	command;
	u32	instance;
	u32	parameter1;
	u32	parameter2;
	u32	parameter3;
	u32	parameter4;
};

struct fimc_is_interface {
	void __iomem			*regs;
	struct is_common_reg __iomem	*com_regs;
	enum fimc_is_interface_state	state;
	spinlock_t			process_barrier;
	struct mutex			request_barrier;
	struct mutex			state_barrier;
	wait_queue_head_t		wait_queue;
	struct work_struct		work_queue[INTR_MAX_MAP];
	struct fimc_is_interface_msg	work[INTR_MAX_MAP];

	bool				streaming;

	struct is_region		*is_region;
	struct is_share_region		*is_shared_region;

	struct fimc_is_framemgr		*framemgr;

	void				*is;
	struct fimc_is_video_common	*video_sensor;
	struct fimc_is_video_common	*video_scp;
};

int fimc_is_interface_probe(struct fimc_is_interface *this,
	struct fimc_is_framemgr *framemgr,
	struct resource *resource, u32 irq,
	struct is_region *addr1,
	struct is_share_region *addr2,
	void *data);

int fimc_is_hw_enum(struct fimc_is_interface *interface,
	u32 instances);
int fimc_is_hw_open(struct fimc_is_interface *interface,
	u32 instance, u32 sensor, u32 channel);
int fimc_is_hw_saddr(struct fimc_is_interface *interface,
	u32 instance, u32 *setfile_addr);
int fimc_is_hw_setfile(struct fimc_is_interface *interface,
	u32 instance);
int fimc_is_hw_process_on(struct fimc_is_interface *interface,
	u32 instance);
int fimc_is_hw_process_off(struct fimc_is_interface *interface,
	u32 instance);
int fimc_is_hw_stream_on(struct fimc_is_interface *interface,
	u32 instance);
int fimc_is_hw_stream_off(struct fimc_is_interface *interface,
	u32 instance);
int fimc_is_hw_s_param(struct fimc_is_interface *interface,
	u32 instance, u32 indexes, u32 lindex, u32 hindex);
int fimc_is_hw_a_param(struct fimc_is_interface *interface,
	u32 instance);
int fimc_is_hw_f_param(struct fimc_is_interface *interface,
	u32 instance);
int fimc_is_hw_cfg_mem(struct fimc_is_interface *interface,
	u32 instance, u32 address, u32 size);
int fimc_is_hw_shot(struct fimc_is_interface *interface,
	u32 instance, u32 byaer, u32 shot, u32 frame);
int fimc_is_hw_power_down(struct fimc_is_interface *interface,
	u32 instance);

#endif
