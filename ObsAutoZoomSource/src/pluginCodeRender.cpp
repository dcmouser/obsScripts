//---------------------------------------------------------------------------
#include <cstdio>
//
#include "myPlugin.h"
//---------------------------------------------------------------------------







//---------------------------------------------------------------------------
void JrPlugin::doRender() {
	uint32_t sourceViewWidth, sourceViewHeight;
	uint32_t sourceTrackWidth, sourceTrackHeight;
	bool didUpdateTrackingBox = false;
	int huntingIndex = -1;

	//mydebug("In dorender stage 1.");


	// get the TrackedSources for viewing and tracking
	// if they are the same (ie we aren't currently hunting for a new source(camera)) then tracking source will be NULL, meaning use view source
	TrackedSource* tsourcepView;
	TrackedSource* tsourcepTrack;
	stracker.getDualTrackedViews(tsourcepView, tsourcepTrack);

	// now full sources from the weak references -- remember these need to be released before we leave this function
	obs_source_t* sourceView = tsourcepView ? tsourcepView->getFullSourceFromWeakSource() : NULL;
	obs_source_t* sourceTrack = tsourcepTrack ? tsourcepTrack->getFullSourceFromWeakSource() : NULL;

	// are we doing tracking? either at a certain rate or during oneshots or when hunting
	bool shouldUpdateTrackingBox = false;
	if (tsourcepTrack && DefAlwaysUpdateTrackingWhenHunting) {
		// when we are hunting we always update tracking
		trackingUpdateCounter = 0;
		shouldUpdateTrackingBox = true;
	} else if (opt_enableAutoUpdate || trackingOneShot || tsourcepTrack) {
		if (++trackingUpdateCounter >= opt_updateRate || trackingOneShot) {
			// frequence to update tracking box
			trackingUpdateCounter = 0;
			shouldUpdateTrackingBox = true;
		}
	}

	
	// default source to track
	obs_source_t* sourcePointerToTrack = tsourcepTrack ? sourceTrack : sourceView;
	TrackedSource* trackedSourcePointerToTrack = tsourcepTrack ? tsourcepTrack : tsourcepView;
	int trackedIndexToTrack =  tsourcepTrack ? stracker.sourceIndexTracking : stracker.sourceIndexViewing;

	// record the hunting index
	//mydebug("In dorender stage 1b.");
	if (tsourcepTrack) {
		// we are hunting, record the index that we are currently checking, BEFORE we advance
		huntingIndex = stracker.sourceIndexTracking;

		// we advance IF we are already in hunt mode, for next time
		// this is awkward
		if (shouldUpdateTrackingBox) {
			// advance hunt? if we tracked, then YES (this test is here in case we are rate-limiting even hunt updates)
			//mydebug("Calling advance...");
			stracker.AdvanceHuntToNextSource();
		}
	}


	bool needsRendering = false;
	//mydebug("after possible advance we have huntindex = %d and viewindex = %d and trackindex = %d and trackedIndexToTrack = %d.", huntingIndex, stracker.sourceIndexViewing, stracker.sourceIndexTracking, trackedIndexToTrack);


	// ok source view stuff
	if (sourceView != NULL) {
		// we have a source view

		//basic info about source
		sourceViewWidth = jrSourceGetWidth(sourceView);
		sourceViewHeight = jrSourceGetHeight(sourceView);
		//mydebug("in plugin_render 2b with source %s size: %dx%d outw = %d,%d [%d,%d].", tsourcepView->getName(), sourceViewWidth, sourceViewHeight, outWidth, outHeight, workingWidth, workingHeight);

		if (sourceViewWidth && sourceViewHeight) {
			// valid source

			// resize to height if it's changed
			recreateStagingMemoryIfNeeded(sourceView);

			// bypass mode says do NOTHING just render source to output (note this may resize it)
			// you can force this to always be true to bypass most processing for debug testing
			if (false || opt_filterBypass) {
				// just render out the source untouched, and quickly
				doRenderSourceOut(sourceView, sourceViewWidth, sourceViewHeight);
			} else {
				// needs real rendering, just drop down
				needsRendering = true;
			}
		}
	}


	if (!sourceTrack) {
		// we are tracking the view
		sourceTrackWidth = sourceViewWidth;
		sourceTrackHeight = sourceViewHeight;
	}
	else {
		// different source
		sourceTrackWidth = jrSourceGetWidth(sourceTrack);
		sourceTrackHeight = jrSourceGetHeight(sourceTrack);
		// ATTN: we might restage here?
	}


	if (sourcePointerToTrack) {
		//mydebug("in plugin_render 3b with TRACK %s size: %dx%d.", trackedSourcePointerToTrack->getName(), sourceTrackWidth, sourceTrackHeight);
	}

	//mydebug("In dorender stage 3.");

	// this first test should always be true, unless source is NULL
	if (sourcePointerToTrack && (sourceTrackWidth && sourceTrackHeight)) {
		// ok so there are two cases where we need to call findTrackingMarkerRegionInSource
		//  one is when we are updating the machine vision detection of the tracking box; another is when we want to SHOW the current tracking box even if its not being update
		if (shouldUpdateTrackingBox || opt_debugDisplay) {
			//mydebug("Calling findTrackingMarkerRegionInSource with source index #%d", trackedIndexToTrack);
			findTrackingMarkerRegionInSource(trackedSourcePointerToTrack, sourcePointerToTrack, sourceTrackWidth, sourceTrackHeight, shouldUpdateTrackingBox, huntingIndex);
			didUpdateTrackingBox = true;
		}
	}

	if (sourceView) {
		// update target zoom box from tracking box -- note this may happen EVEN when we are not doing a tracking box update, to provide smooth movement to target
		// ATTN: BUT.. do we want to do this on tracking source or viewing source?
		// ATTN: new -- calling it on VIEW
		tsourcepView->updateZoomCropBoxFromCurrentCandidate();
	}

	//mydebug("In dorender stage 4.");

	// ok we have (may have) updated tracking info for EITHER the viewing source or the tracking source; we did if we are debugging for sure
	// now final rendering (either with debug info, or with zoom crop effect, unless we bypasses above)
	if (needsRendering) {
		// if we are in debugDisplayMode then we just display the debug overlay as our plugin and do not crop
		if (opt_debugDisplay) {
			// overlay debug info on internal buffer; this uses TRACKED size
			// note that in debug mode we always show the currently TRACKING source (not the view source)
			overlayDebugInfoOnInternalDataBuffer(trackedSourcePointerToTrack);
			//  then render from internal planning data texture to display for debug purposes
			doRenderFromInternalMemoryToFilterOutput();
			//mydebug("in plugin_render 4b did DEBUG on %s size: %dx%d.", trackedSourcePointerToTrack->getName(), sourceTrackWidth, sourceTrackHeight);
		} else if (tsourcepView->lookingBoxReady && !(tsourcepView->lookingx1 == 0 && tsourcepView->lookingy1 == 0 && tsourcepView->lookingx2 == outWidth-1 && tsourcepView->lookingy2 == outHeight-1 && outWidth == workingWidth && outHeight == workingHeight)) {
			// we dont do a debug overlay render but instead crop in on the tracking box, of the VIEW source
			doRenderAutocropBox(tsourcepView, sourceView, sourceViewWidth, sourceViewHeight);
			//mydebug("in plugin_render 4c did render zoomcrop on %s size: %dx%d.", tsourcepView->getName(), sourceViewWidth, sourceViewHeight);
		}
		else {
			// nothing to do at all -- either we have not decided a place to look OR we are looking at the entire view of a non-adjusted view, so nothing needs to be done to the view
			doRenderSourceOut(sourceView, sourceViewWidth, sourceViewHeight);
		}
		needsRendering = false;
	}


	// turn off one shot tracking
	if (trackingOneShot && !stracker.isHunting()) {
		// turn off one-shot once we stop hunting
		trackingOneShot = false;
	}

	//mydebug("In dorender stage 5.");

	// release source (it's ok if we pass NULL)
	stracker.freeFullSource(sourceView); sourceView = NULL;
	stracker.freeFullSource(sourceTrack); sourceTrack = NULL;
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
// see source_render
void JrPlugin::doRenderSourceOut(obs_source_t* source, uint32_t rwidth, uint32_t rheight) {
	// attempts to make it size properly in properties
	if (rwidth == 0) {
		// recompute
		rwidth = jrSourceGetWidth(source);
		rheight = jrSourceGetHeight(source);
	}
	//mydebug("Rendering out source of size %d x %d.", rwidth, rheight);
	if (rwidth == 0 || rheight == 0) {
		//mydebug("in doRenderSourceOut aborting because of dimensions 0.");
		return;
	}
	// render it out
	myRenderSourceIntoTexture(source, NULL, rwidth, rheight, rwidth, rheight);
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
//---------------------------------------------------------------------------








//---------------------------------------------------------------------------
void JrPlugin::overlayDebugInfoOnInternalDataBuffer(TrackedSource* tsourcep) {
	// overlay debug 
	int x1, y1, x2, y2;
	int pixelVal;

	if (true) {
		// show all regions
		JrRegionSummary* region;
		//int pixelVal = 0x00FF00FF;
		pixelVal = 0xFF0000FF;
		JrRegionDetector* rdp = stracker.getRegionDetectorp();
		for (int i = 0; i < rdp->foundRegions; ++i) {
			region = rdp->getRegionpByIndex(i);
			//mydebug("Overlaying region %d (%d,%d,%d,%d).", i, region->x1, region->y1, region->x2, region->y2);
			//pixelVal = calcIsValidmarkerRegion(region) ? 0x88FF55FF : 0xFF0000FF;
			pixelVal = stracker.calcIsValidmarkerRegion(region) ?   0x88FF55FA : 0xFF0000FF;
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
	if (true && tsourcep->markerBoxReady && (tsourcep->markerx1 != tsourcep->lookingx1 || tsourcep->markery1 != tsourcep->lookingy1 || tsourcep->markerx2 != tsourcep->lookingx2 || tsourcep->markery2 != tsourcep->lookingy2)) {
		// modify contents of our internal data to show border around box
		//mydebug("Overlaying tracking box (%d,%d,%d,%d).", markerx1, markery1, markerx2, markery2);
		pixelVal = 0xFF22FFFF;
		x1 = tsourcep->markerx1;
		y1 = tsourcep->markery1;
		x2 = tsourcep->markerx2;
		y2 = tsourcep->markery2;
		RgbaDrawRectangle((uint32_t*)data, dlinesize, (float)x1/stageShrink, (float)y1/stageShrink, (float)x2/stageShrink, (float)y2/stageShrink, pixelVal);
	}
	if (true && tsourcep->lookingBoxReady) {
		// modify contents of our internal data to show border around box
		//mydebug("Overlaying crop box (%d,%d,%d,%d).", bx1, by1, bx2, by2);
		pixelVal = 0xFFFFFFFF;
		x1 = tsourcep->lookingx1;
		y1 = tsourcep->lookingy1;
		x2 = (float)tsourcep->lookingx2;
		y2 = (float)tsourcep->lookingy2;
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
//---------------------------------------------------------------------------














//---------------------------------------------------------------------------
void JrPlugin::findTrackingMarkerRegionInSource(TrackedSource* tsourcep, obs_source_t* source, uint32_t rwidth, uint32_t rheight, bool shouldUpdateTrackingBox, int huntingIndex) {

	// part 1
	// Render to intermediate target texrender instead of output of plugin (screen)
	doRenderWorkFromEffectToStageTexRender(effectChroma, source,  rwidth,  rheight);

	// part 2
	// ok now the output is in texrender texture where we can map it and copy it to private user memory
	doRenderWorkFromStageToInternalMemory();

	// part 3
	// update autotracking by doing machine vision on internal memory copy of effectChroma output
	if (shouldUpdateTrackingBox) {
		//mydebug("Doing part 3 stracker.analyzeSceneAndFindTrackingBox.");
		stracker.analyzeSceneAndFindTrackingBox(tsourcep, data, dlinesize, huntingIndex);
	}
}
//---------------------------------------------------------------------------









//---------------------------------------------------------------------------
void JrPlugin::doRenderAutocropBox(TrackedSource* tsourcep, obs_source_t* source, int rwidth, int rheight) {
	// ATTN: TODO - run crop and zoom effect


	// ATTN: we only need to really do this when box location changes
	if (true) {
		doCalculationsForZoomCrop(tsourcep);
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
//---------------------------------------------------------------------------







//---------------------------------------------------------------------------
bool JrPlugin::doCalculationsForZoomCrop(TrackedSource* tsourcep) {
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
	int bx1 = tsourcep->lookingx1;
	int by1 = tsourcep->lookingy1;
	int bx2 = tsourcep->lookingx2+1;
	int by2 = tsourcep->lookingy2+1;

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

