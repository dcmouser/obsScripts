#pragma once

//---------------------------------------------------------------------------
#include <windows.h>
#include <ctime>
//
// obs
#include <obs-module.h>
//
#include <graphics/matrix4.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
//
#include "obsHelpers.h"
//
#include "myPluginDefs.h"
//#include "jrRegionDetector.h"
#include "jrSourceTracker.h"
#include "source_list.h"
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




























//---------------------------------------------------------------------------
class JrPlugin {
public:
	HANDLE mutex;
	SourceTracker stracker;
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

	// fade effect
	gs_effect_t *effectFade;

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
	char opt_OutputSizeBuf[80];
	float forcedOutputAspect;
	int opt_zcEasing;

	bool opt_enableMarkerlessCoordinates;
	bool opt_enableAutoSourceSwitching;
	int opt_initialViewSourceIndex;

	// markerless stuff - for when we can't find markers, do we show entire view or some subset
	char opt_markerlessCycleListBuf[DefMarkerlessCycleListBufMaxSize];
	int opt_markerlessCycleIndex;
	float opt_fadeDuration;


	int markerlessSourceIndex;
	int stableSourceSwitchDesireCount;
	int markerlessCoordsX1, markerlessCoordsY1, markerlessCoordsX2, markerlessCoordsY2;

	// internals
	// source info
	//uint32_t workingWidth;
	//uint32_t workingHeight;
	uint32_t outputWidthAutomatic;
	uint32_t outputHeightAutomatic;
	int outputWidthPlugin;
	int outputHeightPlugin;

	// hotkeys
	obs_hotkey_id hotkeyId_ToggleAutoUpdate;
	obs_hotkey_id hotkeyId_OneShotZoomCrop;
	obs_hotkey_id hotkeyId_ToggleCropping;
	obs_hotkey_id hotkeyId_ToggleDebugDisplay;
	obs_hotkey_id hotkeyId_ToggleBypass;
	obs_hotkey_id hotkeyId_CycleSource;
	obs_hotkey_id hotkeyId_CycleMarkerlessList;
	obs_hotkey_id hotkeyId_CycleMarkerlessListBack;
	obs_hotkey_id hotkeyId_toggleMarkerlessUse;
	obs_hotkey_id hotkeyId_toggleAutoSourceSwitching;

	// to keep tracking of update rate and one-shot adjustment
	bool trackingOneShot;
	int trackingUpdateCounter;
	//
	bool in_enumSources;
	//
	bool sourcesHaveChanged;
	//
	// computed options
	float computedChangeMomentumDistanceThresholdMoving;
	float computedChangeMomentumDistanceThresholdStabilized;
	float computedThresholdTargetStableDistance;
	int computedThresholdTargetStableCount;
	float computedChangeMomentumDistanceThresholdDontDrift;
	float computedAverageSmoothingRateToCloseTarget;
	bool computedIntegerizeStableAveragingLocation;
	int computedMomentumCounterTargetMissingMarkers;
	int computedMomentumCounterTargetNormal;
	float computedMinDistFractionalBetweenMarkersToReject;
public:
	int fadeStartingSourceIndex, fadeEndingSourceIndex;
	float fadePosition;
	clock_t fadeStartTime;
	clock_t fadeEndTime;
	clock_t fadeDuration;
public:
	// note sure if this is right way to get the source represented by this plugin
	obs_source_t* getThisPluginSource() { return context; }
	//
	void updateSettingsOnChange(obs_data_t* settings);
	//void recheckSizeAndAdjustIfNeeded(obs_source_t* source);
	//void freeBeforeReallocateTexRender();
	//void freeBeforeReallocateFilterTextures();
	//void freeBeforeReallocateNonGraphicData();

	void freeBeforeReallocateEffects();
	bool initFilterInGraphicsContext();
	bool initFilterOutsideGraphicsContext();
	//
	void doTick();
	void doRender();
	//

	bool doCalculationsForZoomCropEffect(TrackedSource* tsourcep);

	void parseTextCordsString(const char* coordStrIn, int* x1, int* y1, int* x2, int* y2, int defx1, int defy1, int defx2, int defy2, int maxwidth, int maxheight);
	int parseCoordStr(const char* cpos, int max);
	//
	void forceUpdatePluginSettingsOnOptionChange();
	//
	void updateOutputSize();
	bool updateMarkerlessCoordsCycle();
	//
	bool setMarkerlessCoordsForSourceNumberAndZoomLevel(int sourceNumber, float zoomLevel, char* alignbuf);
	void markerlessCycleListAdvanceForward();
	void markerlessCycleListAdvanceBackward();
	void markerlessCycleListAdvanceDelta(int delta);

	void reRegisterHotkeys();
	void fillCoordsWithMarkerlessCoords(int* x1, int* y1, int* x2, int* y2);
	void fillCoordsWithMarkerlessCoords(float* x1, float* y1, float* x2, float* y2);
	void addPropertyForASourceOption(obs_properties_t* pp, const char* name, const char* desc);
	
	void jrAddPropertListChoices(obs_property_t* comboString, const char** choiceList);
	int jrAddPropertListChoiceFind(const char* strval, const char** choiceList);
	//
	void autoZoomSetEffectParamsChroma(uint32_t rwidth, uint32_t rheight);
	void autoZoomSetEffectParamsZoomCrop(uint32_t rwidth, uint32_t rheight);

	//
	SourceTracker* getSourceTrackerp() { return &stracker; };
	void multiSourceTargetLost();
	void multiSourceTargetChanged();
public:
	void updateComputedOptions();
	bool updateComputeAutomaticOutputSize();
public:
	void onSourcesHaveChanged();
	void setSourcesChanged(bool val) { sourcesHaveChanged = val; }
public:
	void initiateFade(int startingSourceIndex, int endingSourceIndex);
	void cancelFade();
	bool updateFadePosition();
	bool isFading() { return (fadeStartingSourceIndex != -1); }
};
//---------------------------------------------------------------------------




