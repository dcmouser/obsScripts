//---------------------------------------------------------------------------
#include <cstdio>
//
#include "myPlugin.h"
//---------------------------------------------------------------------------












//---------------------------------------------------------------------------
bool JrPlugin::initFilterInGraphicsContext() {
	// called once at creation startup

	texrender = NULL;
	texrender2 = NULL;

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

	// create texrender
	texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	texrender2 = gs_texrender_create(GS_RGBA, GS_ZS_NONE);

	// success?
	if (!effectChroma || !effectZoomCrop) {
		return false;
	}

	// success
	return true;
}



bool JrPlugin::initFilterOutsideGraphicsContext(obs_source_t* context) {
	// called once at creation startup
	needsRebuildStaging = true;
	in_enumSources = false;
	//
	workingWidth = 0;
	workingHeight = 0;
	stageShrink = -1;
	stageArea = 0;
	//
	dlinesize = 0;
	//
	markerlessCoordsX1 = markerlessCoordsY1 = markerlessCoordsX2 = markerlessCoordsY2 = 0;
	opt_markerlessCycleIndex = 0;
	markerlessSourceIndex = 0;
	//
	staging_texture = NULL;
	drawing_texture = NULL;
	data = NULL;
	// hotkeys init
	hotkeyId_ToggleAutoUpdate = hotkeyId_OneShotZoomCrop = hotkeyId_ToggleCropping = hotkeyId_ToggleDebugDisplay = hotkeyId_ToggleBypass = hotkeyId_CycleSource = -1;

	mutex = CreateMutexA(NULL, FALSE, NULL);

	clearAllBoxReadies();

	trackingOneShot = false;
	trackingUpdateCounter = 0;

	// clear sources
	stracker.init(this);

	mydebug("Back from calling stracker init.");

	// success
	return true;
}



void JrPlugin::reRegisterHotkeys() {
	obs_source_t* target = context;
	if (hotkeyId_ToggleAutoUpdate==-1) hotkeyId_ToggleAutoUpdate = obs_hotkey_register_source(target, "autoZoom.hotkeyToggleAutoUpdate", "1 autoZoom Toggle AutoUpdate", onHotkeyCallback, this);
	if (hotkeyId_OneShotZoomCrop==-1) hotkeyId_OneShotZoomCrop = obs_hotkey_register_source(target, "autoZoom.hotkeyOneShotZoomCrop", "2 autoZoom One-shot ZoomCrop", onHotkeyCallback, this);
	if (hotkeyId_ToggleCropping==-1) hotkeyId_ToggleCropping = obs_hotkey_register_source(target, "autoZoom.hotkeyToggleCropping", "3 autoZoom Toggle Cropping", onHotkeyCallback, this);
	if (hotkeyId_ToggleDebugDisplay==-1) hotkeyId_ToggleDebugDisplay = obs_hotkey_register_source(target, "autoZoom.hotkeyToggleDebugDisplay", "4 autoZoom Toggle Debug Display", onHotkeyCallback, this);
	if (hotkeyId_ToggleBypass==-1) hotkeyId_ToggleBypass = obs_hotkey_register_source(target, "autoZoom.hotkeyToggleBypass", "5 autoZoom Toggle Bypass", onHotkeyCallback, this);
	if (hotkeyId_CycleSource==-1) hotkeyId_CycleSource = obs_hotkey_register_source(target, "autoZoom.hotkeyCycleSource", "6 autoZoom cycle source", onHotkeyCallback, this);
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
	mydebug("In updateSettingsOnChange.");
	
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
	strncpy(opt_forcedOutputResBuf, obs_data_get_string(settings, SETTING_zcForcedOutputRes), 79);

	// markerless stuff
	//opt_markerlessSourceIndex = obs_data_get_int(settings, SETTING_zcMarkerlessSourceIndex);
	//strncpy(opt_markerlessCoordsBuf, obs_data_get_string(settings, SETTING_zcMarkerlessCoords), 79);
	strncpy(opt_markerlessCycleListBuf, obs_data_get_string(settings, SETTING_zcMarkerlessCycleList), DefMarkerlessCycleListBufMaxSize);

	// sources
	// here we parse the names of the sources the user has specified in the options, and record the names of these source; we will retrieve the source details and pointers later
	SourceTracker* strackerp = getSourceTrackerp();
	int userIndicatedSourceCount = obs_data_get_int(settings, SETTING_srcN);
	if (userIndicatedSourceCount <= 0)
		userIndicatedSourceCount = 1;
	else if (userIndicatedSourceCount >= DefMaxSources) {
		userIndicatedSourceCount = DefMaxSources - 1;
	}

	mydebug("In updateSettingsOnChange 2.");

	// ATTN: this will let go of all stored sources -- something the original code did not do
	strackerp->reviseSourceCount(userIndicatedSourceCount);
	for (int i = 0; i < userIndicatedSourceCount; i++) {
		char name[16];
		snprintf(name, sizeof(name), "src%d", i);
		strackerp->updateSourceIndexByName(i, obs_data_get_string(settings, name));
	}

	mydebug("In updateSettingsOnChange 3.");

	// ATTN: test just force curren to default markerless
	//sourceIndex = max(min(param_numSources-1, opt_markerlessSourceIndex), 0);
 
	// anything we need to force to rebuild?
	forceUpdatePluginSettingsOnOptionChange();

	mydebug("In updateSettingsOnChange 4.");
}
//---------------------------------------------------------------------------







//---------------------------------------------------------------------------
void JrPlugin::freeBeforeReallocateEffectsAndTexRender() {
	if (effectChroma) {
		gs_effect_destroy(effectChroma);
		effectChroma = NULL;
	}
	if (effectZoomCrop) {
		gs_effect_destroy(effectZoomCrop);
		effectZoomCrop = NULL;
	}
	if (texrender) {
		gs_texrender_destroy(texrender);
		texrender = NULL;
	}
	if (texrender2) {
		gs_texrender_destroy(texrender2);
		texrender2 = NULL;
	}
}

void JrPlugin::freeBeforeReallocateFilterTextures() {
	if (staging_texture) {
		gs_stagesurface_destroy(staging_texture);
		staging_texture = NULL;
	}
	if (drawing_texture) {
		gs_texture_destroy(drawing_texture);
		drawing_texture = NULL;
	}
}

void JrPlugin::freeBeforeReallocateNonGraphicData() {
	// source tracker release
	stracker.freeBeforeReallocate();

	// this may have to be done LAST?
	if (data) {
		bfree(data);
		data = NULL;
	}
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
void JrPlugin::addPropertyForASourceOption(obs_properties_t *pp, const char *name, const char *desc) {
	// add a propery list dropdown to let user choose a source by name
	obs_property_t *p;
	p = obs_properties_add_list(pp, name, desc, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	property_list_add_sources(p, context);
}
//---------------------------------------------------------------------------











//---------------------------------------------------------------------------
void JrPlugin::forceUpdatePluginSettingsOnOptionChange() {
	// mark it as needing rebuilding based on changes
	needsRebuildStaging = true;

	// we may need to updat this even if we bypass the rebuilding cycle
	// custom outputsize and coords -- used even outside render cycle or on bypass
	updateCoordsAndOutputSize();
}


void JrPlugin::updateCoordsAndOutputSize() {
	int dummyval;
	parseTextCordsString(opt_forcedOutputResBuf, &forcedOutputWidth, &forcedOutputHeight, &dummyval,&dummyval,workingWidth,workingHeight,0,0,workingWidth+1,workingHeight+1);

	outWidth = forcedOutputWidth;
	outHeight = forcedOutputHeight;

	updateMarkerlessCoordsCycle();
	
	//mydebug("in updateCoordsAndOutputSize %d x %d and outsize = %dx%d.", width, height, outWidth, outHeight);

	updateComputedOptions();
}


void JrPlugin::clearAllBoxReadies() {
	stracker.clearAllBoxReadies();
}
//---------------------------------------------------------------------------











//---------------------------------------------------------------------------
void JrPlugin::fillCoordsWithMarkerlessCoords(int* x1, int* y1, int* x2, int* y2) {
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
void JrPlugin::updateMarkerlessCoordsCycle() {
	// markerless source and coords say what should be shown when no markers are found -- whether this is the full source view or something more zoomed
	// parse the opt_markerlessCycleListBuf, and then fill markerlessCoords based on which opt_markerlessCycleIndex user has selected
	// gracefully handle when index is beyond what is found in the list, just correct and modulus it so it cycles around
	// note that ther markerlessCoords are only valid for the source# specified, so it is up to the plugin to switch to the desired source when moving to chosen markerless source and coords
	// note this only gets called on coming back from options or when hotkey triggers a change, so it can be "slowish"
	// format of opt_markerlessCycleListBuf is "1,0 | 2,0 | 2,1 | 2,2 | ...
	//
	bool bretv;
	char markerlessBufTemp[DefMarkerlessCycleListBufMaxSize];
	char sourceNumberBuf[80];
	char zoomLevelBuf[80];
	char* cpos;
	int entryIndex = 0;
	int sourceNumber;
	float zoomLevel;

	// temp copy so we can use strtok
	strcpy(markerlessBufTemp, opt_markerlessCycleListBuf);

	// go through opt_markerlessCycleListBuf and find the SOURCE#,ZOOMLEVEL pair currently selected
	// when we find it, set markerlesscoords based on that
	while (cpos = strtok(markerlessBufTemp, "|")) {
		// got an entry
		char* commaPos = strchr(cpos, ',');
		if (commaPos) {
			strncpy(sourceNumberBuf, cpos, commaPos - cpos);
			sourceNumberBuf[(commaPos - cpos)+1] = '\0';
			strcpy(zoomLevelBuf, commaPos + 1);
		} else {
			// missing a comma should we assume its zoom, source, or complain? let's assume its zoom for now
			strcpy(sourceNumberBuf, "");
			strcpy(zoomLevelBuf, cpos);
		}
		if (false) {
			bretv = setMarkerlessCoordsForSourceNumberAndZoomLevel(sourceNumber, zoomLevel);
		}
		// ATTN: unfinished
		break;
		// 
		++entryIndex;
	};
}


bool JrPlugin::setMarkerlessCoordsForSourceNumberAndZoomLevel(int sourceNumber, float zoomLevel) {
	// zoomLevel 0 means at original scale, and preserve aspect ratio -- this makes it ok to have black bars outside so the whole source is visible and fits LONGER dimension
	// loomLevel 1 and greater means preserve aspect ratio and ZOOM and fit (center) the SHORTER dimension, meaning there will never be black bars
	// the output aspect is outputWidth and outputHeight
	// and the source is queried source width and source height
	// gracefully handle case where source is zero sizes
	int sourceWidth = 0;
	int sourceHeight = 0;

	// sanity check
	if (outWidth <= 0 || outHeight <= 0) {
		return false;
	}

	// first get the source specified and its dimensions; if we can't find then

	// sanity check
	if (sourceWidth <= 0 || sourceHeight <= 0) {
		return false;
	}

	// now compute markerless coords for this source
	float outAspect = (float)outWidth / (float)outHeight;
	float sourceAspect = (float)sourceWidth / (float)sourceHeight;
	if (zoomLevel <= 0.0f) {
		// show entire source scaled to fit longest dimension (normal algorithm will figure out how to center and zoom to fit this)
		markerlessCoordsX1 = 0;
		markerlessCoordsX2 = sourceWidth;
		markerlessCoordsY1 = 0;
		markerlessCoordsY2 = sourceHeight;
	} else {
		// ok here we WANT a crop 
		if (sourceAspect > outAspect) {
			// we are longer aspect than the output, so we want to choose full height and crop out some of the sides
			float ydim = (float)sourceHeight / zoomLevel;
			float xdim = ydim * outAspect;
			float xmargin = (float)(sourceWidth - xdim) / 2.0f;
			float ymargin = (float)(sourceHeight - ydim) / 2.0f;
			markerlessCoordsX1 = (int) xmargin;
			markerlessCoordsX2 = sourceWidth - (int)xmargin;
			markerlessCoordsY1 = (int) ymargin;
			markerlessCoordsY2 = sourceHeight - (int)ymargin;
		}
		else {
			// we are taller than the output
			float xdim = (float)sourceWidth / zoomLevel;
			float ydim = xdim / outAspect;
			float xmargin = (float)(sourceWidth - xdim) / 2.0f;
			float ymargin = (float)(sourceHeight - ydim) / 2.0f;
			markerlessCoordsX1 = (int) xmargin;
			markerlessCoordsX2 = sourceWidth - (int)xmargin;
			markerlessCoordsY1 = (int) ymargin;
			markerlessCoordsY2 = sourceHeight - (int)ymargin;
		}
	}

	// successfully set markerless coords
	return true;
}
//---------------------------------------------------------------------------
