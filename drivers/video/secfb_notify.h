#ifndef _SECFB_NOTIFY_H
#define _SECFB_NOTIFY_H


#define SECFB_EVENT_CABC_READ	0x100
#define SECFB_EVENT_CABC_WRITE	0x101
#define SECFB_EVENT_BL_UPDATE	0x102

extern int secfb_register_client(struct notifier_block *nb);
extern int secfb_unregister_client(struct notifier_block *nb);
extern int secfb_notifier_call_chain(unsigned long val, void *v);

#endif /* _SECFB_NOTIFY_H */
