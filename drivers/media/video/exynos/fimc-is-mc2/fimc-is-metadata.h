/*
 * Samsung Exynos5 SoC series Camera API 2.0 HAL
 *
 * Internal Metadata (controls/dynamic metadata and static metadata)
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd
 * Contact: Sungjoong Kang, <sj3.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*2012.04.18 Version 0.1 Initial Release*/
/*2012.04.23 Version 0.2 Added static metadata (draft)*/


#ifndef FIMC_IS_METADATA_H_
#define FIMC_IS_METADATA_H_

struct rational {
	uint32_t num;
	uint32_t den;
};


/*
 *controls/dynamic metadata
*/

/* android.request */

enum metadata_mode {
	METADATA_MODE_NONE,
	METADATA_MODE_FULL
};

struct camera2_request_ctl {
	uint32_t			id;
	enum metadata_mode	metadataMode;
	uint8_t				outputStreams[16];
};

struct camera2_request_dm {
	uint32_t			id;
	enum metadata_mode	metadataMode;
	uint32_t			frameCount;
};



/* android.lens */

enum optical_stabilization_mode {
	OPTICAL_STABILIZATION_MODE_OFF,
	OPTICAL_STABILIZATION_MODE_ON
};

struct camera2_lens_ctl {
	float							focusDistance;
	float							aperture;
	float							focalLength;
	float							filterDensity;
	enum optical_stabilization_mode	opticalStabilizationMode;
};

struct camera2_lens_dm {
	float							focusDistance;
	float							aperture;
	float							focalLength;
	float							filterDensity;
	enum optical_stabilization_mode	opticalStabilizationMode;
	float							focusRange[2];
};



/* android.sensor */

struct camera2_sensor_ctl {
	uint64_t	exposureTime;
	uint64_t	frameDuration;
	uint32_t	sensitivity;
};

struct camera2_sensor_dm {
	uint64_t	exposureTime;
	uint64_t	frameDuration;
	uint32_t	sensitivity;
	uint64_t	timeStamp;
	uint32_t	frameCount;
};



/* android.flash */

enum flash_mode {
	CAM2_FLASH_MODE_OFF,
	CAM2_FLASH_MODE_SINGLE,
	CAM2_FLASH_MODE_TORCH
};

struct camera2_flash_ctl {
	enum flash_mode	flashMode;
	uint8_t			firingPower;
	uint64_t		firingTime;
};

struct camera2_flash_dm {
	enum flash_mode	flashMode;
	uint8_t			firingPower;
	uint64_t		firingTime;
};



/* android.flash */

enum hotpixel_mode {
	HOTPIXEL_MODE_OFF,
	HOTPIXEL_MODE_FAST,
	HOTPIXEL_MODE_HIGH_QUALITY
};


struct camera2_hotpixel_ctl {
	enum hotpixel_mode	mode;
};

struct camera2_hotpixel_dm {
	enum hotpixel_mode	mode;
};



/* android.demosaic */

enum demosaic_mode {
	DEMOSAIC_MODE_OFF,
	DEMOSAIC_MODE_FAST,
	DEMOSAIC_MODE_HIGH_QUALITY
};

struct camera2_demosaic_ctl {
	enum demosaic_mode	mode;
};

struct camera2_demosaic_dm {
	enum demosaic_mode	mode;
};



/* android.noiseReduction */

enum noise_mode {
	NOISEREDUCTION_MODE_OFF,
	NOISEREDUCTION_MODE_FAST,
	NOISEREDUCTION_MODE_HIGH_QUALITY
};

struct camera2_noisereduction_ctl {
	enum noise_mode	mode;
	uint8_t			strength;
};

struct camera2_noisereduction_dm {
	enum noise_mode	mode;
	uint8_t			strength;
};



/* android.shading */

enum shading_mode {
	SHADING_MODE_OFF,
	SHADING_MODE_FAST,
	SHADING_MODE_HIGH_QUALITY
};

struct camera2_shading_ctl {
	enum shading_mode	mode;
};

struct camera2_shading_dm {
	enum shading_mode	mode;
};



/* android.geometric */

enum geometric_mode {
	GEOMETRIC_MODE_OFF,
	GEOMETRIC_MODE_FAST,
	GEOMETRIC_MODE_HIGH_QUALITY
};

struct camera2_geometric_ctl {
	enum geometric_mode	mode;
};

struct camera2_geometric_dm {
	enum geometric_mode	mode;
};



/* android.colorCorrection */

enum colorcorrection_mode {
	COLORCORRECTION_MODE_TRANSFORM_MATRIX,
	COLORCORRECTION_MODE_FAST,
	COLORCORRECTION_MODE_HIGH_QUALITY,
	COLORCORRECTION_MODE_EFFECT_MONO,
	COLORCORRECTION_MODE_EFFECT_NEGATIVE,
	COLORCORRECTION_MODE_EFFECT_SOLARIZE,
	COLORCORRECTION_MODE_EFFECT_SEPIA,
	COLORCORRECTION_MODE_EFFECT_POSTERIZE,
	COLORCORRECTION_MODE_EFFECT_WHITEBOARD,
	COLORCORRECTION_MODE_EFFECT_BLACKBOARD,
	COLORCORRECTION_MODE_EFFECT_AQUA
};


struct camera2_colorcorrection_ctl {
	enum colorcorrection_mode	mode;
	float						transform[9];
};

struct camera2_colorcorrection_dm {
	enum colorcorrection_mode	mode;
	float						transform[9];
};



/* android.tonemap */

enum tonemap_mode {
	TONEMAP_MODE_CONTRAST_CURVE,
	TONEMAP_MODE_FAST,
	TONEMAP_MODE_HIGH_QUALITY
};

struct camera2_tonemap_ctl {
	enum tonemap_mode	mode;
	float				curveRed[32];
	/*assuming maxCurvePoints = 32*/
	float				curveGreen[32];
	float				curveBlue[32];
};

struct camera2_tonemap_dm {
	enum tonemap_mode	mode;
	float				curveRed[32];
	/*assuming maxCurvePoints = 32*/
	float				curveGreen[32];
	float				curveBlue[32];
};



/* android.edge */

enum edge_mode {
	EDGE_MODE_OFF,
	EDGE_MODE_FAST,
	EDGE_MODE_HIGH_QUALITY
};

struct camera2_edge_ctl {
	enum edge_mode	mode;
	uint8_t			strength;
};

struct camera2_edge_dm {
	enum edge_mode	mode;
	uint8_t			strength;
};



/* android.scaler */

struct camera2_scaler_ctl {
	uint32_t	cropRegion[3];
	uint32_t	rotation;
};

struct camera2_scaler_dm {
	uint32_t	size[2];
	uint8_t		format;
	uint32_t	cropRegion[3];
	uint32_t	rotation;
};



/* android.jpeg */
struct camera2_jpeg_ctl {
	uint8_t		quality;
	uint32_t	thumbnailSize[2];
	uint8_t		thumbnailQuality;
	double		gpsCoordinates[3];
	uint8_t		gpsProcessingMethod;
	uint64_t	gpsTimestamp;
	uint32_t	orientation;
};

struct camera2_jpeg_dm {
	uint8_t		quality;
	uint32_t	thumbnailSize[2];
	uint8_t		thumbnailQuality;
	double		gpsCoordinates[3];
	uint8_t		gpsProcessingMethod;
	uint64_t	gpsTimestamp;
	uint32_t	orientation;
};



/* android.statistics */

enum facedetect_mode {
	FACEDETECT_MODE_OFF,
	FACEDETECT_MODE_SIMPLE,
	FACEDETECT_MODE_FULL
};

enum histogram_mode {
	HISTOGRAM_MODE_OFF,
	HISTOGRAM_MODE_ON
};

enum sharpnessmap_mode {
	SHARPNESSMAP_MODE_OFF,
	SHARPNESSMAP_MODE_ON
};

struct camera2_stats_ctl {
	enum facedetect_mode	faceDetectMode;
	enum histogram_mode		histogramMode;
	enum sharpnessmap_mode	sharpnessMapMode;
};

/* REMARKS : FD results are not included */
struct camera2_stats_dm {
	enum facedetect_mode	faceDetectMode;
	/*faceRetangles
	faceScores
	faceLandmarks
	faceIds*/
	enum histogram_mode		histogramMode;
	/*histogram*/
	enum sharpnessmap_mode	sharpnessMapMode;
	/*sharpnessMap*/
};



/* android.control */
enum aa_mode {
	AA_MODE_OFF,
	AA_MODE_AUTO,
	AA_MODE_SCENE_MODE_FACE_PRIORITY,
	AA_MODE_SCENE_MODE_ACTION,
	AA_MODE_SCENE_MODE_PORTRAIT,
	AA_MODE_SCENE_MODE_LANDSCAPE,
	AA_MODE_SCENE_MODE_NIGHT,
	AA_MODE_SCENE_MODE_NIGHT_PORTRAIT,
	AA_MODE_SCENE_MODE_THEATRE,
	AA_MODE_SCENE_MODE_BEACH,
	AA_MODE_SCENE_MODE_SNOW,
	AA_MODE_SCENE_MODE_SUNSET,
	AA_MODE_SCENE_MODE_STEADYPHOTO,
	AA_MODE_SCENE_MODE_FIREWORKS,
	AA_MODE_SCENE_MODE_SPORTS,
	AA_MODE_SCENE_MODE_PARTY,
	AA_MODE_SCENE_MODE_CANDLELIGHT,
	AA_MODE_SCENE_MODE_BARCODE
};

enum aa_aemode {
	AA_AEMODE_OFF,
	AA_AEMODE_ON,
	AA_AEMODE_ON_AUTO_FLASH,
	AA_AEMODE_ON_ALWAYS_FLASH,
	AA_AEMODE_ON_AUTO_FLASH_REDEYE
};

enum aa_ae_antibanding_mode {
	AA_AE_ANTIBANDING_OFF,
	AA_AE_ANTIBANDING_50HZ,
	AA_AE_ANTIBANDING_60HZ,
	AA_AE_ANTIBANDING_AUTO
};

enum aa_awbmode {
	AA_AWBMODE_OFF,
	AA_AWBMODE_WB_AUTO,
	AA_AWBMODE_WB_INCANDESCENT,
	AA_AWBMODE_WB_FLUORESCENT,
	AA_AWBMODE_WB_WARM_FLUORESCENT,
	AA_AWBMODE_WB_DAYLIGHT,
	AA_AWBMODE_WB_CLOUDY_DAYLIGHT,
	AA_AWBMODE_WB_TWILIGHT,
	AA_AWBMODE_WB_SHADE
};

enum aa_afmode {
	AA_AFMODE_OFF,
	AA_AFMODE_FOCUS_MODE_AUTO,
	AA_AFMODE_FOCUS_MODE_MACRO,
	AA_AFMODE_FOCUS_MODE_CONTINUOUS_VIDEO,
	AA_AFMODE_FOCUS_MODE_CONTINUOUS_PICTURE
};

enum aa_afstate {
	AA_AFSTATE_INACTIVE,
	AA_AFSTATE_PASSIVE_SCAN,
	AA_AFSTATE_ACTIVE_SCAN,
	AA_AFSTATE_AF_ACQUIRED_FOCUS,
	AA_AFSTATE_AF_FAILED_FOCUS
};

struct camera2_aa_ctl {
	enum aa_mode				mode;
	enum aa_aemode				aeMode;
	uint32_t					aeRegions[5];
	/*5 per region(x1,y1,x2,y2,weight). currently assuming 1 region.*/
	int32_t						aeExpCompensation;
	uint32_t					aeTargetFpsRange[2];
	enum aa_ae_antibanding_mode	aeAntibandingMode;
	enum aa_awbmode				awbMode;
	uint32_t					awbRegions[5];
	/*5 per region(x1,y1,x2,y2,weight). currently assuming 1 region.*/
	enum aa_afmode				afMode;
	uint32_t					afRegions[5];
	/*5 per region(x1,y1,x2,y2,weight). currently assuming 1 region.*/
	uint8_t						afTrigger;
	uint8_t						videoStabilizationMode;
};

struct camera2_aa_dm {
	enum aa_mode				mode;
	enum aa_aemode				aeMode;
	/*needs check*/
	uint32_t					aeRegions[5];
	/*5 per region(x1,y1,x2,y2,weight). currently assuming 1 region.*/
	int32_t						aeExpCompensation;
	/*needs check*/
	enum aa_awbmode				awbMode;
	uint32_t					awbRegions[5];
	/*5 per region(x1,y1,x2,y2,weight). currently assuming 1 region.*/
	enum aa_afmode				afMode;
	uint32_t					afRegions[5];
	/*5 per region(x1,y1,x2,y2,weight). currently assuming 1 region*/
	uint8_t						afTrigger;
	enum aa_afstate				afState;
	uint8_t						videoStabilizationMode;
};



struct camera2_ctl {
	struct camera2_request_ctl			request;
	struct camera2_lens_ctl				lens;
	struct camera2_sensor_ctl			sensor;
	struct camera2_flash_ctl			flash;
	struct camera2_hotpixel_ctl			hotpixel;
	struct camera2_demosaic_ctl			demosaic;
	struct camera2_noisereduction_ctl	noise;
	struct camera2_shading_ctl			shading;
	struct camera2_geometric_ctl		geometric;
	struct camera2_colorcorrection_ctl	color;
	struct camera2_tonemap_ctl			tonemap;
	struct camera2_edge_ctl				edge;
	struct camera2_scaler_ctl			scaler;
	struct camera2_jpeg_ctl				jpeg;
	struct camera2_stats_ctl			stats;
	struct camera2_aa_ctl				aa;
};

struct camera2_dm {
	struct camera2_request_dm			request;
	struct camera2_lens_dm				lens;
	struct camera2_sensor_dm			sensor;
	struct camera2_flash_dm				flash;
	struct camera2_hotpixel_dm			hotpixel;
	struct camera2_demosaic_dm			demosaic;
	struct camera2_noisereduction_dm	noise;
	struct camera2_shading_dm			shading;
	struct camera2_geometric_dm			geometric;
	struct camera2_colorcorrection_dm	color;
	struct camera2_tonemap_dm			tonemap;
	struct camera2_edge_dm				edge;
	struct camera2_scaler_dm			scaler;
	struct camera2_jpeg_dm				jpeg;
	struct camera2_stats_dm				stats;
	struct camera2_aa_dm				aa;
};

struct camera2_vs {
    /**	\brief
		Set sensor, lens, flash control for next frame.
		\remarks
		This flag can be combined.
		[0 bit] sensor
		[1 bit] lens
		[2 bit] flash
    */
	uint32_t	updateFlag;

	struct camera2_lens_ctl				lens;
	struct camera2_sensor_ctl			sensor;
	struct camera2_flash_ctl			flash;
};

struct camera2_shot {
	struct camera2_ctl	ctl;
	struct camera2_dm	dm;
	/*vendor specific area*/
	struct camera2_vs	vender;
	uint32_t			magicNumber;
};


/*
 *   static metadata
 */


/* android.lens */

enum lens_facing {
	LENS_FACING_FRONT,
	LENS_FACING_BACK
};

struct camera2_lens_sm {
	float				minimumFocusDistance;
	float				availableFocalLength[2];
	float				availableApertures;
	/*assuming 1 aperture*/
	float				availableFilterDensities;
	/*assuming 1 ND filter value*/
	uint8_t				availableOpticalStabilization;
	/*assuming 1*/
	float				shadingMap[3][40][30];
	float				geometricCorrectionMap[2][3][40][30];
	enum lens_facing	facing;
	float				position[2];
};

/* android.sensor */

enum sensor_colorfilterarrangement {
	SENSOR_COLORFILTERARRANGEMENT_RGGB,
	SENSOR_COLORFILTERARRANGEMENT_GRBG,
	SENSOR_COLORFILTERARRANGEMENT_GBRG,
	SENSOR_COLORFILTERARRANGEMENT_BGGR,
	SENSOR_COLORFILTERARRANGEMENT_RGB
};

enum sensor_ref_illuminant {
	SENSOR_ILLUMINANT_DAYLIGHT = 1,
	SENSOR_ILLUMINANT_FLUORESCENT = 2,
	SENSOR_ILLUMINANT_TUNGSTEN = 3,
	SENSOR_ILLUMINANT_FLASH = 4,
	SENSOR_ILLUMINANT_FINE_WEATHER = 9,
	SENSOR_ILLUMINANT_CLOUDY_WEATHER = 10,
	SENSOR_ILLUMINANT_SHADE = 11,
	SENSOR_ILLUMINANT_DAYLIGHT_FLUORESCENT = 12,
	SENSOR_ILLUMINANT_DAY_WHITE_FLUORESCENT = 13,
	SENSOR_ILLUMINANT_COOL_WHITE_FLUORESCENT = 14,
	SENSOR_ILLUMINANT_WHITE_FLUORESCENT = 15,
	SENSOR_ILLUMINANT_STANDARD_A = 17,
	SENSOR_ILLUMINANT_STANDARD_B = 18,
	SENSOR_ILLUMINANT_STANDARD_C = 19,
	SENSOR_ILLUMINANT_D55 = 20,
	SENSOR_ILLUMINANT_D65 = 21,
	SENSOR_ILLUMINANT_D75 = 22,
	SENSOR_ILLUMINANT_D50 = 23,
	SENSOR_ILLUMINANT_ISO_STUDIO_TUNGSTEN = 24
};

struct camera2_sensor_sm {
	uint32_t	exposureTimeRange[2];
	uint32_t	maxFrameDuration;
	uint32_t	sensitivityRange[2];
	enum sensor_colorfilterarrangement colorFilterArrangement;
	uint32_t	pixelArraySize[2];
	uint32_t	activeArraySize[4];
	uint32_t	whiteLevel;
	uint32_t	blackLevelPattern[4];
	struct rational	colorTransform1[9];
	struct rational	colorTransform2[9];
	enum sensor_ref_illuminant	referenceIlluminant1;
	enum sensor_ref_illuminant	referenceIlluminant2;
	struct rational	forwardMatrix1[9];
	struct rational	forwardMatrix2[9];
	struct rational	calibrationTransform1[9];
	struct rational	calibrationTransform2[9];
	struct rational	baseGainFactor;
	uint32_t	maxAnalogSensitivity;
	float		noiseModelCoefficients[2];
	uint32_t	orientation;
};



/* android.flash */

struct camera2_flash_sm {
	uint8_t		available;
	uint64_t	chargeDuration;
};



/* android.colorCorrection */

struct camera2_colorcorrection_sm {
	enum colorcorrection_mode	availableModes[10];
	/*assuming 10 supported modes*/
};



/* android.tonemap */

struct camera2_tonemap_sm {
	uint32_t	maxCurvePoints;
};



/* android.scaler */

enum scaler_availableformats {
	SCALER_FORMAT_BAYER_RAW,
	SCALER_FORMAT_YV12,
	SCALER_FORMAT_NV21,
	SCALER_FORMAT_JPEG,
	SCALER_FORMAT_UNKNOWN
};

struct camera2_scaler_sm {
	enum scaler_availableformats availableFormats[4];
	/*assuming # of availableFormats = 4*/
	uint32_t	availableSizesPerFormat[4];
	uint32_t	availableSizes[4][8][2];
	/*assuning availableSizesPerFormat=8*/
	uint64_t	availableMinFrameDurations[4][8];
	float		maxDigitalZoom;
};



/* android.jpeg */

struct camera2_jpeg_sm {
	uint32_t	availableThumbnailSizes[2][8];
	/*assuming supported size=8*/
};



/* android.statistics */

struct camera2_statistics_sm {
	uint8_t		availableFaceDetectModes[3];
	/*assuming supported modes = 3;*/
	uint32_t	maxFaceCount;
	uint32_t	histogramBucketCount;
	uint32_t	maxHistogramCount;
	uint32_t	sharpnessMapSize[2];
	uint32_t	maxSharpnessMapValue;
};



/* android.control */

struct camera2_aa_sm {
	uint8_t			availableModes[10];
	/*assuming # of available scene modes = 10*/
	uint32_t		maxRegions;
	uint8_t			aeAvailableModes[8];
	/*assuming # of available ae modes = 8*/
	struct rational	aeCompensationStep;
	int32_t			aeCompensationRange[2];
	uint32_t		aeAvailableTargetFpsRanges[2][8];
	uint8_t			aeAvailableAntibandingModes[4];
	uint8_t			awbAvailableModes[10];
	/*assuming # of awbAvailableModes = 10*/
	uint8_t			afAvailableModes[4];
	/*assuming # of afAvailableModes = 4*/
};

struct camera2_static_metadata {
	struct camera2_lens_sm				lens;
	struct camera2_sensor_sm			sensor;
	struct camera2_flash_sm				flash;
	struct camera2_colorcorrection_sm	color;
	struct camera2_tonemap_sm			tonemap;
	struct camera2_scaler_sm			scaler;
	struct camera2_jpeg_sm				jpeg;
	struct camera2_statistics_sm		statistics;
	struct camera2_aa_sm				aa;
};

#endif
