#ifndef FIMC_IS_VIDEO_SCC_H
#define FIMC_IS_VIDEO_SCC_H

#include "fimc-is-video.h"

struct fimc_is_video_scc {
	struct fimc_is_video_common common;
};

int fimc_is_scc_video_probe(void *core_data);

#endif
