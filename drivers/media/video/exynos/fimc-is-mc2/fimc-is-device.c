#include <linux/module.h>

#include "fimc-is-core.h"
#include "fimc-is-param.h"

void fimc_is_subdev_dis_start(struct fimc_is_device_ischain *device,
	struct dis_param *param, u32 *lindex, u32 *hindex, u32 *indexes)
{
	BUG_ON(!device);
	BUG_ON(!param);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

#ifdef ENABLE_VDIS
	param->control.cmd = CONTROL_COMMAND_START;
	param->control.bypass = CONTROL_BYPASS_DISABLE;
	param->control.buffer_number = SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF;
	param->control.buffer_address = device->imemory.dvaddr_shared + 300 * sizeof(u32);
	device->is_region->shared[300] = device->imemory.dvaddr_dis;
#else
	merr("can't start hw vdis", device);
	BUG();
#endif

	*lindex |= LOWBIT_OF(PARAM_DIS_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_DIS_CONTROL);
	(*indexes)++;
}

void fimc_is_subdev_dis_stop(struct fimc_is_device_ischain *device,
	struct dis_param *param, u32 *lindex, u32 *hindex, u32 *indexes)
{
	BUG_ON(!device);
	BUG_ON(!param);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	param->control.cmd = CONTROL_COMMAND_STOP;
	param->control.bypass = CONTROL_BYPASS_DISABLE;

	*lindex |= LOWBIT_OF(PARAM_DIS_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_DIS_CONTROL);
	(*indexes)++;
}

void fimc_is_subdev_dis_bypass(struct fimc_is_device_ischain *device,
	struct dis_param *param, u32 *lindex, u32 *hindex, u32 *indexes)
{
	BUG_ON(!device);
	BUG_ON(!param);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

#ifdef ENABLE_FULL_BYPASS
	param->control.cmd = CONTROL_COMMAND_STOP;
	param->control.bypass = CONTROL_BYPASS_ENABLE;
#else
	param->control.cmd = CONTROL_COMMAND_START;
	/*
	 * HACK
	 * enabling software dis is 2 of bypass command
	 */
	param->control.bypass = 2;
#endif

	*lindex |= LOWBIT_OF(PARAM_DIS_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_DIS_CONTROL);
	(*indexes)++;
}

void fimc_is_subdev_dnr_start(struct fimc_is_device_ischain *device,
	struct tdnr_param *param, u32 *lindex, u32 *hindex, u32 *indexes)
{
	BUG_ON(!device);
	BUG_ON(!param);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	param->control.cmd = CONTROL_COMMAND_START;
	param->control.bypass = CONTROL_BYPASS_DISABLE;
	param->control.buffer_number = SIZE_DNR_INTERNAL_BUF * NUM_DNR_INTERNAL_BUF;
	param->control.buffer_address = device->imemory.dvaddr_shared + 350 * sizeof(u32);
	device->is_region->shared[350] = device->imemory.dvaddr_3dnr;

	*lindex |= LOWBIT_OF(PARAM_TDNR_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_TDNR_CONTROL);
	(*indexes)++;
}

void fimc_is_subdev_dnr_stop(struct fimc_is_device_ischain *device,
	struct tdnr_param *param, u32 *lindex, u32 *hindex, u32 *indexes)
{
	BUG_ON(!device);
	BUG_ON(!param);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	param->control.cmd = CONTROL_COMMAND_STOP;
	param->control.bypass = CONTROL_BYPASS_DISABLE;

	*lindex |= LOWBIT_OF(PARAM_TDNR_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_TDNR_CONTROL);
	(*indexes)++;
}

void fimc_is_subdev_dnr_bypass(struct fimc_is_device_ischain *device,
	struct tdnr_param *param, u32 *lindex, u32 *hindex, u32 *indexes)
{
	BUG_ON(!device);
	BUG_ON(!param);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

#ifdef ENABLE_FULL_BYPASS
	param->control.cmd = CONTROL_COMMAND_STOP;
#else
	param->control.cmd = CONTROL_COMMAND_START;
#endif
	param->control.bypass = CONTROL_BYPASS_ENABLE;

	*lindex |= LOWBIT_OF(PARAM_TDNR_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_TDNR_CONTROL);
	(*indexes)++;
}