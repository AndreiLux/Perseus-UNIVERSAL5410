/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <kbase/src/common/mali_kbase.h>

#ifdef __KERNEL__
#define beenthere(f, a...)	pr_debug("%s:" f, __func__, ##a)
#else
#define beenthere(f, a...)	OSK_PRINT_INFO(OSK_BASE_EVENT, "%s:" f, __func__, ##a)
#endif

STATIC void *kbase_event_process(kbase_context *ctx,
				 kbase_event *event)
{
	void *data;
	void *ptr = event;
	kbasep_js_policy *js_policy;

	/*
	 * We're in the right user context, do some post processing
	 * before returning to user-mode.
	 */

	OSK_ASSERT(ctx != NULL);
	OSK_ASSERT(event->event_code);
	js_policy = &(ctx->kbdev->js_data.policy);

	if ((event->event_code & BASE_JD_SW_EVENT_TYPE_MASK) == BASE_JD_SW_EVENT_JOB)
	{
		kbase_jd_atom *katom = (void *)event->data;
		/* return the offset in the ring buffer... */
		data = (void *)((uintptr_t)katom->user_atom - (uintptr_t)ctx->jctx.pool);

		if (!(katom->core_req & BASE_JD_REQ_EXTERNAL_RESOURCES))
		{
			/* perform the sync operations only on successful jobs */
			kbase_post_job_sync(ctx,
					base_jd_get_atom_syncset(katom->user_atom, 0),
					katom->nr_syncsets);
		}

		if ((katom->core_req & BASE_JD_REQ_SOFT_JOB) == 0)
		{
			kbasep_js_policy_term_job( js_policy, ctx, katom );
		}

		ptr = katom;
		/* As the event is integral part of the katom, return
		 * immediatly... */
		goto out;
	}

	if ((event->event_code & BASE_JD_SW_EVENT_TYPE_MASK) == BASE_JD_SW_EVENT_BAG)
	{
		ptr = CONTAINER_OF(event, kbase_jd_bag, event);
		goto assign;
	}

assign:
	data = (void *)event->data; /* recast to discard const) */
out:
	osk_free(ptr);
	return data;
}

int kbase_event_pending(kbase_context *ctx)
{
	int ret;

	OSK_ASSERT(ctx);

	osk_mutex_lock(&ctx->event_mutex);
	ret  = (MALI_FALSE == OSK_DLIST_IS_EMPTY(&ctx->event_list)) || (MALI_TRUE == ctx->event_closed);
	osk_mutex_unlock(&ctx->event_mutex);

	return ret;
}
KBASE_EXPORT_TEST_API(kbase_event_pending)

int kbase_event_dequeue(kbase_context *ctx, base_jd_event *uevent)
{
	kbase_event *event;

	OSK_ASSERT(ctx);

	osk_mutex_lock(&ctx->event_mutex);

	if (OSK_DLIST_IS_EMPTY(&ctx->event_list))
	{
		if (ctx->event_closed)
		{
			/* generate the BASE_JD_EVENT_DRV_TERMINATED message on the fly */
			osk_mutex_unlock(&ctx->event_mutex);
			uevent->event_code = BASE_JD_EVENT_DRV_TERMINATED;
			uevent->data = NULL;
			beenthere("event system closed, returning BASE_JD_EVENT_DRV_TERMINATED(0x%X)\n", BASE_JD_EVENT_DRV_TERMINATED);
			return 0;
		}
		else
		{
			osk_mutex_unlock(&ctx->event_mutex);
			return -1;
		}
	}

	/* normal event processing */
	event = OSK_DLIST_POP_FRONT(&ctx->event_list, kbase_event, entry);

	osk_mutex_unlock(&ctx->event_mutex);

	beenthere("event dequeuing %p\n", (void*)event);
	uevent->event_code = event->event_code;
	uevent->data = kbase_event_process(ctx, event);

	return 0;
}
KBASE_EXPORT_TEST_API(kbase_event_dequeue)

void kbase_event_post(kbase_context *ctx,
		      kbase_event *event)
{
	beenthere("event queuing %p\n", event);

	OSK_ASSERT(ctx);

	osk_mutex_lock(&ctx->event_mutex);
	OSK_DLIST_PUSH_BACK(&ctx->event_list, event,
			       kbase_event, entry);
	osk_mutex_unlock(&ctx->event_mutex);

	kbase_event_wakeup(ctx);
}
KBASE_EXPORT_TEST_API(kbase_event_post)

void kbase_event_close(kbase_context * kctx)
{
	osk_mutex_lock(&kctx->event_mutex);
	kctx->event_closed = MALI_TRUE;
	osk_mutex_unlock(&kctx->event_mutex);
	kbase_event_wakeup(kctx);
}

mali_error kbase_event_init(kbase_context *kctx)
{
	osk_error osk_err;

	OSK_ASSERT(kctx);

	OSK_DLIST_INIT(&kctx->event_list);
	osk_err = osk_mutex_init(&kctx->event_mutex, OSK_LOCK_ORDER_QUEUE);
	if (OSK_ERR_NONE != osk_err)
	{
		return MALI_ERROR_FUNCTION_FAILED;
	}

	kctx->event_closed = MALI_FALSE;
	return MALI_ERROR_NONE;
}
KBASE_EXPORT_TEST_API(kbase_event_init)

void kbase_event_cleanup(kbase_context *kctx)
{
	OSK_ASSERT(kctx);

	osk_mutex_lock(&kctx->event_mutex);
	while (!OSK_DLIST_IS_EMPTY(&kctx->event_list))
	{
		kbase_event *event;
		event = OSK_DLIST_POP_FRONT(&kctx->event_list,
					       kbase_event, entry);
		beenthere("event dropping %p\n", event);
		osk_free(event);
	}
	osk_mutex_unlock(&kctx->event_mutex);
	osk_mutex_term(&kctx->event_mutex);
}
KBASE_EXPORT_TEST_API(kbase_event_cleanup)
