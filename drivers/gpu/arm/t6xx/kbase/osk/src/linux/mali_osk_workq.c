/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <osk/mali_osk.h>

#if MALI_LICENSE_IS_GPL == 0
/* Wrapper function to allow flushing in Non-GPL driver */
void oskp_work_func_wrapper( struct work_struct *work )
{
	osk_workq_work *osk_work = CONTAINER_OF( work, osk_workq_work, os_work );
	osk_workq *parent_wq;
	u32 val;
	unsigned long flags;

	parent_wq = osk_work->parent_wq;
	osk_work->actual_fn( osk_work );
	/* work and osk_work could disappear from this point on */

	/* parent_wq of course shouldn't disappear *yet*, because it must itself flush this function before term */

	spin_lock_irqsave( &parent_wq->active_items_lock, flags );
	val = --(parent_wq->nr_active_items);
	/* The operations above and below must form an atomic operation themselves,
	 * hence the lock. See osk_workq_flush() for why */
	if ( val == 0 )
	{
		wake_up( &parent_wq->waitq_zero_active_items );
	}
	spin_unlock_irqrestore( &parent_wq->active_items_lock, flags );

	/* parent_wq may've now disappeared */
}

#endif /* MALI_LICENSE_IS_GPL == 0 */
