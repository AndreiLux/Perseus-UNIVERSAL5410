/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <kbase/src/linux/mali_kbase_config_linux.h>
#include <osk/mali_osk.h>

#if !MALI_LICENSE_IS_GPL || MALI_FAKE_PLATFORM_DEVICE

void kbasep_config_parse_io_resources(const kbase_io_resources *io_resources, struct resource *linux_resources)
{
	OSK_ASSERT(io_resources != NULL);
	OSK_ASSERT(linux_resources != NULL);

	OSK_MEMSET(linux_resources, 0, PLATFORM_CONFIG_RESOURCE_COUNT * sizeof(struct resource));

	linux_resources[0].start = io_resources->io_memory_region.start;
	linux_resources[0].end   = io_resources->io_memory_region.end;
	linux_resources[0].flags = IORESOURCE_MEM;

	linux_resources[1].start = linux_resources[1].end = io_resources->job_irq_number;
	linux_resources[1].flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL;

	linux_resources[2].start = linux_resources[2].end = io_resources->mmu_irq_number;
	linux_resources[2].flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL;

	linux_resources[3].start = linux_resources[3].end = io_resources->gpu_irq_number;
	linux_resources[3].flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL;
}

#endif /* !MALI_LICENSE_IS_GPL || MALI_FAKE_PLATFORM_DEVICE */
