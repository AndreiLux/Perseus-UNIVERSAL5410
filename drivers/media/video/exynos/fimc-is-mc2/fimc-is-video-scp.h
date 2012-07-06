#ifndef FIMC_IS_VIDEO_SCP_H
#define FIMC_IS_VIDEO_SCP_H

#include "fimc-is-video.h"

struct fimc_is_video_scp {
	struct fimc_is_video_common common;
};

int fimc_is_scp_video_probe(void *data);

#endif
