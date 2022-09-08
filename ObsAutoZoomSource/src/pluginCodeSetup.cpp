//---------------------------------------------------------------------------
#include <cstdio>
//
#include "myPlugin.h"
//---------------------------------------------------------------------------












//---------------------------------------------------------------------------
bool JrPlugin::initFilterInGraphicsContext() {
	// called once at creation startup

	// chroma effect
	char *effectChroma_path = obs_module_file("ObsAutoZoomChroma.effect");
	effectChroma = gs_effect_create_from_file(effectChroma_path, NULL);
	if (effectChroma) {
		// effect .effect file uniform parameters
		chroma_param = gs_effect_get_param_by_name(effectChroma, "chroma_key");
		pixel_size_param = gs_effect_get_param_by_name(effectChroma, "pixel_size");
		similarity_param = gs_effect_get_param_by_name(effectChroma, "similarity");
		smoothness_param = gs_effect_get_param_by_name(effectChroma, "smoothness");
		//
		dbgImageBehindAlpha_param = gs_effect_get_param_by_name(effectChroma, "dbgImageBehindAlpha");
	}
	bfree(effectChroma_path);


	// zoomCrop effect
	char *effectZoomCrop_path = obs_module_file("ObsAutoZoomCrop.effect");
	effectZoomCrop = gs_effect_create_from_file(effectZoomCrop_path, NULL);
	if (effectZoomCrop) {
		// effect .effect file uniform parameters
		param_mul = gs_effect_get_param_by_name(effectZoomCrop, "mul_val");
		param_add = gs_effect_get_param_by_name(effectZoomCrop, "add_val");
		param_clip_ul = gs_effect_get_param_by_name(effectZoomCrop, "clip_ul");
		param_clip_lr = gs_effect_get_param_by_name(effectZoomCrop, "clip_lr");
	}
	bfree(effectZoomCrop_path);

	// fade effect
	char *effectFade_path = obs_module_file("ObsAutoZoomFade.effect");
	effectFade = gs_effect_create_from_file(effectFade_path, NULL);
	if (effectFade) {
		// effect .effect file uniform parameters
		/*
		param_mul = gs_effect_get_param_by_name(effectZoomCrop, "mul_val");
		param_add = gs_effect_get_param_by_name(effectZoomCrop, "add_val");
		param_clip_ul = gs_effect_get_param_by_name(effectZoomCrop, "clip_ul");
		param_clip_lr = gs_effect_get_param_by_name(effectZoomCrop, "clip_lr");
		*/
	}
	bfree(effectFade_path);


	// success?
	if (!effectChroma || !effectZoomCrop || !effectFade) {
		return false;
	}

	// success
	return true;
}



bool JrPlugin::initFilterOutsideGraphicsContext() {
	// called once at creation startup
	in_enumSources = false;
	//
	sourcesHaveChanged = false;
	//
	markerlessCoordsX1 = markerlessCoordsY1 = markerlessCoordsX2 = markerlessCoordsY2 = 0;
	opt_markerlessCycleIndex = 0;
	markerlessSourceIndex = 0;

	// hotkeys init
	hotkeyId_ToggleAutoUpdate = hotkeyId_OneShotZoomCrop = hotkeyId_ToggleCropping = hotkeyId_ToggleDebugDisplay = hotkeyId_ToggleBypass = hotkeyId_CycleSource = hotkeyId_CycleMarkerlessList = hotkeyId_CycleMarkerlessListBack = hotkeyId_toggleMarkerlessUse = hotkeyId_toggleAutoSourceSwitching = -1;
	mutex = CreateMutexA(NULL, FALSE, NULL);

	trackingOneShot = false;
	trackingUpdateCounter = 0;
	stableSourceSwitchDesireCount = 0;
	// reset
	cancelFade();

	// clear sources
	stracker.init(this);

	//mydebug("Back from calling stracker init.");

	// success
	return true;
}



void JrPlugin::reRegisterHotkeys() {
	obs_source_t* target = getThisPluginSource();
	if (hotkeyId_ToggleAutoUpdate==-1) hotkeyId_ToggleAutoUpdate = obs_hotkey_register_source(target, "autoZoom.hotkeyToggleAutoUpdate", "1 autoZoom Toggle AutoUpdate", onHotkeyCallback, this);
	if (hotkeyId_OneShotZoomCrop==-1) hotkeyId_OneShotZoomCrop = obs_hotkey_register_source(target, "autoZoom.hotkeyOneShotZoomCrop", "2 autoZoom One-shot ZoomCrop", onHotkeyCallback, this);
	if (hotkeyId_ToggleCropping==-1) hotkeyId_ToggleCropping = obs_hotkey_register_source(target, "autoZoom.hotkeyToggleCropping", "3 autoZoom Toggle Cropping", onHotkeyCallback, this);
	if (hotkeyId_ToggleDebugDisplay==-1) hotkeyId_ToggleDebugDisplay = obs_hotkey_register_source(target, "autoZoom.hotkeyToggleDebugDisplay", "4 autoZoom Toggle Debug Display", onHotkeyCallback, this);
	if (hotkeyId_ToggleBypass==-1) hotkeyId_ToggleBypass = obs_hotkey_register_source(target, "autoZoom.hotkeyToggleBypass", "5 autoZoom Toggle Bypass", onHotkeyCallback, this);
	if (hotkeyId_CycleSource==-1) hotkeyId_CycleSource = obs_hotkey_register_source(target, "autoZoom.hotkeyCycleSource", "6 autoZoom cycle source", onHotkeyCallback, this);
	//
	if (hotkeyId_CycleMarkerlessList==-1) hotkeyId_CycleMarkerlessList = obs_hotkey_register_source(target, "autoZoom.cycleMarkerlessList", "7 autoZoom cycle markerless list", onHotkeyCallback, this);
	if (hotkeyId_CycleMarkerlessListBack==-1) hotkeyId_CycleMarkerlessListBack = obs_hotkey_register_source(target, "autoZoom.cycleMarkerlessListBack", "8 autoZoom cycle markerless list back", onHotkeyCallback, this);
	
	if (hotkeyId_toggleMarkerlessUse==-1) hotkeyId_toggleMarkerlessUse = obs_hotkey_register_source(target, "autoZoom.toggleMarkerlessUse", "9 autoZoom toggle markerless use", onHotkeyCallback, this);
	if (hotkeyId_toggleAutoSourceSwitching==-1) hotkeyId_toggleAutoSourceSwitching = obs_hotkey_register_source(target, "autoZoom.toggleAutoSourceSwitching", "10 toggleaAuto source switching", onHotkeyCallback, this);
	//
	if (hotkeyId_ToggleAutoUpdate == -1) {
		info("Failed to registery hotkey for autoZoom source.");
	}
	else {
		info("Successfully registered hotkey for autoZoom source.");
	}
}
//---------------------------------------------------------------------------






//---------------------------------------------------------------------------
void JrPlugin::updateSettingsOnChange(obs_data_t *settings) {
	//mydebug("In updateSettingsOnChange.");
	
	int64_t similarity = obs_data_get_int(settings, SETTING_SIMILARITY);
	int64_t smoothness = obs_data_get_int(settings, SETTING_SMOOTHNESS);
	uint32_t key_color = (uint32_t)obs_data_get_int(settings, SETTING_KEY_COLOR);
	const char *key_type = obs_data_get_string(settings, SETTING_COLOR_TYPE);

	struct vec4 key_rgb;
	struct vec4 cb_v4;
	struct vec4 cr_v4;

	if (strcmp(key_type, "green") == 0)
		key_color = 0x00FF00;
	else if (strcmp(key_type, "blue") == 0)
		key_color = 0xFF9900;
	else if (strcmp(key_type, "magenta") == 0)
		key_color = 0xFF00FF;

	// chroma stuff
	vec4_from_rgba(&key_rgb, key_color | 0xFF000000);
	static const float cb_vec[] = {-0.100644f, -0.338572f, 0.439216f, 0.501961f};
	static const float cr_vec[] = {0.439216f, -0.398942f, -0.040274f, 0.501961f};
	memcpy(&cb_v4, cb_vec, sizeof(cb_v4));
	memcpy(&cr_v4, cr_vec, sizeof(cr_v4));
	opt_chroma.x = vec4_dot(&key_rgb, &cb_v4);
	opt_chroma.y = vec4_dot(&key_rgb, &cr_v4);
	//
	opt_similarity = (float)similarity / 1000.0f;
	opt_smoothness = (float)smoothness / 1000.0f;
	//
	opt_debugDisplay = obs_data_get_bool(settings, SETTING_debugDisplay);
	opt_filterBypass = obs_data_get_bool(settings, SETTING_filterBypass);
	opt_enableAutoUpdate = obs_data_get_bool(settings, SETTING_enableAutoUpdate);
	opt_updateRate = obs_data_get_int(settings, SETTING_updateRate);
	//
	opt_rmTDensityMin = (float)obs_data_get_int(settings, SETTING_rmDensityMin) / 100.0f;
	opt_rmTAspectMin = (float)obs_data_get_int(settings, SETTING_rmAspectMin) / 100.0f;
	opt_rmTSizeMin = obs_data_get_int(settings, SETTING_rmSizeMin);
	opt_rmTSizeMax = obs_data_get_int(settings, SETTING_rmSizetMax);
	opt_rmStageShrink = obs_data_get_int(settings, SETTING_rmStageShrink);
	//
	opt_zcPreserveAspectRatio = obs_data_get_bool(settings, SETTING_zcPreserveAspectRatio);
	//
	opt_zcMarkerPos = jrAddPropertListChoiceFind(obs_data_get_string(settings, SETTING_zcMarkerPos), (const char**)SETTING_zcMarkerPos_choices);
	opt_zcBoxMargin = obs_data_get_int(settings, SETTING_zcBoxMargin);
	opt_zcBoxMoveSpeed = obs_data_get_int(settings, SETTING_zcBoxMoveSpeed);
	opt_zcBoxMoveDistance = obs_data_get_int(settings, SETTING_zcBoxMoveDistance);
	opt_zcBoxMoveDelay = obs_data_get_int(settings, SETTING_zcBoxMoveDelay);
	opt_zcEasing = jrAddPropertListChoiceFind(obs_data_get_string(settings, SETTING_zcEasing), (const char**)SETTING_zcEasing_choices);
	//
	opt_zcAlign = jrAddPropertListChoiceFind(obs_data_get_string(settings, SETTING_zcAlignment), (const char**)SETTING_zcAlignment_choices);
	opt_zcMode = jrAddPropertListChoiceFind(obs_data_get_string(settings, SETTING_zcMode), (const char**)SETTING_zcMode_choices);
	opt_zcMaxZoom = (float)obs_data_get_int(settings, SETTING_zcMaxZoom) / 33.3f;
	//
	strncpy(opt_OutputSizeBuf, obs_data_get_string(settings, SETTING_zcOutputSize), 79);

	// markerless stuff
	opt_initialViewSourceIndex = obs_data_get_int(settings, SETTING_initialViewSourceIndex);
	opt_enableMarkerlessCoordinates = obs_data_get_bool(settings, SETTING_enableMarkerlessCoordinates);
	opt_enableAutoSourceSwitching = obs_data_get_bool(settings, SETTING_enableAutoSourceSwitching);
	//
	strncpy(opt_markerlessCycleListBuf, obs_data_get_string(settings, SETTING_zcMarkerlessCycleList), DefMarkerlessCycleListBufMaxSize);
	opt_markerlessCycleIndex = obs_data_get_int(settings, SETTING_markerlessCycleIndex);

	// sources
	// here we parse the names of the sources the user has specified in the options, and record the names of these source; we will retrieve the source details and pointers later
	SourceTracker* strackerp = getSourceTrackerp();
	int userIndicatedSourceCount = obs_data_get_int(settings, SETTING_srcN);
	if (userIndicatedSourceCount <= 0)
		userIndicatedSourceCount = 1;
	else if (userIndicatedSourceCount >= DefMaxSources) {
		userIndicatedSourceCount = DefMaxSources - 1;
	}

	//mydebug("In updateSettingsOnChange 2.");

	// ATTN: this will let go of all stored sources -- something the original code did not do
	strackerp->reviseSourceCount(userIndicatedSourceCount);
	for (int i = 0; i < userIndicatedSourceCount; i++) {
		char name[16];
		snprintf(name, sizeof(name), "src%d", i);
		strackerp->updateSourceIndexByName(i, obs_data_get_string(settings, name));
	}

	//mydebug("In updateSettingsOnChange 3.");

	// test
	opt_fadeDuration = 0.5f;

	// anything we need to force to rebuild?
	forceUpdatePluginSettingsOnOptionChange();

	// initialize view source
	stracker.setViewSourceIndex(max(opt_initialViewSourceIndex-1,0), false);
}
//---------------------------------------------------------------------------







//---------------------------------------------------------------------------
void JrPlugin::freeBeforeReallocateEffects() {
	if (effectChroma) {
		gs_effect_destroy(effectChroma);
		effectChroma = NULL;
	}
	if (effectZoomCrop) {
		gs_effect_destroy(effectZoomCrop);
		effectZoomCrop = NULL;
	}
	if (effectFade) {
		gs_effect_destroy(effectFade);
		effectFade = NULL;
	}
}
//---------------------------------------------------------------------------







//---------------------------------------------------------------------------
void JrPlugin::addPropertyForASourceOption(obs_properties_t *pp, const char *name, const char *desc) {
	// add a propery list dropdown to let user choose a source by name
	obs_property_t *p;
	p = obs_properties_add_list(pp, name, desc, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	property_list_add_sources(p, getThisPluginSource());
}
//---------------------------------------------------------------------------











//---------------------------------------------------------------------------
void JrPlugin::forceUpdatePluginSettingsOnOptionChange() {
	// tell any sources they may need internal updating
	stracker.onOptionsChange();

	// we may need to updat this even if we bypass the rebuilding cycle
	// custom outputsize and coords -- used even outside render cycle or on bypass
	updateOutputSize();
	updateMarkerlessCoordsCycle();

	// any computed options we need to recompute
	updateComputedOptions();
}


void JrPlugin::updateOutputSize() {
	// recompute output size of US

	updateComputeAutomaticOutputSize();

	int dummyval;
	parseTextCordsString(opt_OutputSizeBuf, &outputWidthPlugin, &outputHeightPlugin, &dummyval,&dummyval,outputWidthAutomatic,outputHeightAutomatic,0,0,outputWidthAutomatic+1,outputHeightAutomatic+1);
	//mydebug("in updateOutputSize %d x %d and outsize = %dx%d.", outputWidthAutomatic, outputHeightAutomatic, outputWidthPlugin, outputHeightPlugin);
}


bool JrPlugin::updateComputeAutomaticOutputSize() {
	int maxw, maxh;
	stracker.computerMaxSourceDimensions(maxw, maxh);
	//mydebug("Automatic out dimensions are %d,%d.", maxw, maxh);
	if (maxw == 0 || maxh == 0) {
		// defaults
		maxw = 1920;
		maxh = 1080;
	}
	if (maxw != outputWidthAutomatic || maxh != outputHeightAutomatic) {
		// changes
		outputWidthAutomatic = maxw;
		outputHeightAutomatic = maxh;
		return true;
	}
	// no change
	return false;
}
//---------------------------------------------------------------------------











//---------------------------------------------------------------------------
void JrPlugin::fillCoordsWithMarkerlessCoords(int* x1, int* y1, int* x2, int* y2) {
	*x1 = markerlessCoordsX1;
	*y1 = markerlessCoordsY1;
	*x2 = markerlessCoordsX2;
	*y2 = markerlessCoordsY2;
}

void JrPlugin::fillCoordsWithMarkerlessCoords(float* x1, float* y1, float* x2, float* y2) {
	*x1 = markerlessCoordsX1;
	*y1 = markerlessCoordsY1;
	*x2 = markerlessCoordsX2;
	*y2 = markerlessCoordsY2;
}
//---------------------------------------------------------------------------






//---------------------------------------------------------------------------
void JrPlugin::updateComputedOptions() {
	computedChangeMomentumDistanceThresholdMoving = DefChangeMomentumDistanceThresholdMoving;
	computedChangeMomentumDistanceThresholdStabilized = DefChangeMomentumDistanceThresholdStabilized;
	computedThresholdTargetStableDistance = DefThresholdTargetStableDistance;
	computedThresholdTargetStableCount = DefThresholdTargetStableCount;
	computedChangeMomentumDistanceThresholdDontDrift = DefChangeMomentumDistanceThresholdDontDrift;
	computedAverageSmoothingRateToCloseTarget = DefAverageSmoothingRateToCloseTarget;
	computedIntegerizeStableAveragingLocation = DefIntegerizeStableAveragingLocation;
	computedMomentumCounterTargetMissingMarkers = DefMomentumCounterTargetMissingMarkers;
	computedMomentumCounterTargetNormal = DefMomentumCounterTargetNormal;
	computedMinDistFractionalBetweenMarkersToReject = DefMinDistFractionalBetweenMarkersToReject;
}
//---------------------------------------------------------------------------







//---------------------------------------------------------------------------
void  JrPlugin::initiateFade(int startingSourceIndex, int endingSourceIndex) {
	if (opt_fadeDuration < 0.0001f) {
		// fade disabled
		return;
	}

	fadeStartingSourceIndex = startingSourceIndex;
	fadeEndingSourceIndex = endingSourceIndex;
	fadePosition = 0.0f;
	fadeStartTime = clock();
	fadeDuration = opt_fadeDuration * (float)CLOCKS_PER_SEC;
	fadeEndTime = fadeStartTime + fadeDuration;
}

void  JrPlugin::cancelFade() {
	fadeStartingSourceIndex = -1;
	fadePosition = 1.0f;
}

bool JrPlugin::updateFadePosition() {
	if (fadeStartingSourceIndex == -1) {
		return false;
	}
	clock_t curtime = clock();
	if (curtime > fadeEndTime) {
		cancelFade();
		return false;
	}
	fadePosition = (float)(fadeEndTime-curtime) / (float)fadeDuration;
	return true;
}
//---------------------------------------------------------------------------




