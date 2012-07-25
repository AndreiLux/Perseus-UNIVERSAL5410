#ifndef FIMC_IS_INTERFACE_H
#define FIMC_IS_INTERFACE_H

#include "fimc-is-metadata.h"
#include "fimc-is-framemgr.h"
#include "fimc-is-video.h"

/*#define TRACE_WORK*/
/* cam_ctrl : 1
   shot :     2 */
#define TRACE_WORK_ID 0xFF

#define MAX_NBLOCKING_COUNT 3
#define MAX_WORK_COUNT 10

#define TRY_RECV_AWARE_COUNT 100

#define LOWBIT_OF(num)	(num >= 32 ? 0 : (u32)1<<num)
#define HIGHBIT_OF(num)	(num >= 32 ? (u32)1<<(num-32) : 0)

enum fimc_is_interface_state {
	IS_IF_STATE_IDLE,
	IS_IF_STATE_BLOCK_IO,
	IS_IF_STATE_NBLOCK_IO
};

struct fimc_is_msg {
	u32	id;
	u32	command;
	u32	instance;
	u32	parameter1;
	u32	parameter2;
	u32	parameter3;
	u32	parameter4;
};

struct fimc_is_work {
	struct list_head		list;
	struct fimc_is_msg		msg;
};

struct fimc_is_work_list {
	u32				id;
	struct fimc_is_work		work[MAX_WORK_COUNT];
	spinlock_t			slock_free;
	spinlock_t			slock_request;
	struct list_head		work_free_head;
	u32				work_free_cnt;
	struct list_head		work_request_head;
	u32				work_request_cnt;
};

struct fimc_is_interface {
	void __iomem			*regs;
	struct is_common_reg __iomem	*com_regs;
	enum fimc_is_interface_state	state;
	spinlock_t			process_barrier;
	struct mutex			request_barrier;
	struct mutex			state_barrier;

	wait_queue_head_t		wait_queue;
	struct fimc_is_msg		reply;


	struct work_struct		work_queue[INTR_MAX_MAP];
	struct fimc_is_work_list	work_list[INTR_MAX_MAP];

	u32				fcount;
	bool				streaming;

	struct fimc_is_framemgr		*framemgr;

	void				*core;
	struct fimc_is_video_common	*video_sensor;
	struct fimc_is_video_common	*video_isp;
	struct fimc_is_video_common	*video_scc;
	struct fimc_is_video_common	*video_scp;

	struct fimc_is_work_list	nblk_cam_ctrl;
	struct fimc_is_work_list	nblk_shot;

	struct camera2_uctl		isp_peri_ctl;
};

int fimc_is_interface_probe(struct fimc_is_interface *this,
	struct fimc_is_framemgr *framemgr,
	u32 regs,
	u32 irq,
	void *core_data);
int fimc_is_interface_open(struct fimc_is_interface *this);
int fimc_is_interface_close(struct fimc_is_interface *this);

int fimc_is_hw_enum(struct fimc_is_interface *interface,
	u32 instances);
int fimc_is_hw_open(struct fimc_is_interface *interface,
	u32 instance, u32 sensor, u32 channel, u32 *mwidth, u32 *mheight);
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
int fimc_is_hw_g_capability(struct fimc_is_interface *this,
	u32 instance, u32 address);
int fimc_is_hw_cfg_mem(struct fimc_is_interface *interface,
	u32 instance, u32 address, u32 size);
int fimc_is_hw_power_down(struct fimc_is_interface *interface,
	u32 instance);

int fimc_is_hw_shot_nblk(struct fimc_is_interface *this,
	u32 instance, u32 byaer, u32 shot, u32 frame);
int fimc_is_hw_s_camctrl_nblk(struct fimc_is_interface *this,
	u32 instance, u32 address, u32 fnumber);

#endif

