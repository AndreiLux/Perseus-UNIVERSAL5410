/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_kbase_device.c
 * Base kernel device APIs
 */

#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_kbase_defs.h>
#include <kbase/src/common/mali_kbase_hw.h>

/* NOTE: Magic - 0x45435254 (TRCE in ASCII).
 * Supports tracing feature provided in the base module.
 * Please keep it in sync with the value of base module.
 */
#define TRACE_BUFFER_HEADER_SPECIAL 0x45435254

#ifdef MALI_PLATFORM_CONFIG_VEXPRESS
#if (MALI_BACKEND_KERNEL && (!MALI_LICENSE_IS_GPL || MALI_FAKE_PLATFORM_DEVICE))
extern kbase_attribute config_attributes_hw_issue_8408[];
#endif /* (MALI_BACKEND_KERNEL && (!MALI_LICENSE_IS_GPL || MALI_FAKE_PLATFORM_DEVICE)) */
#endif /* MALI_PLATFORM_CONFIG_VEXPRESS */

/* This array is referenced at compile time, it cannot be made static... */
const kbase_device_info kbase_dev_info[] = {
	{
		KBASE_MALI_T6XM,
		(KBASE_FEATURE_HAS_MODEL_PMU)
	},
	{
		KBASE_MALI_T6F1,
		(KBASE_FEATURE_NEEDS_REG_DELAY |
		 KBASE_FEATURE_DELAYED_PERF_WRITE_STATUS |
		 KBASE_FEATURE_HAS_16BIT_PC)
	},
	{
		KBASE_MALI_T601, 0
	},
	{
		KBASE_MALI_T604, 0
	},
	{
		KBASE_MALI_T608, 0
	},
};

#if KBASE_TRACE_ENABLE != 0
STATIC CONST char *kbasep_trace_code_string[] =
{
	/* IMPORTANT: USE OF SPECIAL #INCLUDE OF NON-STANDARD HEADER FILE
	 * THIS MUST BE USED AT THE START OF THE ARRAY */
#define KBASE_TRACE_CODE_MAKE_CODE( X ) # X
#include "mali_kbase_trace_defs.h"
#undef  KBASE_TRACE_CODE_MAKE_CODE
};
#endif

STATIC mali_error kbasep_trace_init( kbase_device *kbdev );
STATIC void kbasep_trace_term( kbase_device *kbdev );
STATIC void kbasep_trace_hook_wrapper( void *param );

void kbasep_as_do_poke(osk_workq_work * work);
void kbasep_reset_timer_callback(void *data);
void kbasep_reset_timeout_worker(osk_workq_work *data);

kbase_device *kbase_device_alloc(void)
{
	return osk_calloc(sizeof(kbase_device));
}

mali_error kbase_device_init(kbase_device *kbdev, const kbase_device_info *dev_info)
{
	osk_error osk_err;
	int i; /* i used after the for loop, don't reuse ! */

	kbdev->dev_info = dev_info;

	osk_err = osk_spinlock_irq_init(&kbdev->mmu_mask_change, OSK_LOCK_ORDER_MMU_MASK);
	if (OSK_ERR_NONE != osk_err)
	{
		goto fail;
	}

	/* Initialize platform specific context */
	if(MALI_FALSE == kbasep_platform_device_init(kbdev))
	{
		goto free_mmu_lock;
	}

	/* Ensure we can access the GPU registers */
	kbase_pm_register_access_enable(kbdev);

	/* Get the list of workarounds for issues on the current HW (identified by the GPU_ID register) */
	if (MALI_ERROR_NONE != kbase_hw_set_issues_mask(kbdev))
	{
		kbase_pm_register_access_disable(kbdev);
		goto free_platform;
	}

	/* Find out GPU properties based on the GPU feature registers */
	kbase_gpuprops_set(kbdev);

	kbdev->nr_hw_address_spaces = kbdev->gpu_props.num_address_spaces;

	/* We're done accessing the GPU registers for now. */
	kbase_pm_register_access_disable(kbdev);

	for (i = 0; i < kbdev->nr_hw_address_spaces; i++)
	{
		const char format[] = "mali_mmu%d";
		char name[sizeof(format)];
		const char poke_format[] = "mali_mmu%d_poker";	/* BASE_HW_ISSUE_8316 */
		char poke_name[sizeof(poke_format)]; /* BASE_HW_ISSUE_8316 */

		if (kbase_hw_has_issue(kbdev, BASE_HW_ISSUE_8316))
		{
			if (0 > osk_snprintf(poke_name, sizeof(poke_name), poke_format, i))
			{
				goto free_workqs;
			}
		}

		if (0 > osk_snprintf(name, sizeof(name), format, i))
		{
			goto free_workqs;
		}

		kbdev->as[i].number = i;
		kbdev->as[i].fault_addr = 0ULL;
		osk_err = osk_workq_init(&kbdev->as[i].pf_wq, name, 0);
		if (OSK_ERR_NONE != osk_err)
		{
			goto free_workqs;
		}
		osk_err = osk_mutex_init(&kbdev->as[i].transaction_mutex, OSK_LOCK_ORDER_AS);
		if (OSK_ERR_NONE != osk_err)
		{
			osk_workq_term(&kbdev->as[i].pf_wq);
			goto free_workqs;
		}

		if (kbase_hw_has_issue(kbdev, BASE_HW_ISSUE_8316))
		{
			osk_err = osk_workq_init(&kbdev->as[i].poke_wq, poke_name, 0);
			if (OSK_ERR_NONE != osk_err)
			{
				osk_workq_term(&kbdev->as[i].pf_wq);
				osk_mutex_term(&kbdev->as[i].transaction_mutex);
				goto free_workqs;
			}
			osk_workq_work_init(&kbdev->as[i].poke_work, kbasep_as_do_poke);
			osk_err = osk_timer_init(&kbdev->as[i].poke_timer);
			if (OSK_ERR_NONE != osk_err)
			{
				osk_workq_term(&kbdev->as[i].poke_wq);
				osk_workq_term(&kbdev->as[i].pf_wq);
				osk_mutex_term(&kbdev->as[i].transaction_mutex);
				goto free_workqs;
			}
			osk_timer_callback_set(&kbdev->as[i].poke_timer, kbasep_as_poke_timer_callback , &kbdev->as[i]);
			osk_atomic_set(&kbdev->as[i].poke_refcount, 0);
		}
	}
	/* don't change i after this point */

	osk_err = osk_spinlock_irq_init(&kbdev->hwcnt.lock, OSK_LOCK_ORDER_HWCNT);
	if (OSK_ERR_NONE != osk_err)
	{
		goto free_workqs;
	}

	kbdev->hwcnt.state = KBASE_INSTR_STATE_DISABLED;
	osk_err = osk_waitq_init(&kbdev->hwcnt.waitqueue);
	if (OSK_ERR_NONE != osk_err)
	{
		goto free_hwcnt_lock;
	}

	if (OSK_ERR_NONE != osk_workq_init(&kbdev->reset_workq, "Mali reset workqueue", 0))
	{
		goto free_hwcnt_waitq;
	}

	osk_workq_work_init(&kbdev->reset_work, kbasep_reset_timeout_worker);

	osk_err = osk_waitq_init(&kbdev->reset_waitq);
	if (OSK_ERR_NONE != osk_err)
	{
		goto free_reset_workq;
	}

	osk_err = osk_timer_init(&kbdev->reset_timer);
	if (OSK_ERR_NONE != osk_err)
	{
		goto free_reset_waitq;
	}
	osk_timer_callback_set(&kbdev->reset_timer, kbasep_reset_timer_callback, kbdev);

	if ( kbasep_trace_init( kbdev ) != MALI_ERROR_NONE )
	{
		goto free_reset_timer;
	}

#if KBASE_TRACE_ENABLE != 0
	osk_debug_assert_register_hook( &kbasep_trace_hook_wrapper, kbdev );
#endif

#ifdef MALI_PLATFORM_CONFIG_VEXPRESS
#if (MALI_BACKEND_KERNEL && (!MALI_LICENSE_IS_GPL || MALI_FAKE_PLATFORM_DEVICE))
	if (kbase_hw_has_issue(kbdev, BASE_HW_ISSUE_8408))
	{
		/* BASE_HW_ISSUE_8408 requires a configuration with different timeouts for
                 * the vexpress platform */
		kbdev->config_attributes = config_attributes_hw_issue_8408;
	}
#endif /* (MALI_BACKEND_KERNEL && (!MALI_LICENSE_IS_GPL || MALI_FAKE_PLATFORM_DEVICE)) */
#endif /* MALI_PLATFORM_CONFIG=vexpress */

	return MALI_ERROR_NONE;

free_reset_timer:
	osk_timer_term(&kbdev->reset_timer);
free_reset_waitq:
	osk_waitq_term(&kbdev->reset_waitq);
free_reset_workq:
	osk_workq_term(&kbdev->reset_workq);
free_hwcnt_waitq:
	osk_waitq_term(&kbdev->hwcnt.waitqueue);
free_hwcnt_lock:
	osk_spinlock_irq_term(&kbdev->hwcnt.lock);
free_workqs:
	while (i > 0)
	{
		i--;
		osk_mutex_term(&kbdev->as[i].transaction_mutex);
		osk_workq_term(&kbdev->as[i].pf_wq);
		if (kbase_hw_has_issue(kbdev, BASE_HW_ISSUE_8316))
		{
			osk_workq_term(&kbdev->as[i].poke_wq);
			osk_timer_term(&kbdev->as[i].poke_timer);
		}
	}
free_platform:
	kbasep_platform_device_term(kbdev);
free_mmu_lock:
	osk_spinlock_irq_term(&kbdev->mmu_mask_change);
fail:
	return MALI_ERROR_FUNCTION_FAILED;
}

void kbase_device_term(kbase_device *kbdev)
{
	int i;

#if KBASE_TRACE_ENABLE != 0
	osk_debug_assert_register_hook( NULL, NULL );
#endif

	kbasep_trace_term( kbdev );

	osk_timer_term(&kbdev->reset_timer);
	osk_waitq_term(&kbdev->reset_waitq);
	osk_workq_term(&kbdev->reset_workq);

	for (i = 0; i < kbdev->nr_hw_address_spaces; i++)
	{
		osk_mutex_term(&kbdev->as[i].transaction_mutex);
		osk_workq_term(&kbdev->as[i].pf_wq);
		if (kbase_hw_has_issue(kbdev, BASE_HW_ISSUE_8316))
		{
			osk_timer_term(&kbdev->as[i].poke_timer);
			osk_workq_term(&kbdev->as[i].poke_wq);
		}
	}

	kbasep_platform_device_term(kbdev);

	osk_spinlock_irq_term(&kbdev->hwcnt.lock);
	osk_waitq_term(&kbdev->hwcnt.waitqueue);
}

void kbase_device_free(kbase_device *kbdev)
{
	osk_free(kbdev);
}

int kbase_device_has_feature(kbase_device *kbdev, u32 feature)
{
	return !!(kbdev->dev_info->features & feature);
}
KBASE_EXPORT_TEST_API(kbase_device_has_feature)

kbase_midgard_type kbase_device_get_type(kbase_device *kbdev)
{
	return kbdev->dev_info->dev_type;
}
KBASE_EXPORT_TEST_API(kbase_device_get_type)

void kbase_device_trace_buffer_install(kbase_context * kctx, u32 * tb, size_t size)
{
	OSK_ASSERT(kctx);
	OSK_ASSERT(tb);

	/* set up the header */
	/* magic number in the first 4 bytes */
	tb[0] = TRACE_BUFFER_HEADER_SPECIAL;
	/* Store (write offset = 0, wrap counter = 0, transaction active = no)
	 * write offset 0 means never written.
	 * Offsets 1 to (wrap_offset - 1) used to store values when trace started
	 */
	tb[1] = 0;

	/* install trace buffer */
	osk_spinlock_irq_lock(&kctx->jctx.tb_lock);
	kctx->jctx.tb_wrap_offset = size / 8;
	kctx->jctx.tb = tb;
	osk_spinlock_irq_unlock(&kctx->jctx.tb_lock);
}

void kbase_device_trace_buffer_uninstall(kbase_context * kctx)
{
	OSK_ASSERT(kctx);
	osk_spinlock_irq_lock(&kctx->jctx.tb_lock);
	kctx->jctx.tb = NULL;
	kctx->jctx.tb_wrap_offset = 0;
	osk_spinlock_irq_unlock(&kctx->jctx.tb_lock);
}

void kbase_device_trace_register_access(kbase_context * kctx, kbase_reg_access_type type, u16 reg_offset, u32 reg_value)
{
	osk_spinlock_irq_lock(&kctx->jctx.tb_lock);
	if (kctx->jctx.tb)
	{
		u16 wrap_count;
		u16 write_offset;
		osk_atomic dummy; /* osk_atomic_set called to use memory barriers until OSK get's them */
		u32 * tb = kctx->jctx.tb;
		u32 header_word;

		header_word = tb[1];
		OSK_ASSERT(0 == (header_word & 0x1));

		wrap_count = (header_word >> 1) & 0x7FFF;
		write_offset = (header_word >> 16) & 0xFFFF;

		/* mark as transaction in progress */
		tb[1] |= 0x1;
		osk_atomic_set(&dummy, 1);

		/* calculate new offset */
		write_offset++;
		if (write_offset == kctx->jctx.tb_wrap_offset)
		{
			/* wrap */
			write_offset = 1;
			wrap_count++;
			wrap_count &= 0x7FFF; /* 15bit wrap counter */
		}

		/* store the trace entry at the selected offset */
		tb[write_offset * 2 + 0] = (reg_offset & ~0x3) | ((type == REG_WRITE) ? 0x1 : 0x0);
		tb[write_offset * 2 + 1] = reg_value;

		osk_atomic_set(&dummy, 1);

		/* new header word */
		header_word = (write_offset << 16) | (wrap_count << 1) | 0x0; /* transaction complete */
		tb[1] = header_word;
	}
	osk_spinlock_irq_unlock(&kctx->jctx.tb_lock);
}

void kbase_reg_write(kbase_device *kbdev, u16 offset, u32 value, kbase_context * kctx)
{
	OSK_ASSERT(kbdev->pm.gpu_powered);
	OSK_ASSERT(kctx==NULL || kctx->as_nr != KBASEP_AS_NR_INVALID);
	OSK_PRINT_INFO(OSK_BASE_CORE, "w: reg %04x val %08x", offset, value);
	kbase_os_reg_write(kbdev, offset, value);
	if (kctx && kctx->jctx.tb) kbase_device_trace_register_access(kctx, REG_WRITE, offset, value);
}
KBASE_EXPORT_TEST_API(kbase_reg_write)

u32 kbase_reg_read(kbase_device *kbdev, u16 offset, kbase_context * kctx)
{
	u32 val;
	OSK_ASSERT(kbdev->pm.gpu_powered);
	OSK_ASSERT(kctx==NULL || kctx->as_nr != KBASEP_AS_NR_INVALID);
	val = kbase_os_reg_read(kbdev, offset);
	OSK_PRINT_INFO(OSK_BASE_CORE, "r: reg %04x val %08x", offset, val);
	if (kctx && kctx->jctx.tb) kbase_device_trace_register_access(kctx, REG_READ, offset, val);
	return val;
}
KBASE_EXPORT_TEST_API(kbase_reg_read)

void kbase_report_gpu_fault(kbase_device *kbdev, int multiple)
{
	u32 status;
	u64 address;

	status = kbase_reg_read(kbdev, GPU_CONTROL_REG(GPU_FAULTSTATUS), NULL);
	address = (u64)kbase_reg_read(kbdev, GPU_CONTROL_REG(GPU_FAULTADDRESS_HI), NULL) << 32;
	address |= kbase_reg_read(kbdev, GPU_CONTROL_REG(GPU_FAULTADDRESS_LO), NULL);

	OSK_PRINT_WARN(OSK_BASE_CORE, "GPU Fault 0x08%x (%s) at 0x%016llx", status, kbase_exception_name(status), address);
	if (multiple)
	{
		OSK_PRINT_WARN(OSK_BASE_CORE, "There were multiple GPU faults - some have not been reported\n");
	}
}

void kbase_gpu_interrupt(kbase_device * kbdev, u32 val)
{
	if (val & GPU_FAULT)
	{
		kbase_report_gpu_fault(kbdev, val & MULTIPLE_GPU_FAULTS);
	}

	if (val & RESET_COMPLETED)
	{
		kbase_pm_reset_done(kbdev);
	}

	if (val & PRFCNT_SAMPLE_COMPLETED)
	{
		kbase_instr_hwcnt_sample_done(kbdev);
	}

	if (val & CLEAN_CACHES_COMPLETED)
	{
		kbase_clean_caches_done(kbdev);
	}

	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_IRQ_CLEAR), val, NULL);

	/* kbase_pm_check_transitions must be called after the IRQ has been cleared. This is because it might trigger
	 * further power transitions and we don't want to miss the interrupt raised to notify us that these further
	 * transitions have finished.
	 */
	if (val & POWER_CHANGED_ALL)
	{
		kbase_pm_check_transitions(kbdev);
	}
}


/*
 * Device trace functions
 */
#if KBASE_TRACE_ENABLE != 0

STATIC mali_error kbasep_trace_init( kbase_device *kbdev )
{
	osk_error osk_err;

	void *rbuf = osk_malloc(sizeof(kbase_trace)*KBASE_TRACE_SIZE);

	kbdev->trace_rbuf = rbuf;
	osk_err = osk_spinlock_irq_init(&kbdev->trace_lock, OSK_LOCK_ORDER_TRACE);

	if (rbuf == NULL || OSK_ERR_NONE != osk_err)
	{
		if ( rbuf != NULL )
		{
			osk_free( rbuf );
		}
		if ( osk_err == OSK_ERR_NONE )
		{
			osk_spinlock_irq_term(&kbdev->trace_lock);
		}
		return MALI_ERROR_FUNCTION_FAILED;
	}

	return MALI_ERROR_NONE;
}

STATIC void kbasep_trace_term( kbase_device *kbdev )
{
	osk_spinlock_irq_term(&kbdev->trace_lock);
	osk_free( kbdev->trace_rbuf );
}

void kbasep_trace_dump_msg( kbase_trace *trace_msg )
{
	char buffer[OSK_DEBUG_MESSAGE_SIZE];
	s32 written = 0;

	/* Initial part of message */
	written += MAX( osk_snprintf(buffer+written, MAX((int)OSK_DEBUG_MESSAGE_SIZE-written,0),
								 "%d.%.6d,%d,%d,%s,%p,%p,%.8llx,",
								 trace_msg->timestamp.tv_sec,
								 trace_msg->timestamp.tv_usec,
								 trace_msg->thread_id,
								 trace_msg->cpu,
								 kbasep_trace_code_string[trace_msg->code],
								 trace_msg->ctx,
								 trace_msg->uatom,
								 trace_msg->gpu_addr ), 0 );

	/* NOTE: Could add function callbacks to handle different message types */
	if ( (trace_msg->flags & KBASE_TRACE_FLAG_JOBSLOT) != MALI_FALSE )
	{
		/* Jobslot present */
		written += MAX( osk_snprintf(buffer+written, MAX((int)OSK_DEBUG_MESSAGE_SIZE-written,0),
									 "%d", trace_msg->jobslot), 0 );
	}
	written += MAX( osk_snprintf(buffer+written, MAX((int)OSK_DEBUG_MESSAGE_SIZE-written,0),
								 ","), 0 );

	if ( (trace_msg->flags & KBASE_TRACE_FLAG_REFCOUNT) != MALI_FALSE )
	{
		/* Refcount present */
		written += MAX( osk_snprintf(buffer+written, MAX((int)OSK_DEBUG_MESSAGE_SIZE-written,0),
									 "%d", trace_msg->refcount), 0 );
	}
	written += MAX( osk_snprintf(buffer+written, MAX((int)OSK_DEBUG_MESSAGE_SIZE-written,0),
								 ",", trace_msg->jobslot), 0 );

	/* Rest of message */
	written += MAX( osk_snprintf(buffer+written, MAX((int)OSK_DEBUG_MESSAGE_SIZE-written,0),
								 "0x%.8x", trace_msg->info_val), 0 );

	OSK_PRINT( OSK_BASE_CORE, "%s", buffer );
}

void kbasep_trace_add(kbase_device *kbdev, kbase_trace_code code, void *ctx, void *uatom, u64 gpu_addr,
					  u8 flags, int refcount, int jobslot, u32 info_val )
{
	kbase_trace *trace_msg;

	osk_spinlock_irq_lock( &kbdev->trace_lock );

	trace_msg = &kbdev->trace_rbuf[kbdev->trace_next_in];

	/* Fill the message */
	osk_debug_get_thread_info( &trace_msg->thread_id, &trace_msg->cpu );

	osk_gettimeofday(&trace_msg->timestamp);

	trace_msg->code     = code;
	trace_msg->ctx      = ctx;
	trace_msg->uatom    = uatom;
	trace_msg->gpu_addr = gpu_addr;
	trace_msg->jobslot  = jobslot;
	trace_msg->refcount = MIN((unsigned int)refcount, 0xFF) ;
	trace_msg->info_val = info_val;
	trace_msg->flags    = flags;

	/* Update the ringbuffer indices */
	kbdev->trace_next_in = (kbdev->trace_next_in + 1) & KBASE_TRACE_MASK;
	if ( kbdev->trace_next_in == kbdev->trace_first_out )
	{
		kbdev->trace_first_out = (kbdev->trace_first_out + 1) & KBASE_TRACE_MASK;
	}

	/* Done */

	osk_spinlock_irq_unlock( &kbdev->trace_lock );
}

void kbasep_trace_clear(kbase_device *kbdev)
{
	osk_spinlock_irq_lock( &kbdev->trace_lock );
	kbdev->trace_first_out = kbdev->trace_next_in;
	osk_spinlock_irq_unlock( &kbdev->trace_lock );
}

void kbasep_trace_dump(kbase_device *kbdev)
{
	u32 start;
	u32 end;


	OSK_PRINT( OSK_BASE_CORE, "Dumping trace:\nsecs,nthread,cpu,code,ctx,uatom,gpu_addr,jobslot,refcount,info_val");
	osk_spinlock_irq_lock( &kbdev->trace_lock );
	start = kbdev->trace_first_out;
	end = kbdev->trace_next_in;

	while (start != end)
	{
		kbase_trace *trace_msg = &kbdev->trace_rbuf[start];
		kbasep_trace_dump_msg( trace_msg );

		start = (start + 1) & KBASE_TRACE_MASK;
	}
	OSK_PRINT( OSK_BASE_CORE, "TRACE_END");

	osk_spinlock_irq_unlock( &kbdev->trace_lock );

	KBASE_TRACE_CLEAR(kbdev);
}

STATIC void kbasep_trace_hook_wrapper( void *param )
{
	kbase_device *kbdev = (kbase_device*)param;
	kbasep_trace_dump( kbdev );
}

#else /* KBASE_TRACE_ENABLE != 0 */
STATIC mali_error kbasep_trace_init( kbase_device *kbdev )
{
	CSTD_UNUSED(kbdev);
	return MALI_ERROR_NONE;
}

STATIC void kbasep_trace_term( kbase_device *kbdev )
{
	CSTD_UNUSED(kbdev);
}

STATIC void kbasep_trace_hook_wrapper( void *param )
{
	CSTD_UNUSED(param);
}

void kbasep_trace_add(kbase_device *kbdev, kbase_trace_code code, void *ctx, void *uatom, u64 gpu_addr,
					  u8 flags, int refcount, int jobslot, u32 info_val )
{
	CSTD_UNUSED(kbdev);
	CSTD_UNUSED(code);
	CSTD_UNUSED(ctx);
	CSTD_UNUSED(uatom);
	CSTD_UNUSED(gpu_addr);
	CSTD_UNUSED(flags);
	CSTD_UNUSED(refcount);
	CSTD_UNUSED(jobslot);
	CSTD_UNUSED(info_val);
}

void kbasep_trace_clear(kbase_device *kbdev)
{
	CSTD_UNUSED(kbdev);
}

void kbasep_trace_dump(kbase_device *kbdev)
{
	CSTD_UNUSED(kbdev);
}
#endif /* KBASE_TRACE_ENABLE != 0 */
