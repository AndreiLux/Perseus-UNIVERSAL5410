/*
 * Universal Flash Storage Host controller driver
 *
 * This code is based on drivers/scsi/ufs/ufshcd.c
 * Copyright (C) 2011-2012 Samsung India Software Operations
 *
 * Santosh Yaraganavi <santosh.sy@samsung.com>
 * Vinayak Holikatti <h.vinayak@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * NO WARRANTY
 * THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT
 * LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is
 * solely responsible for determining the appropriateness of using and
 * distributing the Program and assumes all risks associated with its
 * exercise of rights under this Agreement, including but not limited to
 * the risks and costs of program errors, damage to or loss of data,
 * programs or equipment, and unavailability or interruption of operations.

 * DISCLAIMER OF LIABILITY
 * NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/bitops.h>

#include <asm/irq.h>
#include <asm/byteorder.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_dbg.h>

#include "ufs.h"
#include "ufshci.h"

#define UFSHCD "ufshcd"
#define UFSHCD_DRIVER_VERSION "0.1"

enum {
	UFSHCD_MAX_CHANNEL	= 0,
	UFSHCD_MAX_ID		= 1,
	UFSHCD_MAX_LUNS		= 8,
};

/* UFSHCD states */
enum {
	UFSHCD_STATE_OPERATIONAL,
	UFSHCD_STATE_RESET,
	UFSHCD_STATE_ERROR,
};

/* Interrupt configuration options */
enum {
	UFSHCD_INT_DISABLE,
	UFSHCD_INT_ENABLE,
	UFSHCD_INT_CLEAR,
};

/* Interrupt aggregation options */
enum {
	INT_AGGR_RESET,
	INT_AGGR_CONFIG,
};

/**
 * struct uic_command - UIC command structure
 * @command: UIC command
 * @argument1: UIC command argument 1
 * @argument2: UIC command argument 2
 * @argument3: UIC command argument 3
 * @cmd_active: Indicate if UIC command is outstanding
 * @result: UIC command result
 */
struct uic_command {
	u32 command;
	u32 argument1;
	u32 argument2;
	u32 argument3;
	int cmd_active;
	int result;
};

/**
 * struct ufs_hba - per adapter private structure
 * @mmio_base: UFSHCI base register address
 * @ucdl_base_addr: UFS Command Descriptor base address
 * @utrdl_base_addr: UTP Transfer Request Descriptor base address
 * @utmrdl_base_addr: UTP Task Management Descriptor base address
 * @ucdl_dma_addr: UFS Command Descriptor DMA address
 * @utrdl_dma_addr: UTRDL DMA address
 * @utmrdl_dma_addr: UTMRDL DMA address
 * @host: Scsi_Host instance of the driver
 * @pdev: PCI device handle
 * @lrb: local reference block
 * @capabilities: UFS Controller Capabilities
 * @nutrs: Transfer Request Queue depth supported by controller
 * @nutmrs: Task Management Queue depth supported by controller
 * @active_uic_cmd: handle of active UIC command
 * @ufshcd_state: UFSHCD states
 * @int_enable_mask: Interrupt Mask Bits
 * @uic_workq: Work queue for UIC completion handling
 */
struct ufs_hba {
	void __iomem *mmio_base;

	/* Virtual memory reference */
	struct utp_transfer_cmd_desc *ucdl_base_addr;
	struct utp_transfer_req_desc *utrdl_base_addr;
	struct utp_task_req_desc *utmrdl_base_addr;

	/* DMA memory reference */
	dma_addr_t ucdl_dma_addr;
	dma_addr_t utrdl_dma_addr;
	dma_addr_t utmrdl_dma_addr;

	struct Scsi_Host *host;
	struct pci_dev *pdev;

	struct ufshcd_lrb *lrb;

	u32 capabilities;
	int nutrs;
	int nutmrs;
	u32 ufs_version;

	struct uic_command active_uic_cmd;

	u32 ufshcd_state;
	u32 int_enable_mask;

	/* Work Queues */
	struct work_struct uic_workq;
};

/**
 * struct ufshcd_lrb - local reference block
 * @utr_descriptor_ptr: UTRD address of the command
 * @ucd_cmd_ptr: UCD address of the command
 * @ucd_rsp_ptr: Response UPIU address for this command
 * @ucd_prdt_ptr: PRDT address of the command
 */
struct ufshcd_lrb {
	struct utp_transfer_req_desc *utr_descriptor_ptr;
	struct utp_upiu_cmd *ucd_cmd_ptr;
	struct utp_upiu_rsp *ucd_rsp_ptr;
	struct ufshcd_sg_entry *ucd_prdt_ptr;
};

/**
 * ufshcd_get_ufs_version - Get the UFS version supported by the HBA
 * @hba - Pointer to adapter instance
 *
 * Returns UFSHCI version supported by the controller
 */
static inline u32 ufshcd_get_ufs_version(struct ufs_hba *hba)
{
	return readl(hba->mmio_base + REG_UFS_VERSION);
}

/**
 * ufshcd_is_device_present - Check if any device connected to
 *			      the host controller
 * @reg_hcs - host controller status register value
 *
 * Returns 0 if device present, non-zero if no device detected
 */
static inline int ufshcd_is_device_present(u32 reg_hcs)
{
	return (DEVICE_PRESENT & reg_hcs) ? 0 : -1;
}

/**
 * ufshcd_get_lists_status - Check UCRDY, UTRLRDY and UTMRLRDY
 * @reg: Register value of host controller status
 *
 * Returns integer, 0 on Success and positive value if failed
 */
static inline int ufshcd_get_lists_status(u32 reg)
{
	/*
	 * The mask 0xFF is for the following HCS register bits
	 * Bit		Description
	 *  0		Device Present
	 *  1		UTRLRDY
	 *  2		UTMRLRDY
	 *  3		UCRDY
	 *  4		HEI
	 *  5		DEI
	 * 6-7		reserved
	 */
	return (((reg) & (0xFF)) >> 1) ^ (0x07);
}

/**
 * ufshcd_get_uic_cmd_result - Get the UIC command result
 * @hba: Pointer to adapter instance
 *
 * This function gets the result of UIC command completion
 * Returns 0 on success, non zero value on error
 */
static inline int ufshcd_get_uic_cmd_result(struct ufs_hba *hba)
{
	return readl(hba->mmio_base + REG_UIC_COMMAND_ARG_2) &
	       MASK_UIC_COMMAND_RESULT;
}

/**
 * ufshcd_free_hba_memory - Free allocated memory for LRB, request
 *			    and task lists
 * @hba: Pointer to adapter instance
 */
static inline void ufshcd_free_hba_memory(struct ufs_hba *hba)
{
	size_t utmrdl_size, utrdl_size, ucdl_size;

	kfree(hba->lrb);

	if (hba->utmrdl_base_addr) {
		utmrdl_size = sizeof(struct utp_task_req_desc) * hba->nutmrs;
		dma_free_coherent(&hba->pdev->dev, utmrdl_size,
				  hba->utmrdl_base_addr, hba->utmrdl_dma_addr);
	}

	if (hba->utrdl_base_addr) {
		utrdl_size =
		(sizeof(struct utp_transfer_req_desc) * hba->nutrs);
		dma_free_coherent(&hba->pdev->dev, utrdl_size,
				  hba->utrdl_base_addr, hba->utrdl_dma_addr);
	}

	if (hba->ucdl_base_addr) {
		ucdl_size =
		(sizeof(struct utp_transfer_cmd_desc) * hba->nutrs);
		dma_free_coherent(&hba->pdev->dev, ucdl_size,
				  hba->ucdl_base_addr, hba->ucdl_dma_addr);
	}
}

/**
 * ufshcd_config_int_aggr - Configure interrupt aggregation values.
 *		Currently there is no use case where we want to configure
 *		interrupt aggregation dynamically. So to configure interrupt
 *		aggregation, #define INT_AGGR_COUNTER_THRESHOLD_VALUE and
 *		INT_AGGR_TIMEOUT_VALUE are used.
 * @hba: per adapter instance
 * @option: Interrupt aggregation option
 */
static inline void
ufshcd_config_int_aggr(struct ufs_hba *hba, int option)
{
	switch (option) {
	case INT_AGGR_RESET:
		writel((INT_AGGR_ENABLE |
			INT_AGGR_COUNTER_AND_TIMER_RESET),
			(hba->mmio_base +
			 REG_UTP_TRANSFER_REQ_INT_AGG_CONTROL));
		break;
	case INT_AGGR_CONFIG:
		writel((INT_AGGR_ENABLE |
			INT_AGGR_PARAM_WRITE |
			INT_AGGR_COUNTER_THRESHOLD_VALUE |
			INT_AGGR_TIMEOUT_VALUE),
			(hba->mmio_base +
			 REG_UTP_TRANSFER_REQ_INT_AGG_CONTROL));
		break;
	}
}

/**
 * ufshcd_enable_run_stop_reg - Enable run-stop registers,
 *			When run-stop registers are set to 1, it indicates the
 *			host controller that it can process the requests
 * @hba: per adapter instance
 */
static void ufshcd_enable_run_stop_reg(struct ufs_hba *hba)
{
	writel(UTP_TASK_REQ_LIST_RUN_STOP_BIT,
	       (hba->mmio_base +
		REG_UTP_TASK_REQ_LIST_RUN_STOP));
	writel(UTP_TRANSFER_REQ_LIST_RUN_STOP_BIT,
	       (hba->mmio_base +
		REG_UTP_TRANSFER_REQ_LIST_RUN_STOP));
}

/**
 * ufshcd_hba_stop - Send controller to reset state
 * @hba: per adapter instance
 */
static inline void ufshcd_hba_stop(struct ufs_hba *hba)
{
	writel(CONTROLLER_DISABLE, (hba->mmio_base + REG_CONTROLLER_ENABLE));
}

/**
 * ufshcd_hba_start - Start controller initialization sequence
 * @hba: per adapter instance
 */
static inline void ufshcd_hba_start(struct ufs_hba *hba)
{
	writel(CONTROLLER_ENABLE , (hba->mmio_base + REG_CONTROLLER_ENABLE));
}

/**
 * ufshcd_is_hba_active - Get controller state
 * @hba: per adapter instance
 *
 * Returns zero if controller is active, 1 otherwise
 */
static inline int ufshcd_is_hba_active(struct ufs_hba *hba)
{
	return (readl(hba->mmio_base + REG_CONTROLLER_ENABLE) & 0x1) ? 0 : 1;
}

/**
 * ufshcd_hba_capabilities - Read controller capabilities
 * @hba: per adapter instance
 */
static inline void ufshcd_hba_capabilities(struct ufs_hba *hba)
{
	hba->capabilities =
		readl(hba->mmio_base + REG_CONTROLLER_CAPABILITIES);

	/* nutrs and nutmrs are 0 based values */
	hba->nutrs = (hba->capabilities & MASK_TRANSFER_REQUESTS_SLOTS) + 1;
	hba->nutmrs =
	((hba->capabilities & MASK_TASK_MANAGEMENT_REQUEST_SLOTS) >> 16) + 1;
}

/**
 * ufshcd_send_uic_command - Send UIC commands to unipro layers
 * @hba: per adapter instance
 * @uic_command: UIC command
 */
static inline void
ufshcd_send_uic_command(struct ufs_hba *hba, struct uic_command *uic_cmnd)
{
	/* Write Args */
	writel(uic_cmnd->argument1,
	      (hba->mmio_base + REG_UIC_COMMAND_ARG_1));
	writel(uic_cmnd->argument2,
	      (hba->mmio_base + REG_UIC_COMMAND_ARG_2));
	writel(uic_cmnd->argument3,
	      (hba->mmio_base + REG_UIC_COMMAND_ARG_3));

	/* Write UIC Cmd */
	writel((uic_cmnd->command & COMMAND_OPCODE_MASK),
	       (hba->mmio_base + REG_UIC_COMMAND));
}

/**
 * ufshcd_int_config - enable/disable interrupts
 * @hba: per adapter instance
 * @option: interrupt option
 */
static void ufshcd_int_config(struct ufs_hba *hba, u32 option)
{
	switch (option) {
	case UFSHCD_INT_ENABLE:
		writel(hba->int_enable_mask,
		      (hba->mmio_base + REG_INTERRUPT_ENABLE));
		break;
	case UFSHCD_INT_DISABLE:
		if (hba->ufs_version == UFSHCI_VERSION_10)
			writel(INTERRUPT_DISABLE_MASK_10,
			      (hba->mmio_base + REG_INTERRUPT_ENABLE));
		else
			writel(INTERRUPT_DISABLE_MASK_11,
			       (hba->mmio_base + REG_INTERRUPT_ENABLE));
		break;
	}
}

/**
 * ufshcd_memory_alloc - allocate memory for host memory space data structures
 * @hba: per adapter instance
 *
 * 1. Allocate DMA memory for Command Descriptor array
 *	Each command descriptor consist of Command UPIU, Response UPIU and PRDT
 * 2. Allocate DMA memory for UTP Transfer Request Descriptor List (UTRDL).
 * 3. Allocate DMA memory for UTP Task Management Request Descriptor List
 *	(UTMRDL)
 * 4. Allocate memory for local reference block(lrb).
 *
 * Returns 0 for success, non-zero in case of failure
 */
static int ufshcd_memory_alloc(struct ufs_hba *hba)
{
	size_t utmrdl_size, utrdl_size, ucdl_size;

	/* Allocate memory for UTP command descriptors */
	ucdl_size = (sizeof(struct utp_transfer_cmd_desc) * hba->nutrs);
	hba->ucdl_base_addr = dma_alloc_coherent(&hba->pdev->dev,
						 ucdl_size,
						 &hba->ucdl_dma_addr,
						 GFP_KERNEL);

	/*
	 * UFSHCI requires UTP command descriptor to be 128 byte aligned.
	 * make sure hba->ucdl_dma_addr is aligned to PAGE_SIZE
	 * if hba->ucdl_dma_addr is aligned to PAGE_SIZE, then it will
	 * be aligned to 128 bytes as well
	 */
	if (!hba->ucdl_base_addr ||
	    WARN_ON(hba->ucdl_dma_addr & (PAGE_SIZE - 1))) {
		dev_err(&hba->pdev->dev,
			"Command Descriptor Memory allocation failed\n");
		goto out;
	}

	/*
	 * Allocate memory for UTP Transfer descriptors
	 * UFSHCI requires 1024 byte alignment of UTRD
	 */
	utrdl_size = (sizeof(struct utp_transfer_req_desc) * hba->nutrs);
	hba->utrdl_base_addr = dma_alloc_coherent(&hba->pdev->dev,
						  utrdl_size,
						  &hba->utrdl_dma_addr,
						  GFP_KERNEL);
	if (!hba->utrdl_base_addr ||
	    WARN_ON(hba->utrdl_dma_addr & (PAGE_SIZE - 1))) {
		dev_err(&hba->pdev->dev,
			"Transfer Descriptor Memory allocation failed\n");
		goto out;
	}

	/*
	 * Allocate memory for UTP Task Management descriptors
	 * UFSHCI requires 1024 byte alignment of UTMRD
	 */
	utmrdl_size = sizeof(struct utp_task_req_desc) * hba->nutmrs;
	hba->utmrdl_base_addr = dma_alloc_coherent(&hba->pdev->dev,
						   utmrdl_size,
						   &hba->utmrdl_dma_addr,
						   GFP_KERNEL);
	if (!hba->utmrdl_base_addr ||
	    WARN_ON(hba->utmrdl_dma_addr & (PAGE_SIZE - 1))) {
		dev_err(&hba->pdev->dev,
		"Task Management Descriptor Memory allocation failed\n");
		goto out;
	}

	/* Allocate memory for local reference block */
	hba->lrb = kcalloc(hba->nutrs, sizeof(struct ufshcd_lrb), GFP_KERNEL);
	if (!hba->lrb) {
		dev_err(&hba->pdev->dev, "LRB Memory allocation failed\n");
		goto out;
	}
	return 0;
out:
	ufshcd_free_hba_memory(hba);
	return -ENOMEM;
}

/**
 * ufshcd_host_memory_configure - configure local reference block with
 *				memory offsets
 * @hba: per adapter instance
 *
 * Configure Host memory space
 * 1. Update Corresponding UTRD.UCDBA and UTRD.UCDBAU with UCD DMA
 * address.
 * 2. Update each UTRD with Response UPIU offset, Response UPIU length
 * and PRDT offset.
 * 3. Save the corresponding addresses of UTRD, UCD.CMD, UCD.RSP and UCD.PRDT
 * into local reference block.
 */
static void ufshcd_host_memory_configure(struct ufs_hba *hba)
{
	struct utp_transfer_cmd_desc *cmd_descp;
	struct utp_transfer_req_desc *utrdlp;
	dma_addr_t cmd_desc_dma_addr;
	dma_addr_t cmd_desc_element_addr;
	u16 response_offset;
	u16 prdt_offset;
	int cmd_desc_size;
	int i;

	utrdlp = hba->utrdl_base_addr;
	cmd_descp = hba->ucdl_base_addr;

	response_offset =
		offsetof(struct utp_transfer_cmd_desc, response_upiu);
	prdt_offset =
		offsetof(struct utp_transfer_cmd_desc, prd_table);

	cmd_desc_size = sizeof(struct utp_transfer_cmd_desc);
	cmd_desc_dma_addr = hba->ucdl_dma_addr;

	for (i = 0; i < hba->nutrs; i++) {
		/* Configure UTRD with command descriptor base address */
		cmd_desc_element_addr =
				(cmd_desc_dma_addr + (cmd_desc_size * i));
		utrdlp[i].command_desc_base_addr_lo =
				cpu_to_le32(cmd_desc_element_addr);
		utrdlp[i].command_desc_base_addr_hi =
				cpu_to_le32(cmd_desc_element_addr >> 32);

		/* Response upiu and prdt offset should be in double words */
		utrdlp[i].response_upiu_offset =
				cpu_to_le16((response_offset >> 2));
		utrdlp[i].prd_table_offset =
				cpu_to_le16((prdt_offset >> 2));
		utrdlp[i].response_upiu_length =
				cpu_to_le16(ALIGNED_UPIU_SIZE);

		hba->lrb[i].utr_descriptor_ptr = (utrdlp + i);
		hba->lrb[i].ucd_cmd_ptr =
			(struct utp_upiu_cmd *)(cmd_descp + i);
		hba->lrb[i].ucd_rsp_ptr =
			(struct utp_upiu_rsp *)cmd_descp[i].response_upiu;
		hba->lrb[i].ucd_prdt_ptr =
			(struct ufshcd_sg_entry *)cmd_descp[i].prd_table;
	}
}

/**
 * ufshcd_dme_link_startup - Notify Unipro to perform link startup
 * @hba: per adapter instance
 *
 * UIC_CMD_DME_LINK_STARTUP command must be issued to Unipro layer,
 * in order to initialize the Unipro link startup procedure.
 * Once the Unipro links are up, the device connected to the controller
 * is detected.
 *
 * Returns 0 on success, non-zero value on failure
 */
static int ufshcd_dme_link_startup(struct ufs_hba *hba)
{
	struct uic_command *uic_cmd;
	unsigned long flags;

	/* check if controller is ready to accept UIC commands */
	if (((readl(hba->mmio_base + REG_CONTROLLER_STATUS)) &
	    UIC_COMMAND_READY) == 0x0) {
		dev_err(&hba->pdev->dev,
			"Controller not ready"
			" to accept UIC commands\n");
		return -EIO;
	}

	spin_lock_irqsave(hba->host->host_lock, flags);

	/* form UIC command */
	uic_cmd = &hba->active_uic_cmd;
	uic_cmd->command = UIC_CMD_DME_LINK_STARTUP;
	uic_cmd->argument1 = 0;
	uic_cmd->argument2 = 0;
	uic_cmd->argument3 = 0;

	/* enable UIC related interrupts */
	hba->int_enable_mask |= UIC_COMMAND_COMPL;
	ufshcd_int_config(hba, UFSHCD_INT_ENABLE);

	/* sending UIC commands to controller */
	ufshcd_send_uic_command(hba, uic_cmd);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return 0;
}

/**
 * ufshcd_make_hba_operational - Make UFS controller operational
 * @hba: per adapter instance
 *
 * To bring UFS host controller to operational state,
 * 1. Check if device is present
 * 2. Configure run-stop-registers
 * 3. Enable required interrupts
 * 4. Configure interrupt aggregation
 *
 * Returns 0 on success, non-zero value on failure
 */
static int ufshcd_make_hba_operational(struct ufs_hba *hba)
{
	int err = 0;
	u32 reg;

	/* check if device present */
	reg = readl((hba->mmio_base + REG_CONTROLLER_STATUS));
	if (ufshcd_is_device_present(reg)) {
		dev_err(&hba->pdev->dev, "cc: Device not present\n");
		err = -ENXIO;
		goto out;
	}

	/*
	 * UCRDY, UTMRLDY and UTRLRDY bits must be 1
	 * DEI, HEI bits must be 0
	 */
	if (!(ufshcd_get_lists_status(reg))) {
		ufshcd_enable_run_stop_reg(hba);
	} else {
		dev_err(&hba->pdev->dev,
			"Host controller not ready to process requests");
		err = -EIO;
		goto out;
	}

	/* Enable required interrupts */
	hba->int_enable_mask |= (UTP_TRANSFER_REQ_COMPL |
				 UIC_ERROR |
				 UTP_TASK_REQ_COMPL |
				 DEVICE_FATAL_ERROR |
				 CONTROLLER_FATAL_ERROR |
				 SYSTEM_BUS_FATAL_ERROR);
	ufshcd_int_config(hba, UFSHCD_INT_ENABLE);

	/* Configure interrupt aggregation */
	ufshcd_config_int_aggr(hba, INT_AGGR_CONFIG);

	hba->ufshcd_state = UFSHCD_STATE_OPERATIONAL;
out:
	return err;
}

/**
 * ufshcd_hba_enable - initialize the controller
 * @hba: per adapter instance
 *
 * The controller resets itself and controller firmware initialization
 * sequence kicks off. When controller is ready it will set
 * the Host Controller Enable bit to 1.
 *
 * Returns 0 on success, non-zero value on failure
 */
static int ufshcd_hba_enable(struct ufs_hba *hba)
{
	int retry;

	/*
	 * msleep of 1 and 5 used in this function might result in msleep(20),
	 * but it was necessary to send the UFS FPGA to reset mode during
	 * development and testing of this driver. msleep can be changed to
	 * mdelay and retry count can be reduced based on the controller.
	 */
	if (!ufshcd_is_hba_active(hba)) {

		/* change controller state to "reset state" */
		ufshcd_hba_stop(hba);

		/*
		 * This delay is based on the testing done with UFS host
		 * controller FPGA. The delay can be changed based on the
		 * host controller used.
		 */
		msleep(5);
	}

	/* start controller initialization sequence */
	ufshcd_hba_start(hba);

	/*
	 * To initialize a UFS host controller HCE bit must be set to 1.
	 * During initialization the HCE bit value changes from 1->0->1.
	 * When the host controller completes initialization sequence
	 * it sets the value of HCE bit to 1. The same HCE bit is read back
	 * to check if the controller has completed initialization sequence.
	 * So without this delay the value HCE = 1, set in the previous
	 * instruction might be read back.
	 * This delay can be changed based on the controller.
	 */
	msleep(1);

	/* wait for the host controller to complete initialization */
	retry = 10;
	while (ufshcd_is_hba_active(hba)) {
		if (retry) {
			retry--;
		} else {
			dev_err(&hba->pdev->dev,
				"Controller enable failed\n");
			return -EIO;
		}
		msleep(5);
	}
	return 0;
}

/**
 * ufshcd_initialize_hba - start the initialization process
 * @hba: per adapter instance
 *
 * 1. Enable the controller via ufshcd_hba_enable.
 * 2. Program the Transfer Request List Address with the starting address of
 * UTRDL.
 * 3. Program the Task Management Request List Address with starting address
 * of UTMRDL.
 *
 * Returns 0 on success, non-zero value on failure.
 */
static int ufshcd_initialize_hba(struct ufs_hba *hba)
{
	if (ufshcd_hba_enable(hba))
		return -EIO;

	/* Configure UTRL and UTMRL base address registers */
	writel(hba->utrdl_dma_addr,
	       (hba->mmio_base + REG_UTP_TRANSFER_REQ_LIST_BASE_L));
	writel((hba->utrdl_dma_addr >> 32),
	       (hba->mmio_base + REG_UTP_TRANSFER_REQ_LIST_BASE_H));
	writel(hba->utmrdl_dma_addr,
	       (hba->mmio_base + REG_UTP_TASK_REQ_LIST_BASE_L));
	writel((hba->utmrdl_dma_addr >> 32),
	       (hba->mmio_base + REG_UTP_TASK_REQ_LIST_BASE_H));

	/* Initialize unipro link startup procedure */
	return ufshcd_dme_link_startup(hba);
}

/**
 * ufshcd_uic_cc_handler - handle UIC command completion
 * @work: pointer to a work queue structure
 *
 * Returns 0 on success, non-zero value on failure
 */
static void ufshcd_uic_cc_handler (struct work_struct *work)
{
	struct ufs_hba *hba;

	hba = container_of(work, struct ufs_hba, uic_workq);

	if ((hba->active_uic_cmd.command == UIC_CMD_DME_LINK_STARTUP) &&
	    !(ufshcd_get_uic_cmd_result(hba))) {

		if (ufshcd_make_hba_operational(hba))
			dev_err(&hba->pdev->dev,
				"cc: hba not operational state\n");
		return;
	}
}

/**
 * ufshcd_sl_intr - Interrupt service routine
 * @hba: per adapter instance
 * @intr_status: contains interrupts generated by the controller
 */
static void ufshcd_sl_intr(struct ufs_hba *hba, u32 intr_status)
{
	if (intr_status & UIC_COMMAND_COMPL)
		schedule_work(&hba->uic_workq);
}

/**
 * ufshcd_intr - Main interrupt service routine
 * @irq: irq number
 * @__hba: pointer to adapter instance
 *
 * Returns IRQ_HANDLED - If interrupt is valid
 *		IRQ_NONE - If invalid interrupt
 */
static irqreturn_t ufshcd_intr(int irq, void *__hba)
{
	u32 intr_status;
	irqreturn_t retval = IRQ_NONE;
	struct ufs_hba *hba = __hba;

	spin_lock(hba->host->host_lock);
	intr_status = readl(hba->mmio_base + REG_INTERRUPT_STATUS);

	if (intr_status) {
		ufshcd_sl_intr(hba, intr_status);

		/* If UFSHCI 1.0 then clear interrupt status register */
		if (hba->ufs_version == UFSHCI_VERSION_10)
			writel(intr_status,
			       (hba->mmio_base + REG_INTERRUPT_STATUS));
		retval = IRQ_HANDLED;
	}
	spin_unlock(hba->host->host_lock);
	return retval;
}

static struct scsi_host_template ufshcd_driver_template = {
	.module			= THIS_MODULE,
	.name			= UFSHCD,
	.proc_name		= UFSHCD,
	.this_id		= -1,
};

/**
 * ufshcd_shutdown - main function to put the controller in reset state
 * @pdev: pointer to PCI device handle
 */
static void ufshcd_shutdown(struct pci_dev *pdev)
{
	ufshcd_hba_stop((struct ufs_hba *)pci_get_drvdata(pdev));
}

#ifdef CONFIG_PM
/**
 * ufshcd_suspend - suspend power management function
 * @pdev: pointer to PCI device handle
 * @state: power state
 *
 * Returns -ENOSYS
 */
static int ufshcd_suspend(struct pci_dev *pdev, pm_message_t state)
{
	/*
	 * TODO:
	 * 1. Block SCSI requests from SCSI midlayer
	 * 2. Change the internal driver state to non operational
	 * 3. Set UTRLRSR and UTMRLRSR bits to zero
	 * 4. Wait until outstanding commands are completed
	 * 5. Set HCE to zero to send the UFS host controller to reset state
	 */

	return -ENOSYS;
}

/**
 * ufshcd_resume - resume power management function
 * @pdev: pointer to PCI device handle
 *
 * Returns -ENOSYS
 */
static int ufshcd_resume(struct pci_dev *pdev)
{
	/*
	 * TODO:
	 * 1. Set HCE to 1, to start the UFS host controller
	 * initialization process
	 * 2. Set UTRLRSR and UTMRLRSR bits to 1
	 * 3. Change the internal driver state to operational
	 * 4. Unblock SCSI requests from SCSI midlayer
	 */

	return -ENOSYS;
}
#endif /* CONFIG_PM */

/**
 * ufshcd_hba_free - free allocated memory for
 *			host memory space data structures
 * @hba: per adapter instance
 */
static void ufshcd_hba_free(struct ufs_hba *hba)
{
	iounmap(hba->mmio_base);
	ufshcd_free_hba_memory(hba);
	pci_release_regions(hba->pdev);
}

/**
 * ufshcd_remove - de-allocate PCI/SCSI host and host memory space
 *		data structure memory
 * @pdev - pointer to PCI handle
 */
static void ufshcd_remove(struct pci_dev *pdev)
{
	struct ufs_hba *hba = pci_get_drvdata(pdev);

	/* disable interrupts */
	ufshcd_int_config(hba, UFSHCD_INT_DISABLE);
	free_irq(pdev->irq, hba);

	ufshcd_hba_stop(hba);
	ufshcd_hba_free(hba);

	scsi_remove_host(hba->host);
	scsi_host_put(hba->host);
	pci_set_drvdata(pdev, NULL);
	pci_clear_master(pdev);
	pci_disable_device(pdev);
}

/**
 * ufshcd_set_dma_mask - Set dma mask based on the controller
 *			 addressing capability
 * @pdev: PCI device structure
 *
 * Returns 0 for success, non-zero for failure
 */
static int ufshcd_set_dma_mask(struct ufs_hba *hba)
{
	int err;
	u64 dma_mask;

	/*
	 * If controller supports 64 bit addressing mode, then set the DMA
	 * mask to 64-bit, else set the DMA mask to 32-bit
	 */
	if (hba->capabilities & MASK_64_ADDRESSING_SUPPORT)
		dma_mask = DMA_BIT_MASK(64);
	else
		dma_mask = DMA_BIT_MASK(32);

	err = pci_set_dma_mask(hba->pdev, dma_mask);
	if (err)
		return err;

	err = pci_set_consistent_dma_mask(hba->pdev, dma_mask);

	return err;
}

/**
 * ufshcd_probe - probe routine of the driver
 * @pdev: pointer to PCI device handle
 * @id: PCI device id
 *
 * Returns 0 on success, non-zero value on failure
 */
static int __devinit
ufshcd_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct Scsi_Host *host;
	struct ufs_hba *hba;
	int err;

	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "pci_enable_device failed\n");
		goto out_error;
	}

	pci_set_master(pdev);

	host = scsi_host_alloc(&ufshcd_driver_template,
				sizeof(struct ufs_hba));
	if (!host) {
		dev_err(&pdev->dev, "scsi_host_alloc failed\n");
		err = -ENOMEM;
		goto out_disable;
	}
	hba = shost_priv(host);

	err = pci_request_regions(pdev, UFSHCD);
	if (err < 0) {
		dev_err(&pdev->dev, "request regions failed\n");
		goto out_disable;
	}

	hba->mmio_base = pci_ioremap_bar(pdev, 0);
	if (!hba->mmio_base) {
		dev_err(&pdev->dev, "memory map failed\n");
		err = -ENOMEM;
		goto out_release_regions;
	}

	hba->host = host;
	hba->pdev = pdev;

	/* Read capabilities registers */
	ufshcd_hba_capabilities(hba);

	/* Get UFS version supported by the controller */
	hba->ufs_version = ufshcd_get_ufs_version(hba);

	err = ufshcd_set_dma_mask(hba);
	if (err) {
		dev_err(&pdev->dev, "set dma mask failed\n");
		goto out_iounmap;
	}

	/* Allocate memory for host memory space */
	err = ufshcd_memory_alloc(hba);
	if (err) {
		dev_err(&pdev->dev, "Memory allocation failed\n");
		goto out_iounmap;
	}

	/* Configure LRB */
	ufshcd_host_memory_configure(hba);

	host->can_queue = hba->nutrs;
	host->max_id = UFSHCD_MAX_ID;
	host->max_lun = UFSHCD_MAX_LUNS;
	host->max_channel = UFSHCD_MAX_CHANNEL;
	host->unique_id = host->host_no;

	/* Initialize work queues */
	INIT_WORK(&hba->uic_workq, ufshcd_uic_cc_handler);

	/* IRQ registration */
	err = request_irq(pdev->irq, ufshcd_intr, IRQF_SHARED, UFSHCD, hba);
	if (err) {
		dev_err(&pdev->dev, "request irq failed\n");
		goto out_lrb_free;
	}

	pci_set_drvdata(pdev, hba);

	err = scsi_add_host(host, &pdev->dev);
	if (err) {
		dev_err(&pdev->dev, "scsi_add_host failed\n");
		goto out_free_irq;
	}

	/* Initialization routine */
	err = ufshcd_initialize_hba(hba);
	if (err) {
		dev_err(&pdev->dev, "Initialization failed\n");
		goto out_free_irq;
	}

	return 0;

out_free_irq:
	free_irq(pdev->irq, hba);
out_lrb_free:
	ufshcd_free_hba_memory(hba);
out_iounmap:
	iounmap(hba->mmio_base);
out_release_regions:
	pci_release_regions(pdev);
out_disable:
	scsi_host_put(host);
	pci_clear_master(pdev);
	pci_disable_device(pdev);
out_error:
	return err;
}

static DEFINE_PCI_DEVICE_TABLE(ufshcd_pci_tbl) = {
	{ PCI_VENDOR_ID_SAMSUNG, 0xC00C, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ }	/* terminate list */
};

MODULE_DEVICE_TABLE(pci, ufshcd_pci_tbl);

static struct pci_driver ufshcd_pci_driver = {
	.name = UFSHCD,
	.id_table = ufshcd_pci_tbl,
	.probe = ufshcd_probe,
	.remove = __devexit_p(ufshcd_remove),
	.shutdown = ufshcd_shutdown,
#ifdef CONFIG_PM
	.suspend = ufshcd_suspend,
	.resume = ufshcd_resume,
#endif
};

/**
 * ufshcd_init - Driver registration routine
 */
static int __init ufshcd_init(void)
{
	return pci_register_driver(&ufshcd_pci_driver);
}
module_init(ufshcd_init);

/**
 * ufshcd_exit - Driver exit clean-up routine
 */
static void __exit ufshcd_exit(void)
{
	pci_unregister_driver(&ufshcd_pci_driver);
}
module_exit(ufshcd_exit);


MODULE_AUTHOR("Santosh Yaragnavi <santosh.sy@samsung.com>, "
	      "Vinayak Holikatti <h.vinayak@samsung.com>");
MODULE_DESCRIPTION("Generic UFS host controller driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(UFSHCD_DRIVER_VERSION);
