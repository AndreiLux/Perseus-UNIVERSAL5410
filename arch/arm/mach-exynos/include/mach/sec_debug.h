#ifndef SEC_DEBUG_H
#define SEC_DEBUG_H

#include <linux/sched.h>
#include <linux/semaphore.h>


extern void register_log_char_hook(void (*f) (char c));

static inline int sec_debug_init(void)
{
	return 0;
}

static inline int sec_debug_magic_init(void)
{
	return 0;
}

static inline void sec_debug_check_crash_key(unsigned int code, int value)
{
}

static inline void sec_getlog_supply_fbinfo(void *p_fb, u32 res_x, u32 res_y,
					    u32 bpp, u32 frames)
{
}

static inline void sec_getlog_supply_meminfo(u32 size0, u32 addr0, u32 size1,
					     u32 addr1)
{
}

static inline void sec_getlog_supply_loggerinfo(void *p_main,
						void *p_radio, void *p_events,
						void *p_system)
{
}

static inline void sec_getlog_supply_kloginfo(void *klog_buf)
{
}

static inline void sec_gaf_supply_rqinfo(unsigned short curr_offset,
					 unsigned short rq_offset)
{
}

static inline void sec_debug_save_context(void)
{
}

struct worker;
struct work_struct;

static inline void sec_debug_task_log(int cpu, struct task_struct *task)
{
}

static inline void sec_debug_irq_log(unsigned int irq, void *fn, int en)
{
}

static inline void sec_debug_work_log(struct worker *worker,
				      struct work_struct *work, work_func_t f, int en)
{
}

static inline void sec_debug_timer_log(unsigned int type, void *fn)
{
}

static inline void sec_debug_softirq_log(unsigned int irq, void *fn, int en)
{
}

static inline void sec_debug_irq_last_exit_log(void)
{
}

static inline void debug_semaphore_init(void)
{
}

static inline void debug_semaphore_down_log(struct semaphore *sem)
{
}

static inline void debug_semaphore_up_log(struct semaphore *sem)
{
}

static inline void debug_rwsemaphore_init(void)
{
}

static inline void debug_rwsemaphore_down_read_log(struct rw_semaphore *sem)
{
}

static inline void debug_rwsemaphore_down_write_log(struct rw_semaphore *sem)
{
}

static inline void debug_rwsemaphore_up_log(struct rw_semaphore *sem)
{
}
enum sec_debug_aux_log_idx {
	SEC_DEBUG_AUXLOG_CPU_CLOCK_SWITCH_CHANGE,
	SEC_DEBUG_AUXLOG_RUNTIME_PM_CHANGE,
	SEC_DEBUG_AUXLOG_PM_CHANGE,
	SEC_DEBUG_AUXLOG_NOTIFY_FAIL,
	SEC_DEBUG_AUXLOG_ITEM_MAX,
};

#ifdef CONFIG_SEC_DEBUG_AUXILIARY_LOG
extern void sec_debug_aux_log(int idx, char *fmt, ...);
#else
#define sec_debug_aux_log(idx, ...) do { } while (0)
#endif

#ifdef CONFIG_SEC_AVC_LOG
extern void sec_debug_avc_log(char *fmt, ...);
#endif

#if defined(CONFIG_MACH_Q1_BD)
extern int sec_debug_panic_handler_safe(void *buf);
#endif

extern void read_lcd_register(void);

#endif				/* SEC_DEBUG_H */
