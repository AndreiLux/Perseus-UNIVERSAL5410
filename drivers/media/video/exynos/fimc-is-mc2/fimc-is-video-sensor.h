#ifndef FIMC_IS_VIDEO_SENSOR_H
#define FIMC_IS_VIDEO_SENSOR_H

#include "fimc-is-video.h"

struct fimc_is_video_sensor {
	struct fimc_is_video_common common;
};

int fimc_is_sensor_video_probe(void *data);

#endif
