/*
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MODEM_LINK_DEVICE_DPRAM_H__
#define __MODEM_LINK_DEVICE_DPRAM_H__

#include "modem_link_device_memory.h"

/*
	magic_code +
	access_enable +
	fmt_tx_head + fmt_tx_tail + fmt_tx_buff +
	raw_tx_head + raw_tx_tail + raw_tx_buff +
	fmt_rx_head + fmt_rx_tail + fmt_rx_buff +
	raw_rx_head + raw_rx_tail + raw_rx_buff +
	mbx_cp2ap +
	mbx_ap2cp
 =	2 +
	2 +
	2 + 2 + 1336 +
	2 + 2 + 4564 +
	2 + 2 + 1336 +
	2 + 2 + 9124 +
	2 +
	2
 =	16384
*/
#define DP_16K_FMT_TX_BUFF_SZ	1336
#define DP_16K_RAW_TX_BUFF_SZ	4564
#define DP_16K_FMT_RX_BUFF_SZ	1336
#define DP_16K_RAW_RX_BUFF_SZ	9124

struct dpram_ipc_16k_map {
	u16 magic;
	u16 access;

	u16 fmt_tx_head;
	u16 fmt_tx_tail;
	u8  fmt_tx_buff[DP_16K_FMT_TX_BUFF_SZ];

	u16 raw_tx_head;
	u16 raw_tx_tail;
	u8  raw_tx_buff[DP_16K_RAW_TX_BUFF_SZ];

	u16 fmt_rx_head;
	u16 fmt_rx_tail;
	u8  fmt_rx_buff[DP_16K_FMT_RX_BUFF_SZ];

	u16 raw_rx_head;
	u16 raw_rx_tail;
	u8  raw_rx_buff[DP_16K_RAW_RX_BUFF_SZ];

	u16 mbx_cp2ap;
	u16 mbx_ap2cp;
};

enum idpram_link_pm_states {
	IDPRAM_PM_SUSPEND_PREPARE,
	IDPRAM_PM_DPRAM_POWER_DOWN,
	IDPRAM_PM_SUSPEND_START,
	IDPRAM_PM_RESUME_START,
	IDPRAM_PM_ACTIVE,
};

struct idpram_pm_data {
	atomic_t pm_lock;

	enum idpram_link_pm_states pm_state;

	struct completion down_cmpl;

	struct wake_lock ap_wlock;
	struct wake_lock hold_wlock;

	struct delayed_work tx_dwork;
	struct delayed_work resume_dwork;

	struct notifier_block pm_noti;

	unsigned resume_try_cnt;

	/* the last value in the mbx_cp2ap */
	unsigned last_msg;
};

struct dpram_link_device;
struct dpram_ext_op;
struct idpram_pm_op;

struct dpram_link_device {
	struct link_device ld;

	/* DPRAM type */
	enum dpram_type type;
	enum ap_type ap;	/* AP type for AP_IDPRAM */

	/* DPRAM address and size */
	u8 __iomem *base;	/* Virtual address of DPRAM	*/
	u32 size;		/* DPRAM size			*/

	/* DPRAM SFR */
	u8 __iomem *sfr_base;	/* Virtual address of SFR	*/

	/* Whether or not this DPRAM can go asleep */
	bool need_wake_up;

	/* DPRAM IRQ GPIO# */
	unsigned gpio_int2ap;
	unsigned gpio_cp_status;
	unsigned gpio_cp_wakeup;
	unsigned gpio_int2cp;

	/* DPRAM IRQ from CP */
	int irq;
	unsigned long irq_flags;
	char irq_name[MIF_MAX_NAME_LEN];

	/* Link to DPRAM control functions dependent on each platform */
	struct modemlink_dpram_data *dpram;

	/* Physical configuration -> logical configuration */
	struct memif_boot_map bt_map;
	struct memif_dload_map dl_map;
	struct memif_uload_map ul_map;

	/* IPC device map */
	struct dpram_ipc_map ipc_map;

	/* Pointers (aliases) to IPC device map */
	u16 __iomem *magic;
	u16 __iomem *access;
	struct dpram_ipc_device *dev[MAX_IPC_DEV];
	u16 __iomem *mbx2ap;
	u16 __iomem *mbx2cp;

	/* Wakelock for DPRAM device */
	struct wake_lock wlock;
	char wlock_name[MIF_MAX_NAME_LEN];

	/* For booting */
	unsigned boot_start_complete;

	/* For UDL */
	struct tasklet_struct ul_tsk;
	struct tasklet_struct dl_tsk;
	struct completion udl_cmpl;

	/*
	** For CP crash dump
	*/
	bool forced_cp_crash;
	struct timer_list crash_ack_timer;
	struct timer_list crash_timer;
	struct completion crash_cmpl;
	/* If this field is wanted to be used, it must be initialized only in
	 * the "ld->dump_start" method.
	 */
	struct delayed_work crash_dwork;
	/* Count of CP crash dump packets received */
	int crash_rcvd;

	/* For locking TX process */
	spinlock_t tx_lock[MAX_IPC_DEV];

	/* For retransmission under DPRAM flow control after TXQ full state */
	unsigned long res_ack_wait_timeout;
	atomic_t res_required[MAX_IPC_DEV];
	struct completion req_ack_cmpl[MAX_IPC_DEV];

	/* For efficient RX process */
	struct tasklet_struct rx_tsk;
	struct mif_rxb_queue rxbq[MAX_IPC_DEV];
	struct io_device *iod[MAX_IPC_DEV];
	bool rx_with_skb;

	/* For wake-up/sleep control */
	atomic_t accessing;

	/* Multi-purpose miscellaneous buffer */
	u8 *buff;

	/* DPRAM IPC initialization status */
	int init_status;

	/* Alias to device-specific IOCTL function */
	int (*ext_ioctl)(struct dpram_link_device *dpld, struct io_device *iod,
			unsigned int cmd, unsigned long arg);

	/* Alias to DPRAM IRQ handler */
	irqreturn_t (*irq_handler)(int irq, void *data);

	/* For logging DPRAM status */
	struct mem_stat_queue stat_list;

	/* For DPRAM dump */
	void (*dpram_dump)(struct link_device *ld, char *buff);
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	char dump_path[MIF_MAX_PATH_LEN];
	char trace_path[MIF_MAX_PATH_LEN];
	struct trace_queue dump_list;
	struct trace_queue trace_list;
	struct delayed_work dump_dwork;
	struct delayed_work trace_dwork;
#endif

	/* Common operations for each DPRAM */
	u16 (*recv_intr)(struct dpram_link_device *dpld);
	void (*send_intr)(struct dpram_link_device *dpld, u16 mask);
	u16 (*get_magic)(struct dpram_link_device *dpld);
	void (*set_magic)(struct dpram_link_device *dpld, u16 value);
	u16 (*get_access)(struct dpram_link_device *dpld);
	void (*set_access)(struct dpram_link_device *dpld, u16 value);
	u32 (*get_txq_head)(struct dpram_link_device *dpld, int id);
	u32 (*get_txq_tail)(struct dpram_link_device *dpld, int id);
	void (*set_txq_head)(struct dpram_link_device *dpld, int id, u32 in);
	void (*set_txq_tail)(struct dpram_link_device *dpld, int id, u32 out);
	u8 *(*get_txq_buff)(struct dpram_link_device *dpld, int id);
	u32 (*get_txq_buff_size)(struct dpram_link_device *dpld, int id);
	u32 (*get_rxq_head)(struct dpram_link_device *dpld, int id);
	u32 (*get_rxq_tail)(struct dpram_link_device *dpld, int id);
	void (*set_rxq_head)(struct dpram_link_device *dpld, int id, u32 in);
	void (*set_rxq_tail)(struct dpram_link_device *dpld, int id, u32 out);
	u8 *(*get_rxq_buff)(struct dpram_link_device *dpld, int id);
	u32 (*get_rxq_buff_size)(struct dpram_link_device *dpld, int id);
	u16 (*get_mask_req_ack)(struct dpram_link_device *dpld, int id);
	u16 (*get_mask_res_ack)(struct dpram_link_device *dpld, int id);
	u16 (*get_mask_send)(struct dpram_link_device *dpld, int id);
	void (*get_dpram_status)(struct dpram_link_device *dpld,
			enum circ_dir_type, struct mem_status *stat);
	void (*ipc_rx_handler)(struct dpram_link_device *dpld,
			struct mem_status *stat);
	void (*reset_dpram_ipc)(struct dpram_link_device *dpld);

	/* Extended operations for various modems */
	struct dpram_ext_op *ext_op;

	/* Power management (PM) for AP_IDPRAM */
	struct idpram_pm_data pm_data;
	struct idpram_pm_op *pm_op;
};

/* converts from struct link_device* to struct xxx_link_device* */
#define to_dpram_link_device(linkdev) \
		container_of(linkdev, struct dpram_link_device, ld)

struct dpram_ext_op {
	/* flag for checking whether or not a dpram_ext_op instance exists */
	int exist;

	/* methods for setting up DPRAM maps */
	void (*init_boot_map)(struct dpram_link_device *dpld);
	void (*init_dl_map)(struct dpram_link_device *dpld);
	void (*init_ul_map)(struct dpram_link_device *dpld);
	void (*init_ipc_map)(struct dpram_link_device *dpld);

	/* methods for CP booting */
	int (*xmit_boot)(struct dpram_link_device *dpld, unsigned long arg);
	int (*xmit_binary)(struct dpram_link_device *dpld, struct sk_buff *skb);

	/* methods for DPRAM command handling */
	void (*cmd_handler)(struct dpram_link_device *dpld, u16 cmd);
	void (*cp_start_handler)(struct dpram_link_device *dpld);

	/* method for CP firmware upgrade */
	int (*firm_update)(struct dpram_link_device *dpld, unsigned long arg);

	/* methods for CP crash dump */
	void (*crash_log)(struct dpram_link_device *dpld);
	int (*dump_start)(struct dpram_link_device *dpld);
	int (*dump_update)(struct dpram_link_device *dpld, unsigned long arg);
	int (*dump_finish)(struct dpram_link_device *dpld, unsigned long arg);

	/* IOCTL extension */
	int (*ioctl)(struct dpram_link_device *dpld, struct io_device *iod,
			unsigned int cmd, unsigned long arg);

	/* methods for interrupt handling */
	irqreturn_t (*irq_handler)(int irq, void *data);
	void (*clear_int2ap)(struct dpram_link_device *dpld);

	/* methods for power management */
	int (*wakeup)(struct dpram_link_device *dpld);
	void (*sleep)(struct dpram_link_device *dpld);
};

struct dpram_ext_op *dpram_get_ext_op(enum modem_t modem);

struct idpram_pm_op {
	/* flag for checking whether or not a idpram_pm_op instance exists */
	int exist;
	int (*pm_init)(struct dpram_link_device *dpld, struct modem_data *modem,
			void (*pm_tx_func)(struct work_struct *work));
	void (*power_down)(struct dpram_link_device *dpld);
	void (*power_up)(struct dpram_link_device *dpld);
	void (*halt_suspend)(struct dpram_link_device *dpld);
	bool (*locked)(struct dpram_link_device *dpld);
	bool (*int2cp_possible)(struct dpram_link_device *dpld);
};

struct idpram_pm_op *idpram_get_pm_op(enum ap_type id);

#endif

