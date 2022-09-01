#pragma once

//---------------------------------------------------------------------------
#include <windows.h>
//
// obs
#include <obs-module.h>
//
#include <graphics/matrix4.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
//
#include "obsHelpers.h"
#include "jrRegionDetector.h"
#include "source_list.h"
#include "ObsAutoZoomSourceDefs.h"
//---------------------------------------------------------------------------





//---------------------------------------------------------------------------
// forward declarations that are extern "C"
#ifdef __cplusplus
extern "C" {
#endif
	bool OnPropertyKeyTypeChangeCallback(obs_properties_t* props, obs_property_t* p, obs_data_t* settings);
	bool OnPropertyNSrcModifiedCallback(obs_properties_t* props, obs_property_t* property, obs_data_t* settings);
	void onHotkeyCallback(void* data, obs_hotkey_id id, obs_hotkey_t* key, bool pressed);
#ifdef __cplusplus
} // extern "C"
#endif
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// forward declarations for GLOBAL CHAR* ARRAYS USED IN SETTINGS PROPERTIES (would be better to have these as simple defines but awkward)
extern char* SETTING_zcMarkerPos_choices[];
extern char* SETTING_zcAlignment_choices[];
extern char* SETTING_zcMode_choices[];
extern char* SETTING_zcEasing_choices[];
//---------------------------------------------------------------------------



/*
//---------------------------------------------------------------------------
// forward declarations
class JrPlugin;
class SourceTracker;
//---------------------------------------------------------------------------
*/


























//---------------------------------------------------------------------------
class SourceTracker {
public:
	obs_weak_source_t* src_ref;
	char src_name[DefNameLenSource];

	uint32_t width;
	uint32_t height;	
	//
	int bx1, by1, bx2, by2;
	int tbx1, tby1, tbx2, tby2;
	int tx1comp, ty1comp, tx2comp, ty2comp;
	int changeTargetMomentumCounter;
	float priorx1, priory1,priorx2, priory2;
	//
	JrRegionSummary lastGoodMarkerRegion1, lastGoodMarkerRegion2;
	//
	bool trackingBoxReady;
	bool trackingBoxCompReady;
	bool cropBoxReady;
	bool priorBoxReady;
	//
	int targetStableCount;
};
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
class JrPlugin {
public:
	// obs pointer
	obs_source_t *context;

	// modified chroma effect
	gs_effect_t *effectChroma;
	// chroma effect params
	gs_eparam_t *pixel_size_param;
	gs_eparam_t *chroma_param;
	gs_eparam_t *similarity_param;
	gs_eparam_t *smoothness_param;
	//
	gs_eparam_t* dbgImageBehindAlpha_param;
	// internal params for chroma effect
	struct vec2 opt_chroma;
	float opt_similarity;
	float opt_smoothness;

	// zoomCrop effect
	gs_effect_t *effectZoomCrop;
	// zoomCrop effect params
	gs_eparam_t *param_mul;
	gs_eparam_t *param_add;
	gs_eparam_t* param_clip_ul;
	gs_eparam_t* param_clip_lr;
	// internal params
	struct vec2 mul_val;
	struct vec2 add_val;
	struct vec2 clip_ul;
	struct vec2 clip_lr;

	// options
	bool opt_zcPreserveAspectRatio;
	//
	int opt_zcMarkerPos;
	int opt_zcBoxMargin;
	int opt_zcBoxMoveSpeed;
	int opt_zcBoxMoveDistance;
	int opt_zcBoxMoveDelay;
	//
	// general configurable params
	bool opt_filterBypass;
	bool opt_debugDisplay;
	//
	bool opt_enableAutoUpdate;
	int opt_updateRate;
	//
	float opt_rmTDensityMin;
	float opt_rmTAspectMin;
	int opt_rmTSizeMin;
	int opt_rmTSizeMax;
	int opt_rmStageShrink;
	//
	// cropping aspect things
	int opt_zcAlign;
	int opt_zcMode;
	float opt_zcMaxZoom;
	char opt_forcedOutputResBuf[80];
	int forcedOutputWidth, forcedOutputHeight;
	float forcedOutputAspect;
	int opt_zcEasing;

	// markerless stuff - for when we can't find markers, do we show entire view or some subset
	int opt_markerlessSourceIndex;
	char opt_markerlessCoordsBuf[80];
	int markerlessCoordsX1, markerlessCoordsY1, markerlessCoordsX2, markerlessCoordsY2;

	// helper for detection regions using Connected Component Analysis
	JrRegionDetector regionDetector;
	//
	int stageArea;
	float stageShrink;

	// internals
	bool needsRebuildStaging;
	// source info
	uint32_t workingWidth;
	uint32_t workingHeight;
	uint32_t stageWidth;
	uint32_t stageHeight;
	uint32_t outWidth;
	uint32_t outHeight;

	// hotkeys
	obs_hotkey_id hotkeyId_ToggleAutoUpdate;
	obs_hotkey_id hotkeyId_OneShotZoomCrop;
	obs_hotkey_id hotkeyId_ToggleCropping;
	obs_hotkey_id hotkeyId_ToggleDebugDisplay;
	obs_hotkey_id hotkeyId_ToggleBypass;

	// new staging texture area and memory
	gs_texrender_t *texrender;
	gs_texrender_t *texrender2;
	gs_stagesurf_t *staging_texture;
	gs_texture_t *drawing_texture;
	uint8_t *data;
	uint32_t dlinesize;
	bool ready;
	HANDLE mutex;

	JrRegionSummary lastGoodMarkerRegion1, lastGoodMarkerRegion2;
	// new border box details
	int dbx1, dby1, dbx2, dby2;
	int tbx1, tby1, tbx2, tby2;
	int tx1comp, ty1comp, tx2comp, ty2comp;
	int changeTargetMomentumCounter;
	float priorx1, priory1,priorx2, priory2;
	//
	bool trackingBoxReady;
	bool trackingBoxCompReady;
	bool cropBoxReady;
	bool priorBoxReady;
	//
	int targetStableCount;
	//
	bool trackingOneShot;
	int trackingUpdateCounter;
	//
	// new source list stuff
	char src_name[DefMaxSources][DefNameLenSource];
	obs_weak_source_t *src_ref[DefMaxSources];
	//
	bool in_enum;
	int n_src;
	//
	int sourceIndex;
	int lastSourceIndex;
	//
	// new multiscoure trackers
	SourceTracker tracker[DefMaxSources];
	int sourceIndexViewing;
	int sourceIndexScanning;
public:
	void updateSettingsOnChange(obs_data_t* settings);
	void recreateStagingMemoryIfNeeded(obs_source_t* source);
	void freeDestroyEffectsAndTexRender();
	void freeDestroyFilterTextures();
	void freeDestroyNonGraphicData();
	void initFilterInGraphicsContext();
	void initFilterOutsideGraphicsContext(obs_source_t* context);
	void doRenderWorkUpdateTracking(obs_source_t* source, uint32_t rwidth, uint32_t rheight, bool shouldUpdateTrackingBox);
	void overlayDebugInfoOnInternalDataBuffer();
	void doRenderFromInternalMemoryToFilterOutput();
	void parseTextCordsString(const char* coordStrIn, int* x1, int* y1, int* x2, int* y2, int defx1, int defy1, int defx2, int defy2, int maxwidth, int maxheight);
	int parseCoordStr(const char* cpos, int max);
	//
	void forceUpdateFilterSettingsOnOptionChange();
	void updateCoordsAndOutputSize();
	void clearAllBoxReadies();
	void analyzeSceneAndFindTrackingBox();
	void analyzeSceneFindComponentMarkers();
	bool findNewCandidateTrackingBox();
	void updateZoomCropBoxFromCurrentCandidate();
	void doRenderAutocropBox(obs_source_t* source, int rwidth, int rheight);
	bool doCalculationsForZoomCrop();
	void markRegionsQuality();
	void clearFoundTrackingBox();
	bool calcIsValidmarkerRegion(JrRegionSummary* region);
	bool isOneValidRegionAtUnchangedLocation();
	bool isRegionSimilarBounds(JrRegionSummary* regiona, JrRegionSummary* regionb);
	void reRegisterHotkeys();
	void fillCoordsWithMarkerlessCoords(int* x1, int* y1, int* x2, int* y2);
	void update_src(int ix, const char* insrc_name);
	void releaseSources(JrPlugin *plugin);
	void initSources();
	void doRenderSourceOut(obs_source_t* source);
	//
	void checkAndUpdateAllSources();
	//
	void properties_add_source(obs_properties_t* pp, const char* name, const char* desc);
	obs_source_t* getCurrentSource();
	obs_source_t* get_source(int ix);
	//
	void jrAddPropertListChoices(obs_property_t* comboString, const char** choiceList);
	int jrAddPropertListChoiceFind(const char* strval, const char** choiceList);
	void RgbaDrawPixel(uint32_t* textureData, int ilinesize, int x, int y, int pixelVal);
	void RgbaDrawRectangle(uint32_t* textureData, int ilinesize, int x1, int y1, int x2, int y2, int pixelVal);
	//
	void freeSource(obs_source_t*);
	int jrSourceGetWidth(obs_source_t* src);
	int jrSourceGetHeight(obs_source_t* src);
	//
	void autoZoomSetEffectParamsChroma(uint32_t rwidth, uint32_t rheight);
	void autoZoomSetEffectParamsZoomCrop(uint32_t rwidth, uint32_t rheight);
	void myRenderSourceIntoTexture(obs_source_t* source, gs_texrender_t* tex, uint32_t stageWidth, uint32_t stageHeight, uint32_t sourceWidth, uint32_t sourceHeight);
	void myRenderEffectIntoTexture(obs_source_t* source, gs_texrender_t* tex, gs_effect_t* effect, gs_texrender_t* inputTex, uint32_t stageWidth, uint32_t stageHeight);
	void doRenderWorkFromEffectToStageTexRender(gs_effect_t* effectChroma, obs_source_t* source, uint32_t rwidth, uint32_t rheight);
	//
	void doRenderWorkFromStageToInternalMemory();
	void releaseSources();
	//
	void multiSourceTargetLost();
	void multiSourceTargetChanged();
};
//---------------------------------------------------------------------------




