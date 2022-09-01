//---------------------------------------------------------------------------
#include <cstdio>
//
#include "ObsAutoZoomSource.h"
#include "ObsAutoZoomSourceDefs.h"
//
#include "obsHelpers.h"
//---------------------------------------------------------------------------


















//---------------------------------------------------------------------------
void JrPlugin::initFilterInGraphicsContext() {
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
}



void JrPlugin::initFilterOutsideGraphicsContext(obs_source_t* context) {
	// called once at creation startup
	needsRebuildStaging = true;
	in_enum = false;
	//
	workingWidth = 0;
	workingHeight = 0;
	stageShrink = -1;
	stageArea = 0;
	//
	dlinesize = 0;
	//
	markerlessCoordsX1 = markerlessCoordsY1 = markerlessCoordsX2 = markerlessCoordsY2 = 0;
	//
	staging_texture = NULL;
	drawing_texture = NULL;
	data = NULL;
	// hotkeys init
	hotkeyId_ToggleAutoUpdate = hotkeyId_OneShotZoomCrop = hotkeyId_ToggleCropping = hotkeyId_ToggleDebugDisplay = hotkeyId_ToggleBypass = -1;

	mutex = CreateMutexA(NULL, FALSE, NULL);

	clearAllBoxReadies();

	trackingOneShot = false;
	trackingUpdateCounter = 0;

	// list of sources
	initSources();
	lastSourceIndex = -1;

	regionDetector.rdInit();
}



void JrPlugin::reRegisterHotkeys() {
	obs_source_t* target = context;
	if (hotkeyId_ToggleAutoUpdate==-1) hotkeyId_ToggleAutoUpdate = obs_hotkey_register_source(target, "autoZoom.hotkeyToggleAutoUpdate", "autoZoom Toggle AutoUpdate", onHotkeyCallback, this);
	if (hotkeyId_OneShotZoomCrop==-1) hotkeyId_OneShotZoomCrop = obs_hotkey_register_source(target, "autoZoom.hotkeyOneShotZoomCrop", "autoZoom One-shot ZoomCrop", onHotkeyCallback, this);
	if (hotkeyId_ToggleCropping==-1) hotkeyId_ToggleCropping = obs_hotkey_register_source(target, "autoZoom.hotkeyToggleCropping", "autoZoom Toggle Cropping", onHotkeyCallback, this);
	if (hotkeyId_ToggleDebugDisplay==-1) hotkeyId_ToggleDebugDisplay = obs_hotkey_register_source(target, "autoZoom.hotkeyToggleDebugDisplay", "autoZoom Toggle Debug Display", onHotkeyCallback, this);
	if (hotkeyId_ToggleBypass==-1) hotkeyId_ToggleBypass = obs_hotkey_register_source(target, "autoZoom.hotkeyToggleBypass", "autoZoom Toggle Bypass", onHotkeyCallback, this);
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
	opt_markerlessSourceIndex = obs_data_get_int(settings, SETTING_zcMarkerlessSourceIndex);
	strncpy(opt_markerlessCoordsBuf, obs_data_get_string(settings, SETTING_zcMarkerlessCoords), 79);
	//
	// sources
	n_src = obs_data_get_int(settings, SETTING_srcN);
	if (n_src <= 0)
		n_src = 1;
	else if (n_src > DefMaxSources)
		n_src = DefMaxSources;
	for (int i = 0; i < DefMaxSources && i < n_src; i++) {
		char name[16];
		snprintf(name, sizeof(name), "src%d", i);
		update_src(i, obs_data_get_string(settings, name));
	}

	// ATTN: test just force curren to default markerless
	sourceIndex = max(min(n_src-1, opt_markerlessSourceIndex), 0);
 
	// anything we need to force to rebuild?
	forceUpdateFilterSettingsOnOptionChange();
}
//---------------------------------------------------------------------------







//---------------------------------------------------------------------------
void JrPlugin::freeDestroyEffectsAndTexRender() {
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

void JrPlugin::freeDestroyFilterTextures() {
	if (staging_texture) {
		gs_stagesurface_destroy(staging_texture);
		staging_texture = NULL;
	}
	if (drawing_texture) {
		gs_texture_destroy(drawing_texture);
		drawing_texture = NULL;
	}
}

void JrPlugin::freeDestroyNonGraphicData() {
	//
	regionDetector.rdFree();
	//
	// ATTN: 8/31/22 -- this seems to be causing an occasional OBS crash on exit, not sure why
	// since we ONLY do it on exit, its safe to not do it
	if (false) {
		releaseSources();
	}

	// this may have to be done LAST?
	if (data) {
		bfree(data);
		data = NULL;
	}
}


// reaallocate staging memory stuff if needed
void JrPlugin::recreateStagingMemoryIfNeeded(obs_source_t* source) {
	if (!source) {
		// delete EVERYTHING if we can't find source
		workingWidth = 0;
		workingHeight = 0;
		//
		clearAllBoxReadies();
		//
		// is this part really needed? or will we do this when we need to resize..
		if (true) {
			obs_enter_graphics();
			freeDestroyFilterTextures();
			obs_leave_graphics();
			freeDestroyNonGraphicData();
		}
		return;
	}

	// update if size changes
	bool update = false;
	uint32_t sourceWidth = jrSourceGetWidth(source);
	uint32_t sourceHeight = jrSourceGetHeight(source);
	//mydebug("recreateStagingMemoryIfNeeded 2 with dimensions %dx%d.",width,height);

	if (needsRebuildStaging || workingWidth != sourceWidth || workingHeight != sourceHeight || opt_rmStageShrink != (int)stageShrink) {
		//mydebug("recreateStagingMemoryIfNeeded 3.");		

		// did size change? if so we will reset boxes
		bool sizeChanged = false;
		if (workingWidth != sourceWidth || workingHeight != sourceHeight || opt_rmStageShrink != (int)stageShrink) {
			sizeChanged = true;
		}

		update = true;

		// mutex surround -- not sure if needed
		WaitForSingleObject(mutex, INFINITE);

		// set width and height based on source
		workingWidth = sourceWidth;
		workingHeight = sourceHeight;
		// output size reduction
		stageShrink = opt_rmStageShrink;
		stageWidth = workingWidth / stageShrink;
		stageHeight = workingHeight / stageShrink;
		//
		stageArea = stageWidth * stageHeight;

		// custom outputsize stuff
		updateCoordsAndOutputSize();

		// mark current boxes as invalid
		if (sizeChanged) {
			clearAllBoxReadies();
		}

		obs_enter_graphics();
		freeDestroyFilterTextures();
		//
		// allocate staging area based on DESTINATION size?
		staging_texture = gs_stagesurface_create(stageWidth, stageHeight, GS_RGBA);
		// new drawing texture
		drawing_texture = gs_texture_create(stageWidth, stageHeight, GS_RGBA, 1, NULL, GS_DYNAMIC);
		//
		obs_leave_graphics();

		info("JR Created drawing and staging texture %d by %d (%d x %d): %x", workingWidth, workingHeight, stageWidth, stageHeight,  staging_texture);

		freeDestroyNonGraphicData();

		// realloc internal memory
		data = (uint8_t *) bzalloc((stageWidth + 32) * stageHeight * 4);

		// region detect helper
		regionDetector.rdResize(stageWidth, stageHeight);

		// clear flag saying we need rebuild
		needsRebuildStaging = false;

		// done in protected mutex area
		ReleaseMutex(mutex);
	}

	// track this
	lastSourceIndex = sourceIndex;
}


void JrPlugin::forceUpdateFilterSettingsOnOptionChange() {
	// mark it as needing rebuilding based on changes
	needsRebuildStaging = true;

	// we may need to updat this even if we bypass the rebuilding cycle
	// custom outputsize and coords -- used even outside render cycle or on bypass
	updateCoordsAndOutputSize();
}


void JrPlugin::updateCoordsAndOutputSize() {
	int dummyval;
	parseTextCordsString(opt_forcedOutputResBuf, &forcedOutputWidth, &forcedOutputHeight, &dummyval,&dummyval,workingWidth,workingHeight,0,0,workingWidth+1,workingHeight+1);
	parseTextCordsString(opt_markerlessCoordsBuf, &markerlessCoordsX1, &markerlessCoordsY1, &markerlessCoordsX2, &markerlessCoordsY2,0,0,-1,-1,workingWidth,workingHeight);

	outWidth = forcedOutputWidth;
	outHeight = forcedOutputHeight;

	//mydebug("in updateCoordsAndOutputSize %d x %d and outsize = %dx%d.", width, height, outWidth, outHeight);
}


void JrPlugin::clearAllBoxReadies() {
	trackingBoxReady = false;
	cropBoxReady = false;
	priorBoxReady = false;
	trackingBoxCompReady = false;
	//
	dbx1 = dby1 = dbx2 = dby2 = 0;
	tbx1 = tby1 = tbx2 = tby2 = 0;
	priorx1 = priory1 = priorx2 = priory2 = 0;
	//
	changeTargetMomentumCounter = 0;
}
//---------------------------------------------------------------------------

























































//---------------------------------------------------------------------------
void JrPlugin::autoZoomSetEffectParamsChroma(uint32_t rwidth, uint32_t rheight) {
	// setting params for effects file .effect
	struct vec2 pixel_size;
	vec2_set(&pixel_size, 1.0f / (float)rwidth, 1.0f / (float)rheight);
	gs_effect_set_vec2(chroma_param, &opt_chroma);
	gs_effect_set_vec2(pixel_size_param, &pixel_size);
	gs_effect_set_float(similarity_param, opt_similarity);
	gs_effect_set_float(smoothness_param, opt_smoothness);
	//
	// note this is a FLOAT in the effects plugin
	gs_effect_set_float(dbgImageBehindAlpha_param, DefEffectAlphaShowImage);
}


void JrPlugin::autoZoomSetEffectParamsZoomCrop(uint32_t rwidth, uint32_t rheight) {
	gs_effect_set_vec2(param_mul, &mul_val);
	gs_effect_set_vec2(param_add, &add_val);
	gs_effect_set_vec2(param_clip_ul, &clip_ul);
	gs_effect_set_vec2(param_clip_lr, &clip_lr);
}
//---------------------------------------------------------------------------














































































//---------------------------------------------------------------------------
// this took me about 7 hours of pure depressive hell to figure out
//
void JrPlugin::myRenderSourceIntoTexture(obs_source_t* source, gs_texrender_t *tex, uint32_t stageWidth, uint32_t stageHeight, uint32_t sourceWidth, uint32_t sourceHeight) {
	// render source onto a texture

	//mydebug("myRenderSourceIntoTexture (%d,%d) (%d,%d) (%d,%d).", stageWidth, stageHeight, sourceWidth, sourceHeight, outWidth, outHeight);

	// seems critical wtf
	if (tex) {
		gs_texrender_reset(tex);
	}
	if (tex && !gs_texrender_begin(tex, stageWidth, stageHeight)) {
		return;
	}

	// save blend state
	gs_blend_state_push();

	// blend mode
	//gs_blend_function_separate(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA, GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	bool custom_draw = (OBS_SOURCE_CUSTOM_DRAW) != 0;
	bool async = (0 & OBS_SOURCE_ASYNC) != 0;
	struct vec4 clear_color;
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
	gs_ortho(0.0f, (float)sourceWidth, 0.0f, (float)sourceHeight, -100.0f, 100.0f);
	// RENDER THE SOURCE
	obs_source_video_render(source);
	//
	// end stuff
	gs_blend_state_pop();
	if (tex) {
		gs_texrender_end(tex);
	}
}


void JrPlugin::myRenderEffectIntoTexture(obs_source_t* source, gs_texrender_t *tex, gs_effect_t* effect, gs_texrender_t *inputTex, uint32_t stageWidth, uint32_t stageHeight) {
	// render effect onto texture using an input texture (set effect params before invoking)

	if (tex) {
		gs_texrender_reset(tex);
	}
	if (tex && !gs_texrender_begin(tex, stageWidth, stageHeight)) {
		return;
	}

	// save blend state
	gs_blend_state_push();

	// blend mode
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	// tell chroma effect to use the rendered image as its input
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_texture_t *obsInputTex = gs_texrender_get_texture(inputTex);
	gs_effect_set_texture(image, obsInputTex);

	struct vec4 clear_color;
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
	//
	// ATTN: this seems to be important for plugin property dialog preview
	gs_ortho(0.0f, (float)stageWidth, 0.0f, (float)stageHeight, -100.0f, 100.0f);

	while (gs_effect_loop(effect, "Draw")) {
		gs_draw_sprite(NULL, 0, stageWidth, stageHeight);
	}

	// end stuff
	gs_blend_state_pop();
	if (tex) {
		gs_texrender_end(tex);
	}
}
//---------------------------------------------------------------------------











//---------------------------------------------------------------------------
void JrPlugin::doRenderWorkFromEffectToStageTexRender(gs_effect_t* effectChroma, obs_source_t* source, uint32_t rwidth, uint32_t rheight) {
	// STAGE 1 render SOURCE
	// render into texrender2 (temporary) texture - to a stage x stage dimension
	myRenderSourceIntoTexture(source, texrender2, stageWidth, stageHeight, rwidth, rheight);

	// STAGE 2 - render effect
	// first set effect params
	autoZoomSetEffectParamsChroma(rwidth, rheight);
	// then render effect into target texture texrender
	myRenderEffectIntoTexture(source, texrender, effectChroma, texrender2, stageWidth, stageHeight);
}
//---------------------------------------------------------------------------













//---------------------------------------------------------------------------
void JrPlugin::doRenderWorkFromStageToInternalMemory() {
	bool stageMemoryReady = false;
	if (data) {
		// default OBS plugin since we don't want to do anything
		gs_effect_t *effectDefault = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		gs_texture_t *tex = gs_texrender_get_texture(texrender);
		if (tex) {
			gs_stage_texture(staging_texture, tex);
			//
			uint8_t *idata;
			uint32_t ilinesize;
			WaitForSingleObject(mutex, INFINITE);
			if (gs_stagesurface_map(staging_texture, &idata, &ilinesize)) {
				//mydebug("ATTN: JR gs_stage_texture 2.\n");
				memcpy(data, idata, ilinesize * stageHeight);
				// update our stored data linesize
				dlinesize = ilinesize;
				gs_stagesurface_unmap(staging_texture);
				stageMemoryReady = true;
				}
			ReleaseMutex(mutex);
		}
	}
}



void JrPlugin::markRegionsQuality() {
	JrRegionSummary* region;
	int foundValidCount = 0;
	for (int i = 0; i < regionDetector.foundRegions; ++i) {
		region = &regionDetector.regions[i];
		region->valid = calcIsValidmarkerRegion(region);
		if (region->valid) {
			++foundValidCount;
		}
	}
	regionDetector.foundRegionsValid = foundValidCount;
}


bool JrPlugin::calcIsValidmarkerRegion(JrRegionSummary* region) {
	// valid region based on plugin options
	if (region->pixelCount == 0) {
		return false;
	}

	if (region->density < opt_rmTDensityMin) {
		return false;
	}

	if (region->aspect < opt_rmTAspectMin) {
		return false;
	}

	double relativeArea1000 = (sqrt(region->area) / sqrt(stageArea)) * 1000.0;
	//mydebug("Commparing region %d area of (%d in %d) relative (%f) against min of %f.", region->label, region->area, stageArea, relativeArea1000, opt_rmTSizeMin);
	if (relativeArea1000 < opt_rmTSizeMin || (opt_rmTSizeMax>0 && relativeArea1000 > opt_rmTSizeMax) ) {
		return false;
	}

	// valid
	return true;
}



void JrPlugin::overlayDebugInfoOnInternalDataBuffer() {
	// overlay debug 
	int x1, y1, x2, y2;
	int pixelVal;

	if (true) {
		// show all regions
		JrRegionSummary* region;
		//int pixelVal = 0x00FF00FF;
		pixelVal = 0xFF0000FF;
		for (int i = 0; i < regionDetector.foundRegions; ++i) {
			region = &regionDetector.regions[i];
			//mydebug("Overlaying region %d (%d,%d,%d,%d).", i, region->x1, region->y1, region->x2, region->y2);
			//pixelVal = calcIsValidmarkerRegion(region) ? 0x88FF55FF : 0xFF0000FF;
			pixelVal = calcIsValidmarkerRegion(region) ?   0x88FF55FA : 0xFF0000FF;
			x1 = region->x1-1;
			y1 = region->y1-1;
			x2 = region->x2+1;
			y2 = region->y2+1;
			//
			if (x1 < 0) { x1 = 0; }
			if (y1 < 0) { y1 = 0; }
			if (x2 >= stageWidth) { x2 = stageWidth-1; }
			if (y2 >= stageHeight) { y2 = stageHeight-1; }
			//
			RgbaDrawRectangle((uint32_t*)data, dlinesize, x1, y1, x2, y2, pixelVal);
		}
	}

	// show found cropable area
	if (true && trackingBoxReady && (tbx1 != dbx1 || tby1 != dby1 || tbx2 != dbx2 || tby2 != dby2)) {
		// modify contents of our internal data to show border around box
		//mydebug("Overlaying tracking box (%d,%d,%d,%d).", tbx1, tby1, tbx2, tby2);
		pixelVal = 0xFF22FFFF;
		x1 = tbx1;
		y1 = tby1;
		x2 = tbx2;
		y2 = tby2;

		RgbaDrawRectangle((uint32_t*)data, dlinesize, (float)x1/stageShrink, (float)y1/stageShrink, (float)x2/stageShrink, (float)y2/stageShrink, pixelVal);
	}
	if (true && cropBoxReady) {
		// modify contents of our internal data to show border around box
		//mydebug("Overlaying crop box (%d,%d,%d,%d).", bx1, by1, bx2, by2);
		pixelVal = 0xFFFFFFFF;
		x1 = dbx1;
		y1 = dby1;
		x2 = (float)dbx2;
		y2 = (float)dby2;
		if (x2 > workingWidth-1) {
			x2 = workingWidth - 1;
		}
		if (y2 > workingHeight-1) {
			y2 = workingHeight - 1;
		}
		RgbaDrawRectangle((uint32_t*)data, dlinesize, (float)x1/stageShrink, (float)y1/stageShrink, (float)x2/stageShrink, (float)y2/stageShrink, pixelVal);
	}
}
	
	
	
void JrPlugin::doRenderFromInternalMemoryToFilterOutput() {
	bool flagCreateNewTex = false;
	bool flagUseDrawingTex = true;

	// try to render to screen the previously generated texture (possibly scaling up)

	// default OBS plugin since we don't want to do anything
	gs_effect_t *effectDefault = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_texture_t *tex = gs_texrender_get_texture(texrender);
	gs_texture_t* texOut = NULL;
	gs_texture_t* texRender = tex;

	// copy our data modified pixels into a texture to render
	//mydebug("doRenderFromInternalMemoryToFilterOutput (%d,%d,%d,%d).", stageWidth, stageHeight, outWidth, outHeight);

	// test write out to drawing texture for plugin output
	// see https://gitlab.hs-bremerhaven.de/jpeters/obs-studio/-/blob/master/test/test-input/sync-pair-vid.c
	// see https://discourse.urho3d.io/t/urho3d-as-plugin-for-obs-studio/6849/7
	uint8_t *ptr;
	uint32_t ilinesize;
	if (flagCreateNewTex) {
		// this will be destroyed at end
		texRender = gs_texture_create(stageWidth, stageHeight, GS_RGBA, 1, NULL, GS_DYNAMIC);
	} else if (flagUseDrawingTex) {
		texRender = drawing_texture;			
	}

	// copy from our internal buffer to the output texture in RGBA format
	if (gs_texture_map(texRender, &ptr, &ilinesize)) {
		//mydebug("ATTN: JR inside texture map1: %d.\n",ilinesize);	
		//fill_texture((uint32_t *)ptr, 0xFFFFFFFF);
		memcpy((uint32_t*)ptr, data, ilinesize * stageHeight);
		gs_texture_unmap(texRender);
	}


	// render from texRender to plugin output
	gs_eparam_t *image = gs_effect_get_param_by_name(effectDefault, "image");
	gs_effect_set_texture(image, texRender);
	// just a simple draw of texture
	// test
	if (false) {
		gs_enable_depth_test(false);
		//gs_set_cull_mode(GS_NEITHER);
		//gs_ortho(0.0f, (float)stageWidth, 0.0f, (float)stageHeight, -100.0f, 100.0f);
		//gs_set_viewport(0, 0, stageWidth, stageHeight);
	}
	//
	// note we are willing to scale it up to full size from smaller stage
	while (gs_effect_loop(effectDefault, "Draw")) {
		gs_draw_sprite(texRender, 0, outWidth, outHeight);
	}

	// free temporary tex?
	if (flagCreateNewTex) {
		gs_texture_destroy(texRender);
	}
}
















void JrPlugin::doRenderWorkUpdateTracking(obs_source_t* source, uint32_t rwidth, uint32_t rheight, bool shouldUpdateTrackingBox) {

	// part 1
	// Render to intermediate target texrender instead of output of plugin (screen)
	doRenderWorkFromEffectToStageTexRender(effectChroma, source,  rwidth,  rheight);

	// part 2
	// ok now the output is in texrender texture where we can map it and copy it to private user memory
	doRenderWorkFromStageToInternalMemory();

	// part 3
	// update autotracking by doing machine vision on internal memory copy of effectChroma output
	if (shouldUpdateTrackingBox) {
		analyzeSceneAndFindTrackingBox();
	}
}
//---------------------------------------------------------------------------



























//---------------------------------------------------------------------------
// see source_render
void JrPlugin::doRenderSourceOut(obs_source_t* source) {
	// attempts to make it size properly in properties
	uint32_t rwidth = jrSourceGetWidth(source);
	uint32_t rheight = jrSourceGetHeight(source);
	if (rwidth == 0 || rheight == 0) {
		//mydebug("in doRenderSourceOut aborting because of dimensions 0.");
		return;
	}
	// render it out
	myRenderSourceIntoTexture(source, NULL, rwidth, rheight, rwidth, rheight);
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
void JrPlugin::analyzeSceneAndFindTrackingBox() {

	// step 1 - build labeling matrix
	// this does the possibly heavy duty computation of Connected Components Contour Tracing, etc.
	analyzeSceneFindComponentMarkers();

	// step 2 identify the new candidate tracking box in the view
	// this may find nothing reliable -- that's ok
	findNewCandidateTrackingBox();
}




void JrPlugin::analyzeSceneFindComponentMarkers() {
	// step 1 build initial labels
	int foregroundPixelCount = regionDetector.fillFromStagingMemory(data, dlinesize);

	// step 2 - connected component labelsing
	int regionCountFound = regionDetector.doConnectedComponentLabeling();

	// debug
	//mydebug("JrObs CCL regioncount = %d (%d foregroundpixels) regionsize = [%d,%d].", regionCountFound, foregroundPixelCount, regionDetector.width, regionDetector.height);
}



bool JrPlugin::findNewCandidateTrackingBox() {
	// reset to no box found
	clearFoundTrackingBox();


	if (false) {
		// TEST
		tbx1 = 400;
		tby1 = 135;
		tbx2 = 1000;
		tby2 = 535;
		trackingBoxReady = true;
		regionDetector.foundRegions = 0;
		return true;
	}


	// mark regions quality to being markers
	markRegionsQuality();

	JrRegionDetector* rd = &regionDetector;;
	JrRegionSummary* region;

	// abort if not at least 2 regions
	if (rd->foundRegionsValid != 2) {
		// we need exactly 2 regions of validity to identify a new box
		// ATTN: TODO - we might actually be able to work with less or more IFF our current markers are CONSISTENT
		return false;
	}

	// ok now find the valid regions
	int validRegionCounter = 0;
	int validRegionIndices[2];
	for (int i = 0; i < rd->foundRegions; ++i) {
		region = &rd->regions[i];
		if (region->valid) {
			validRegionIndices[validRegionCounter] = i;
			if (++validRegionCounter >= 2) {
				break;
			}
		}
	}

	// region1 and region 2
	JrRegionSummary* region1=&rd->regions[validRegionIndices[0]];
	JrRegionSummary* region2=&rd->regions[validRegionIndices[1]];

	int posType = opt_zcMarkerPos;
	int tx1, ty1, tx2, ty2;
	int posMult = -1;
	if (posType==1) {
		// inside the markers (exclude the markers themselves)
		posMult = 1;
		if (region1->x1 <= region2->x2) {
			tx1 = region1->x2;
			tx2 = region2->x1;
		}
		else {
			tx1 = region2->x2;
			tx2 = region1->x1;
		}
		if (region1->y1 <= region2->y2) {
			ty1 = region1->y2;
			ty2 = region2->y1;
		}
		else {
			ty1 = region2->y2;
			ty2 = region1->y1;
		}
	}
	else {
		// outside the markers?
		float xszhalf1 = abs(region1->x2 - region1->x1) / 2.0f;
		float xszhalf2 = abs(region2->x2 - region2->x1) / 2.0f;
		float yszhalf1 = abs(region1->y2 - region1->y1) / 2.0f;
		float yszhalf2 = abs(region2->y2 - region2->y1) / 2.0f;
		if (region1->x1 <= region2->x2) {
			tx1 = region1->x1 + (posType==0 ? 0 : xszhalf1);
			tx2 = region2->x2 - (posType==0 ? 0 : xszhalf2);
		}
		else {
			tx1 = region2->x1 + (posType==0 ? 0 : xszhalf1);
			tx2 = region1->x2 - (posType==0 ? 0 : xszhalf2);
		}
		if (region1->y1 <= region2->y2) {
			ty1 = region1->y1 + (posType==0 ? 0 : yszhalf1);
			ty2 = region2->y2 - (posType==0 ? 0 : yszhalf2);
		}
		else {
			ty1 = region2->y1 + (posType==0 ? 0 : yszhalf1);
			ty2 = region1->y2 - (posType==0 ? 0 : yszhalf2);
		}
	}


	// last chance to reject if too close
	// inner distance (closet points distance)
	float itx1, ity1, itx2, ity2;
	if (region1->x1 <= region2->x2) {
		itx1 = region1->x2;
		itx2 = region2->x1;
	}
	else {
		itx1 = region2->x2;
		itx2 = region1->x1;
	}
	if (region1->y1 <= region2->y2) {
		ity1 = region1->y2;
		ity2 = region2->y1;
	}
	else {
		ity1 = region2->y2;
		ity2 = region1->y1;
	}
	// ATTN: unfinished.. easiest way is to let user specify min distance
	// im not sure what a good heuristic size would be
	float minDist = ((float)workingWidth / DefMinDistFractionalBetweenMarkersToReject) / stageShrink;
	if (max(ity2 - ity1, itx2 - itx1) < minDist) {
		return false;
	}


	// rescale from stage to original resolution
	tbx1 = tx1 * stageShrink + opt_zcBoxMargin * posMult;
	tby1 = ty1 * stageShrink + opt_zcBoxMargin * posMult;
	tbx2 = tx2 * stageShrink - opt_zcBoxMargin * posMult;
	tby2 = ty2 * stageShrink - opt_zcBoxMargin * posMult;


	// new check if they are inline then we treat them as indicator of bottom or left
	bool horzInline = false;
	bool vertInline = false;
	//
	if (false) {
		// orig way
		if ((region1->y1 <= region2->y2 && region1->y2 >= region2->y1) || (region2->y1 <= region1->y2 && region2->y2 >= region1->y1)) {
			// horizontally inline
			horzInline = true;
		}
		else if ((region1->x1 <= region2->x2 && region1->x2 >= region2->x1) || (region2->x1 <= region1->x2 && region2->x2 >= region1->x1)) {
			// vertically inline
			vertInline = true;
		}
	} else {
		// new test
		// we check if midpoints are as close as avg region size, with a bonus slack allowed more distance if the aspect ratio is very skewed
		if (true) {
			float midpointDist = fabs(((float)(region1->x1 + region1->x2) / 2.0f) - ((float)(region2->x1 + region2->x2) / 2.0f));
			float avgRegionSize = fabs(((float)(region1->x2 - region1->x1) + (region2->x2 - region2->x1)) / 2.0f);
			//float aspectBonus = abs(min((float)(tby2 - tby1),1.0f) / min((float)(tbx2 - tbx1),1.0f));
			float distx = (float)max(abs(tbx2 - tbx1), 1.0f);
			float disty = (float)max(abs(tby2 - tby1), 1.0f);
			float aspectBonus = disty / distx;
			float distThresh = avgRegionSize * (1.0f + 5.0f * min(aspectBonus, 20.0) / 20.0f);
			//info("minpointdist = %f  Avg RegionSize = %f  aspectbonus = %f distthresh = %f (distx = %f, disty=%f)", midpointDist, avgRegionSize, aspectBonus, distThresh,distx,disty);
			if (midpointDist <= distThresh) {
				vertInline = true;
			}
		}
		if (true) {
			float midpointDist = fabs(((float)(region1->y1 + region1->y2) / 2.0f) - ((float)(region2->y1 + region2->y2) / 2.0f));
			float avgRegionSize = (float)((region1->y2 - region1->y1) + (region2->y2 - region2->y1)) / 2.0f;
			float distx = (float)max(abs(tbx2 - tbx1), 1.0f);
			float disty = (float)max(abs(tby2 - tby1), 1.0f);
			float aspectBonus = distx / disty;
			float distThresh = avgRegionSize * (1.0f + 5.0f * min(aspectBonus, 20.0) / 20.0f);
			if (midpointDist <= distThresh) {
				horzInline = true;
			}
		}
	}

	// inline aspect ratios
	if (horzInline) {
		// so we leave the x values alone and move y2 up to match aspect
		float aspectRatio = (float)outHeight / (float)outWidth;
		float sz = ((float)((tbx2 - tbx1) + 1.0f) * aspectRatio) + 1.0f;
		tby2 = (float)tby1 + sz;
		if (tby2 > workingHeight - 1) {
			int dif = tby2 - (workingHeight - 1);
			tby2 = workingHeight - 1;
			tby1 = tby2 - sz;
		}
	}
	else if (vertInline) {
		// so we leave the y values alone and move x2 up to match aspect
		float aspectRatio = (float)outWidth / (float)outHeight;
		float sz = ((float)((tby2 - tby1) + 1.0f) * aspectRatio) - 1.0f;
		tbx2 = (float)tbx1 + sz;
		if (tbx2 > workingWidth-1) {
			int dif = tbx2 - (workingWidth-1);
			tbx2 = workingWidth-1;
			tbx1 = tbx2 - sz;
		}
	}

	// clean up bounds
	if (tbx1 < 0) {
		tbx1 = 0;
	}
	if (tby1 < 0) {
		tby1 = 0;
	}
	if (tbx1 >= workingWidth) {
		tbx1 = workingWidth-1;
	}
	if (tby1 >= workingHeight) {
		tby1 = workingHeight-1;
	}
	if (tbx2 < 0) {
		tbx2 = 0;
	}
	if (tby2 < 0) {
		tby2 = 0;
	}
	if (tbx2 >= workingWidth) {
		tbx2 = workingWidth-1;
	}
	if (tby2 >= workingHeight) {
		tby2 = workingHeight-1;
	}

	// tracking box now has valid data
	trackingBoxReady = true;

	// remember last two good regions
	memcpy(&lastGoodMarkerRegion1, region1, sizeof(JrRegionSummary));
	memcpy(&lastGoodMarkerRegion2, region2, sizeof(JrRegionSummary));

	return true;
}




void JrPlugin::updateZoomCropBoxFromCurrentCandidate() {
	// ok we have a CANDIDATE box tbx1,tby1,tbx2,tby2 and/or a planned next place to go
	// now we want to change our actual zoomCrop box towards this

	// current box locations
	int tx1, ty1, tx2, ty2;
	// planned targets to move to
	int x1, y1, x2, y2;

	// stickiness
	int changeMomentumCountTrigger = DefMomentumCounterTargetNormal;

	// ok get current target location (either of found box, or of whole screen if no markers found)
	if (trackingBoxReady) {
		// we have a new box, set targets to it
		tx1 = tbx1;
		ty1 = tby1;
		tx2 = tbx2;
		ty2 = tby2;
	} else {
		// we have not found a valid tracking box
		changeMomentumCountTrigger = DefMomentumCounterTargetMissingMarkers;
		// so we have some options now -- let go of any current zoomcrop, or wait, or smoothly move, etc...
		// should we assume missing markers are a target to turn off all crops?
		// ATTN: we now avoid JrPlugin::returning here and just return the set markerless coordinates as a valid box whenever this happens
		if (false && (!cropBoxReady || (!opt_enableAutoUpdate && !trackingOneShot))) {
			// there is no current crop AND there is no target box, AND we are not supposed to be changing the crop, so we return
			return;
		}
		// we dont have a new target, but we are zoomcropped to somewhere, so make new target full screen
		// set targets to full size (we can use this if we want to slowly adjust to new target of full screen)
		// IMPORTANT: if we are going to assume no markers mean go wide screen, we should have max delay before changing to this wide screen, in case we've just lost a tracking point

		// if we found one marker (not two), and it's at prior location, just assume the other got briefly occluded and do nothing
		if (isOneValidRegionAtUnchangedLocation()) {
			//info("Checking for isOneValidRegionAtUnchangedLocation returned true.");
			tx1 = priorx1;
			ty1 = priory1;
			tx2 = priorx2;
			ty2 = priory2;
		}
		else {
			fillCoordsWithMarkerlessCoords(&tx1, &ty1, &tx2, &ty2);
		}
	}

	// if no current box, then treat the whole screen as our starting (current) point
	if (!cropBoxReady) {
		fillCoordsWithMarkerlessCoords(&dbx1, &dby1, &dbx2, &dby2);
	}

	// should we immediately move towards our chosen target, or do we want some stickiness at current location or previous target to avoid JrPlugin::jitters and momentary missing markers?
	if (!priorBoxReady) {
		// we dont have a prior target, so we use current location as our prior "sticky" location
		priorx1 = dbx1;
		priory1 = dby1;
		priorx2 = dbx2;
		priory2 = dby2;
	}

	// always true at this point
	priorBoxReady = true;

	// at this point we have our current crop (bx1...) and our newly found tracking box (tbx1) and our PRIOR box location (priorx1)

	// ok now decide whether to move to the new box (tx values or stick with next)

	// if we wanted to be sticky and ignore new target box, just leave priorx alone
	// if we want to immediately start moving to new target, set priorx to tx
	bool switchToNewTarget = false;
	bool averageToCloseTarget = false;
	if (trackingOneShot) {
		switchToNewTarget = true;
	}

	// decide how much distance away triggers a need to change
	float changeMomentumThreshold;
	if (changeTargetMomentumCounter >= changeMomentumCountTrigger) {
		// if we have exceeded stickiness and are now moving, then we insist on getting CLOSER to target than normal before we start moving
		changeMomentumThreshold = DefChangeMomentumDistanceThresholdMoving;
	} else {
		// once we are stabilized we require more deviation before we break off and start moving again
		changeMomentumThreshold = DefChangeMomentumDistanceThresholdStabilized;
	}

	// keep track of when target is in motion
	bool newTargetInMotion = true;
	float thisTargetDelta;
	if (!trackingBoxCompReady) {
		// reset to default
		targetStableCount = 0;
	} else {
		// ok we can compare the new change in target location and accumulate it, so we we figure out when target is in motion
		float thisTargetDelta = abs(tx1 - tx1comp) + abs(ty1 - ty1comp) + abs(tx2 - tx2comp) + abs(ty2 - ty2comp);
		if (thisTargetDelta < DefThresholdTargetStableDistance) {
			// target has stayed still, increase stable count
			++targetStableCount;
			if (targetStableCount > DefThresholdTargetStableCount) {
				newTargetInMotion = false;
			}
		} else {
			// target moved too much, reset counter
			targetStableCount = 0;
		}
	}
	//
	// update
	tx1comp = tx1;
	ty1comp = ty1;
	tx2comp = tx2;
	ty2comp = ty2;
	trackingBoxCompReady = true;

	// compute distance of new target from current position
	float newTargetDeltaBoxAll = abs(tx1-dbx1) + abs(ty1-dby1) + abs(tx2-dbx2) + abs(ty2-dby2);
	float newTargetDeltaPriorAll = fabs(tx1-priorx1) + fabs(ty1-priory1) + fabs(tx2-priorx2) + fabs(ty2-priory2);
	//
	if (newTargetInMotion) {
		// when new target is in motion, we avoid JrPlugin::making any changes
		// instead we wait for target to stabilize then we move
	} else if (newTargetDeltaBoxAll > changeMomentumThreshold) {
		if (newTargetDeltaPriorAll < changeMomentumThreshold) {
			// too close to prior goal, so let it drift but dont pick new target, and dont reset counter so we will be fast to react
			// this also helps us more clearly know when we are choosing a dramatically new target and not set switchToNewTarget when doing minor tweaks
			averageToCloseTarget = true;
		} else {
			// new target is far enough way, lets build some momentum to switch to it
			changeTargetMomentumCounter += 1;
			if (changeTargetMomentumCounter >= changeMomentumCountTrigger) {
				// ok we have exceeded stickiness, we are ready to move to it... UNLESS the target itself is in motion, in which case we might wait until target stabilizes to its new location...
				// 
				// should we reset momentum counter? NO we want to quickly move to it once we start moving; only when we stop and stabilize do we reset
				// instead we clamp it to current value so that it wont grow so high as to trigger on missing markers
				// but note that because we don't reset this, we will rapidly be switching to new target over and over again once this triggers
				// ATTN: do we want to check if the new target is different from current target and not keep doing this if so -- so we only trigger on FIRST change of target
				changeTargetMomentumCounter = changeMomentumCountTrigger;
				// switch to it
				switchToNewTarget = true;
			}
		}
	} else {
		// target is where we are very near, so reset momentum
		changeTargetMomentumCounter = 0;
		// but let's drift slowly to new target averaging out
		if (newTargetDeltaBoxAll> DefChangeMomentumDistanceThresholdDontDrift) {
			averageToCloseTarget = true;
		}
	}

	// one shot mode always snaps to new target location, as if this target was long stable
	if (trackingOneShot) {
		switchToNewTarget = true;
	}

	// immediately switch over to move to newly found target box?
	if (switchToNewTarget) {
		// make our new target the newly found box
		priorx1 = tx1;
		priory1 = ty1;
		priorx2 = tx2;
		priory2 = ty2;
	}
	else if (averageToCloseTarget) {
		// we consider ourselves close enough to the new target that we arent going to do anything, but we can drift average out towards it
		// the averageing should stop jitter but the drifting should keep us from locking on a point 1 pixel away that is wrong
		priorx1 = (DefAverageSmoothingRateToCloseTarget * (float)priorx1)  + ((1.0f - DefAverageSmoothingRateToCloseTarget) * (float)tx1);
		priory1 = (DefAverageSmoothingRateToCloseTarget * (float)priory1)  + ((1.0f - DefAverageSmoothingRateToCloseTarget) * (float)ty1);
		priorx2 = (DefAverageSmoothingRateToCloseTarget * (float)priorx2)  + ((1.0f - DefAverageSmoothingRateToCloseTarget) * (float)tx2);
		priory2 = (DefAverageSmoothingRateToCloseTarget * (float)priory2)  + ((1.0f - DefAverageSmoothingRateToCloseTarget) * (float)ty2);

		// this is weird but letting prior vals be floats causes painful flickering of size when locations flicker
		if (DefIntegerizeStableAveragingLocation) {
			priorx1 = (int)priorx1;
			priory1 = (int)priory1;
			priorx2 = (int)priorx2;
			priory2 = (int)priory2;
		}
	}


	// ok now we have our TARGET updated in priorx
	// ok move to next chosen target (may be our current location)
	x1 = priorx1;
	y1 = priory1;
	x2 = priorx2;
	y2 = priory2;



	// some options at this point are:
	// 1. instantly switch to the new box
	// 2. smoothly move to the new box

	// move towards new box
	// ATTN: TODO easing - https://easings.net/#easeInOutCubic
	if (opt_zcBoxMoveSpeed == 0 || opt_zcEasing == 0 ) {
		// instant jump to new target
		dbx1 = x1;
		dby1 = y1;
		dbx2 = x2;
		dby2 = y2;
	} else if (opt_zcEasing == 1 || true) {
		// gradually move towards new box

		// we are go to morph/move MIDPOINT to MIDPOINT and dimensions to dimensions
		// delta(change) measurements
		float xmid = (float)(x1 + x2) / 2.0f;
		float ymid = (float)(y1 + y2) / 2.0f;
		float bxmid = (float)(dbx1 + dbx2) / 2.0f;
		float bymid = (float)(dby1 + dby2) / 2.0f;
		float deltaXMid = (xmid - bxmid);
		float deltaYMid = (ymid - bymid);
		float bwidth = dbx2 - dbx1;
		float bheight = dby2 - dby1;
		float deltaWidth = abs(x2 - x1) - fabs(bwidth);
		float deltaHeight = abs(y2 - y1) - fabs(bheight);
		float largestDelta = jrmax4(fabs(deltaXMid), fabs(deltaYMid), fabs(deltaWidth), fabs(deltaHeight));
		//float largestDelta = jrmax4(abs(deltaXMid), abs(deltaYMid), abs(deltaWidth*deltaHeight));
		float baseMoveSpeed = opt_zcBoxMoveSpeed;
		// modify speed upward based on distance to cover
		float moveSpeedMult = 50.0f;
		float maxMoveSpeed = baseMoveSpeed * (1.0f + moveSpeedMult * (largestDelta / (float)max(workingWidth, workingHeight)));
		float safeDelta = min(largestDelta, maxMoveSpeed);
		float percentMove = min(safeDelta / largestDelta, 1.0);
		// info("Debugging boxmove deltaWidth = %f  deltaHeight = %f  deltaXMid = %f deltaYmid = %f   largestDelta = %f  safeDelta = %f   percentMove = %f.",deltaWidth, deltaHeight, deltaXMid, deltaYMid, largestDelta, safeDelta, percentMove);
		//
		// adjust b midpoints and dimensions
		bxmid += deltaXMid * percentMove;
		bymid += deltaYMid * percentMove;
		bwidth += deltaWidth * percentMove;
		bheight += deltaHeight * percentMove;

		// now set x1, etc. baded on width and new dimensions
		dbx1 = bxmid - (bwidth / 2.0f);
		dby1 = bymid - (bheight / 2.0f);
		dbx2 = dbx1 + bwidth;
		dby2 = dby1 + bheight;

	} else if (opt_zcEasing == 2) {
		// eased movement
	}

	// note that we have a cropable box (this does not go false just because we dont find a new one -- it MIGHT stay valid)
	cropBoxReady = true;
}



void JrPlugin::clearFoundTrackingBox() {
	tbx1 = tby1 = tbx2 = tby2 = -1;
	trackingBoxReady = false;
}
//---------------------------------------------------------------------------






//---------------------------------------------------------------------------
bool JrPlugin::isOneValidRegionAtUnchangedLocation() {
	// return true if we found one region at same location as prior tracking box
	if (!priorBoxReady) {
		return false;
	}
	if (regionDetector.foundRegionsValid != 1) {
		return false;
	}

	JrRegionSummary* region;
	for (int i = 0; i < regionDetector.foundRegions; ++i) {
		region = &regionDetector.regions[i];
		if (region->valid) {
			// found the valid region
			// is one of the corners of the region possible at priorx1 or priorx2?
			if (isRegionSimilarBounds(region, &lastGoodMarkerRegion1)) {
				return true;
			}
			if (isRegionSimilarBounds(region, &lastGoodMarkerRegion2)) {
				return true;
			}
			break;
		}
	}
	return false;
}


bool JrPlugin::isRegionSimilarBounds(JrRegionSummary* regiona, JrRegionSummary* regionb) {
	float delta = abs(regiona->x1 - regionb->x1) + abs(regiona->x2 - regionb->x2) + abs(regiona->y1 - regionb->y1) + abs(regiona->y2 - regionb->y2);
	//
	//info("Checking for isOneValidRegionAtUnchangedLocation A comp (%d,%d) vs (%d,%d) and (%d,%d) vs (%d,%d) dist = %f.",regiona->x1,regiona->y1,regionb->x1,regionb->y1, regiona->x2,regiona->y2,regionb->x2,regionb->y2,delta);
	if (delta <= DefThresholdDistanceForPhantomMarker) {
		// close enough
		return true;
	}
	return false;
}
//---------------------------------------------------------------------------








//---------------------------------------------------------------------------
void JrPlugin::doRenderAutocropBox(obs_source_t* source, int rwidth, int rheight) {
	// ATTN: TODO - run crop and zoom effect


	// ATTN: we only need to really do this when box location changes
	if (true) {
		doCalculationsForZoomCrop();
	}

	// now render

	// STAGE 1 render SOURCE
	// render into texrender2 (temporary) texture - to a stage x stage dimension
	myRenderSourceIntoTexture(source, texrender2, rwidth, rheight, rwidth, rheight);

	// STAGE 2 - render effect to output
	// first set effect params
	autoZoomSetEffectParamsZoomCrop(rwidth, rheight);
	// then render effect into target texture texrender
	myRenderEffectIntoTexture(source, NULL, effectZoomCrop, texrender2, rwidth, rheight);
}





bool JrPlugin::doCalculationsForZoomCrop() {
	// this code need refactoring
	//float width = width;
	//float height = height;

	float sourceWidth = workingWidth;
	float sourceHeight = workingHeight;

	float oWidth = outWidth;
	float oHeight = outHeight;


	// sanity checks
	if (sourceWidth <= 0 || sourceHeight <= 0 || oWidth<=0 || oHeight<=0) {
		return false;
	}

	
	// target region size
	int bx1 = dbx1;
	int by1 = dby1;
	int bx2 = dbx2+1;
	int by2 = dby2+1;

	//info("Debugging b vals: %d,%d,%d,%d.", bx1, by1, bx2, by2);

	int cropWidth = (bx2-bx1);
	int cropHeight = (by2-by1);

	// sanity check
	if (cropWidth <= 1 || cropHeight <= 1) {
		return false;
	}






	// actual effect clipping crop is simple, either none or around box
	// clipping in 0-1 scale (default to none)
	if (opt_zcMode == 2) {
		// no crop clipping
		vec2_zero(&clip_ul);
		if (true) {
			//clip_lr.x = max(bx2,(float)oWidth) / (float)sourceWidth;
			//clip_lr.y = max(by2,(float)oHeight) / (float)sourceHeight;
			clip_lr.x = (float)(oWidth) / (float)sourceWidth;
			clip_lr.y = (float)(oHeight) / (float)sourceHeight;
		}
	} else {
		// crop clip box
		clip_ul.x = (float)bx1 / sourceWidth;
		clip_lr.x = (float)(bx2) / sourceWidth;
		clip_ul.y = (float)by1 / sourceHeight;
		clip_lr.y = (float)(by2) / sourceHeight;
	}


	// allignment prep
	int halign[9] = { -1,0,1,-1,0,1,-1,0,1 };
	int valign[9] = { -1,-1,-1,0,0,0,1,1,1 };
	int opt_halign = halign[opt_zcAlign];
	int opt_valign = valign[opt_zcAlign];


	// ok now we calculate scaling
	// if full zoom and no aspect ratio
	float scaleX = (float)cropWidth / oWidth;
	float scaleY = (float)cropHeight / oHeight;
	// enforce max zoom limit?
	if (opt_zcMode == 1) {
		// no zoom (just crop)
		//scaleX = scaleY = 1.0f;
		cropWidth = sourceWidth;
		cropHeight = sourceHeight;
		scaleX = (float)sourceWidth / oWidth;
		scaleY = (float)sourceHeight / oHeight;
	}
	if (opt_zcMaxZoom > 0.01f) {
		// max zoom cap
		float zoomScale = min(1.0 / opt_zcMaxZoom,1.0f);
		scaleX = max(scaleX, zoomScale);
		scaleY = max(scaleY, zoomScale);
	}
	// now adjust for aspect ratio constraints -- force them to be same zoom, whichever is LESS (so more will be shown in one dimension than we wanted)
	if (opt_zcPreserveAspectRatio) {
		if (scaleX > scaleY) {
			scaleY = scaleX;
		}
		else if (scaleY > scaleX) {
			scaleX = scaleY;
		}
	}



	// effect scaling -- straighforward enough
	mul_val.x = scaleX;
	mul_val.y = scaleY;

	// alignment is the more tricky part..
	// calculate add offset value

	float leftPixelX, topPixelY;

	// ok now that we have zoom scale we can calculate extents of our area of interest
	float resizedWidth = (float)cropWidth / scaleX;
	float resizedHeight = (float)cropHeight / scaleY;
	// and the amount of space around the zoomed area (should be 0 on full non-aspect constrained zoom)
	float surroundX = oWidth - resizedWidth;
	float surroundY = oHeight - resizedHeight;

	// align

	// calculation of top left offset for drawing
	if (opt_halign == -1) {
		// left
		leftPixelX = 0;
	}
	else if (opt_halign == 0) {
		// center
		leftPixelX = surroundX / 2.0;
	}
	else if (opt_halign == 1) {
		// right
		leftPixelX = surroundX;
	}
	if (opt_valign == -1) {
		// top
		topPixelY = 0;
	}
	else if (opt_valign == 0) {
		// center
		topPixelY = surroundY / 2.0;
	}
	else if (opt_valign == 1) {
		// bottom
		topPixelY = surroundY;
	}
	vec2_zero(&add_val);
	//

	if (opt_zcMode == 1) {
		add_val.x = (float)( - leftPixelX * scaleX) / sourceWidth;
		add_val.y = (float)( - topPixelY * scaleY) / sourceHeight;
	}
	else {
		add_val.x = (float)(bx1 - leftPixelX * scaleX) / sourceWidth;
		add_val.y = (float)(by1 - topPixelY * scaleY) / sourceHeight;
		if (opt_zcMode == 2) {
			// cropping clipping in zoom mode normally crops to source size but if window output is forced smalller we need to clip to that
			vec2_zero(&clip_ul);
			float cropXextra = (surroundX - leftPixelX) * scaleX;
			float cropYextra = (surroundY - topPixelY) * scaleY ;
			clip_lr.x = ((float)bx2 + cropXextra) / sourceWidth;
			clip_lr.y = ((float)by2 + cropYextra) / sourceHeight;
		}
	}

	return true;
}
//---------------------------------------------------------------------------




























































//---------------------------------------------------------------------------
// from multisource plugin

void JrPlugin::update_src(int ix, const char *insrc_name)
{
	if (!insrc_name || !*insrc_name)
		return;

	if (src_name[ix] && strcmp(src_name[ix], insrc_name) == 0) {
		// already set, nothing needs to be done
		return;
	}

	// save name -- this will be used to get a pointer in the tick function
	strcpy(src_name[ix], insrc_name);

	//bfree(src_name[ix]);
	//src_name[ix] = bstrdup(src_name);

	// free it? what's happening here does this mean we are asking to re-get it on next loop?
	// i think so, i think we recheck it on every tick
	if (src_ref[ix]) {
		obs_weak_source_release(src_ref[ix]);
		src_ref[ix] = NULL;
	}
}


obs_source_t *JrPlugin::get_source(int ix)
{
	if (src_ref[ix])
		return obs_weak_source_get_source(src_ref[ix]);
	return NULL;
}




void JrPlugin::properties_add_source(obs_properties_t *pp, const char *name, const char *desc)
{
	obs_property_t *p;
	p = obs_properties_add_list(pp, name, desc, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	property_list_add_sources(p, context);
}








void JrPlugin::releaseSources() {
	for (int i = 0; i < DefMaxSources; i++)
		if (src_ref[i]) {
			obs_weak_source_release(src_ref[i]);
		}
}

void JrPlugin::initSources() {
	for (int i = 0; i < DefMaxSources; i++)
		src_ref[i] = NULL;
}




obs_source_t* JrPlugin::getCurrentSource() {
	obs_source_t* source = get_source(sourceIndex);
	if (source == NULL) {
		//mydebug("Current source returned NULL, with index %d of %d.", sourceIndex, n_src);
	}
	return source;
}

void JrPlugin::freeSource(obs_source_t* source) {
	obs_source_release(source);
}


int JrPlugin::jrSourceGetWidth(obs_source_t* src) {
	//return obs_source_get_base_width(src);
	return obs_source_get_width(src);
}

int JrPlugin::jrSourceGetHeight(obs_source_t* src) {
	//return obs_source_get_base_height(src);
	return obs_source_get_height(src);
}
//---------------------------------------------------------------------------
















//---------------------------------------------------------------------------
void JrPlugin::multiSourceTargetLost() {
	// when we lose a target, we consider checking the wider camera (source with lower values index)
	// if we can't re-aquire targets on the lowest source then we switch to option markerless source index and markerless view (by default full view)
	// note that this new view may NOT be the widest source(!)
	// but note that regardless of our default markerless source view, when we are looking to reaquire markers, we will always be scanning the widest (lowest index) source
}


void JrPlugin::multiSourceTargetChanged() {
	// when we find a new target in the CURRENT source, before we will switch and lock on to it, we try to find it in the most zoomed in source, and switch to it there.
}
//---------------------------------------------------------------------------



























//---------------------------------------------------------------------------
void JrPlugin::parseTextCordsString(const char* coordStrIn, int* x1, int* y1, int* x2, int* y2, int defx1, int defy1, int defx2, int defy2, int maxwidth, int maxheight) {
	char coordStr[80];
	strcpy(coordStr, coordStrIn);

	char* cpos;
	if (cpos = strtok(coordStr, ",")) {
		*x1 = parseCoordStr(cpos, maxwidth);
	} else {
		*x1 = defx1 >=0 ? defx1 : maxwidth+defx1;
	}
	if (cpos = strtok(NULL, ",")) {
		*y1 = parseCoordStr(cpos, maxheight);
	} else {
		*y1 = defy1 >=0 ? defy1 : maxheight+defy1;
	}
	if (cpos = strtok(NULL, ",")) {
		*x2 = parseCoordStr(cpos, maxwidth);
	} else {
		*x2 = defx2 >=0 ? defx2 : maxwidth+defx2;
	}
	if (cpos = strtok(NULL, ",")) {
		*y2 = parseCoordStr(cpos, maxheight);
	} else {
		*y2 = defy2 >=0 ? defy2 : maxheight+defy2;
	}

	// info("Parsed cords (%s): %d, %d, %d, %d.", coordStrIn, *x1, *y1, *x2, *y2);
}


int JrPlugin::parseCoordStr(const char* cpos, int max) {
	int ival = atoi(cpos);
	if (ival < 0) {
		return max + ival;
	}
	return ival;
}
//---------------------------------------------------------------------------






//---------------------------------------------------------------------------
void JrPlugin::jrAddPropertListChoices(obs_property_t* comboString, const char** choiceList) {
	for (int i = 0;; ++i) {
		if (choiceList[i] == NULL || strcmp(choiceList[i], "") == 0) {
			break;
		}
	obs_property_list_add_string(comboString, choiceList[i],choiceList[i]);
	}
}

int JrPlugin::jrAddPropertListChoiceFind(const char* strval, const char** choiceList) {
	for (int i = 0;; ++i) {
		if (choiceList[i] == NULL || strcmp(choiceList[i], "") == 0) {
			break;
		}
		if (strcmp(choiceList[i], strval) == 0) {
			return i;
		}
	}
	// not found
	return -1;
}
//---------------------------------------------------------------------------









//---------------------------------------------------------------------------
// for debugging
void JrPlugin::RgbaDrawPixel(uint32_t *textureData, int ilinesize, int x, int y, int pixelVal) {
	textureData[y * (ilinesize/4) + x] = pixelVal;
}

void JrPlugin::RgbaDrawRectangle(uint32_t *textureData, int ilinesize, int x1, int y1, int x2, int y2, int pixelVal) {
	for (int iy=y1; iy<=y2; ++iy) {
		RgbaDrawPixel(textureData, ilinesize, x1, iy, pixelVal);
		RgbaDrawPixel(textureData, ilinesize, x2, iy, pixelVal);
	}
	for (int ix=x1; ix<=x2; ++ix) {
		RgbaDrawPixel(textureData, ilinesize, ix, y1, pixelVal);
		RgbaDrawPixel(textureData, ilinesize, ix, y2, pixelVal);
	}
}
//---------------------------------------------------------------------------








//---------------------------------------------------------------------------
void JrPlugin::checkAndUpdateAllSources() {
	for (int i = 0; i < DefMaxSources && i < n_src; i++) {
		if (!src_name[i] || !*src_name[i])
			continue;

		// get a NEW weak ref to our STORED source in this slot; this MUST be released at end of this function
		obs_source_t *oldSrc = get_source(i);

		if (src_name[i]) {
			if (oldSrc) {
				//mydebug("Examining stored source '%s' ref %d: %s with NON-NULL oldSrc.", src_name[i], i, src_name[i]);
			} else {
				//mydebug("Examining stored source '%s' ref %d: %s with null oldSrc.", src_name[i], i, src_name[i]);
			}
		}

		// now we see if our current entry here is the same or needs releasing / restoring
		bool fail = false;
		const char* name = NULL;
		if (!oldSrc) {
			// this entry no longer valid; set fail so we will release our stored one
			fail = true;
			// but point to the name
			name = src_name[i];
		} else {
			// we have a stored entry, use its name to RECHECK it; re-get the name in case it has changed??
			name = obs_source_get_name(oldSrc);
			//
			if (!name || !src_name[i])
				fail = true;
			else if (strcmp(name, src_name[i]) != 0)
				fail = true;
		}

		if (fail) {
			// we need to update/replace this stored source

			// set flag to make sure we rebuild stages
			needsRebuildStaging = true;

			//mydebug("Need to update src ref %d.", i);

			// release currently stored weak ref
			if (src_ref[i]) {
				// free our underlying pointer at this index
				//mydebug("Freeing existing non-null entry at ref %d.", i);
				obs_weak_source_release(src_ref[i]);
				strcpy(src_name[i], "");
				src_ref[i] = NULL;
			}

			// store new one?
			// get new weak link from OBS to the new source
			if (name) {
				//mydebug("Trying to update weak ref to source by name: %s.", name);
				obs_source_t *newSrc = obs_get_source_by_name(name);
				// store the new weak ref
				if (newSrc) {
					// store new name
					//mydebug("Found and storing weak ref to source by name: %s.", name);
					strcpy(src_name[i], name);
					// store new weak ref
					src_ref[i] = obs_source_get_weak_source(newSrc);
					// release new source we got
					obs_source_release(newSrc);
				}
			}
			else {
				// new one is GONE/non-present
			}
		}

		// no matter what we have to release the src we got above
		if (oldSrc) {
			obs_source_release(oldSrc);
		}
	}
}
//---------------------------------------------------------------------------
