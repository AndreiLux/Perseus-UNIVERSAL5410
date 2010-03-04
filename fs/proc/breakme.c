#include <linux/cpumask.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/irqnr.h>
#include <asm/cputime.h>
#include <asm/uaccess.h>

#define MAX_BREAKME_WRITE 64
static ssize_t write_breakme(struct file *file, const char __user *buf,
				   size_t count, loff_t *ppos)
{
	char kbuf[MAX_BREAKME_WRITE + 1];
	DEFINE_SPINLOCK(lock_me_up);

	if (count) {
		if (count > MAX_BREAKME_WRITE)
			return -EINVAL;
		if (copy_from_user(&kbuf, buf, count))
			return -EFAULT;
		kbuf[min(count, sizeof(kbuf))-1] = '\0';

		/* Null pointer dereference */
		if (!strcmp(kbuf, "nullptr"))
			*(unsigned long *)0 = 0;
		/* BUG() */
		else if (!strcmp(kbuf, "bug"))
			BUG();
		/* Panic */
		else if (!strcmp(kbuf, "panic"))
			panic("Testing panic");
		/* Set up a deadlock (call this twice) */
		else if (!strcmp(kbuf, "deadlock"))
			spin_lock(&lock_me_up);
		/* lockup */
		else if (!strcmp(kbuf, "softlockup")) {
			while (1)
				;
		}
		/* lockup with interrupts enabled */
		else if (!strcmp(kbuf, "irqlockup")) {
			spin_lock(&lock_me_up);
			while (1)
				;
		}
		/* lockup with interrupts disabled */
		else if (!strcmp(kbuf, "nmiwatchdog")) {
			spin_lock_irq(&lock_me_up);
			while (1)
				;
		}
	}
	return count;
}

static struct file_operations proc_breakme_operations = {
	.write		= write_breakme,
};

void __init proc_breakme_init(void)
{
	proc_create("breakme", S_IWUSR, NULL, &proc_breakme_operations);
}
