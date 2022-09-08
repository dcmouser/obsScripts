//---------------------------------------------------------------------------
#include "jrSourceTracker.h"
#include "myPlugin.h"
#include "obsHelpers.h"
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
void TrackedSource::doRenderWorkFromEffectToStageTexRender(gs_effect_t* effectChroma, obs_source_t* source) {
	JrPlugin* plugin = sourceTrackerp->getPluginp();

	if (stageHoldingSpaceTexrender == NULL) {
		mydebug("!!!!!!!!!!!!!!!!!!!!!! WARNING, stageHoldingSpaceTexrender is null!");
	}
	if (stagingTexrender == NULL) {
		mydebug("!!!!!!!!!!!!!!!!!!!!!! WARNING, stagingTexrender is null!");
	}

	// STAGE 1 render SOURCE
	// render into holdingSpaceTexrender (temporary) texture - to a stage x stage dimension
	jrRenderSourceIntoTexture(source, stageHoldingSpaceTexrender, stageWidth, stageHeight, sourceWidth, sourceHeight);

	// STAGE 2 - render effect from holdingSpaceTexrender to stagingTexRender
	// first set effect params
	plugin->autoZoomSetEffectParamsChroma(sourceWidth, sourceHeight);
	// then render effect from holding space into stage
	jrRenderEffectIntoTexture(stagingTexrender, effectChroma, stageHoldingSpaceTexrender, stageWidth, stageHeight);
}
//---------------------------------------------------------------------------









//---------------------------------------------------------------------------
void TrackedSource::doRenderWorkFromStageToInternalMemory() {
	JrPlugin* plugin = sourceTrackerp->getPluginp();
	bool stageMemoryReady = false;

	// clear it to help debugger not see old version
	if (DefClearDrawingSpacedForDebugging) {
		memset(stagedData, 0x07, (stageWidth + 32) * stageHeight * 4);
	}


	if (stagedData) {
		// default OBS plugin since we don't want to do anything
		gs_effect_t *effectDefault = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		gs_texture_t *tex = gs_texrender_get_texture(stagingTexrender);
		if (tex) {
			gs_stage_texture(stagingSurface, tex);
			//
			uint8_t *idata;
			uint32_t ilinesize;
			WaitForSingleObject(plugin->mutex, INFINITE);
			if (gs_stagesurface_map(stagingSurface, &idata, &ilinesize)) {
				//mydebug("ATTN: JR gs_stage_texture 2.\n");
				memcpy(stagedData, idata, ilinesize * stageHeight);
				// update our stored data linesize
				stagedDataLineSize = ilinesize;
				gs_stagesurface_unmap(stagingSurface);
				stageMemoryReady = true;
			} else {
				mydebug("ERROR ------> doRenderWorkFromStageToInternalMemory failed in call to gs_stagesurface_map.");
			}
			ReleaseMutex(plugin->mutex);
		}
		else {
			mydebug("ERROR ---> doRenderWorkFromStageToInternalMemory tex is NULL.");
		}
	} else {
		mydebug("ERROR ---> doRenderWorkFromStageToInternalMemory stagedata is NULL.");
	}

	if (!stageMemoryReady) {
		mydebug("ERROR ---> stageMemoryReady is false.");
		// clear it to help debugger not see old version
		if (DefClearDrawingSpacedForDebugging) {
			memset(stagedData, 0xFF, (stageWidth + 32) * stageHeight * 4);
		}
	}
}
//---------------------------------------------------------------------------








//---------------------------------------------------------------------------
void TrackedSource::overlayDebugInfoOnInternalDataBuffer() {
	// overlay debug 
	int x1, y1, x2, y2;
	int pixelVal;

	//mydebug("overlayDebugInfoOnInternalDataBuffer for index %d.", index);

	if (true) {
		// show all regions
		JrRegionSummary* region;
		//int pixelVal = 0x00FF00FF;
		pixelVal = 0xFF0000FF;
		JrRegionDetector* rdp = &regionDetector;
		for (int i = 0; i < rdp->foundRegions; ++i) {
			region = rdp->getRegionpByIndex(i);
			//mydebug("Overlaying region %d (%d,%d,%d,%d).", i, region->x1, region->y1, region->x2, region->y2);
			//pixelVal = calcIsValidmarkerRegion(region) ? 0x88FF55FF : 0xFF0000FF;
			pixelVal = calcIsValidmarkerRegion(region) ? 0x88FF55FA : 0xFF0000FF;
			x1 = region->x1 - 1;
			y1 = region->y1 - 1;
			x2 = region->x2 + 1;
			y2 = region->y2 + 1;
			//
			if (x1 < 0) { x1 = 0; }
			if (y1 < 0) { y1 = 0; }
			if (x2 >= stageWidth) { x2 = stageWidth - 1; }
			if (y2 >= stageHeight) { y2 = stageHeight - 1; }
			//
			jrRgbaDrawRectangle((uint32_t*)stagedData, stagedDataLineSize, x1, y1, x2, y2, pixelVal);
		}
	}

	// show found cropable area
	if (true && areMarkersBothVisibleOrOneOccluded() && (true || markerx1 != lookingx1 || markery1 != lookingy1 || markerx2 != lookingx2 || markery2 != lookingy2 || !areMarkersBothVisibleNeitherOccluded())) {
		// modify contents of our internal data to show border around box
		//mydebug("Overlaying tracking box (%d,%d,%d,%d).", markerx1, markery1, markerx2, markery2);
		// this is the yellow box
		pixelVal = 0xFF22FFFF;
		if (index == sourceTrackerp->getViewSourceIndex()) {
			pixelVal = 0xFF22FFFF;
		} else {
			pixelVal = 0xFFFF00FF;
		}
		x1 = markerx1-stageShrink;
		y1 = markery1-stageShrink;
		x2 = markerx2+stageShrink;
		y2 = markery2+stageShrink;
		if (x1 < 0) { x1 = 0; }
		if (y1 < 0) { y1 = 0; }
		if (x2 >= sourceWidth) { x2 = sourceWidth - 1; }
		if (y2 >= sourceHeight) { y2 = sourceHeight - 1; }	
		jrRgbaDrawRectangle((uint32_t*)stagedData, stagedDataLineSize, (float)x1/stageShrink, (float)y1/stageShrink, (float)x2/stageShrink, (float)y2/stageShrink, pixelVal);

		if (!areMarkersBothVisibleNeitherOccluded()) {
			// draw somethign to let us know one marker is excluded
			int ix1 = x1;
			int iy1 = y1;
			int ix2 = x2;
			int iy2 = y2;
			int pdim = stageShrink * 4;
			if (false && occludedCornerIndex == 1) {
				ix1 = x1 - pdim;
				ix2 = x1 + pdim;
				iy1 = y1 - pdim;
				iy2 = y1 + pdim;
			} else if (false && occludedCornerIndex == 2) {
				ix1 = x2 - pdim;
				ix2 = x2 + pdim;
				iy1 = y2 - pdim;
				iy2 = y2 + pdim;
			} else {
				// shouldnt happen
				ix1 = (x1+x2)/2 - pdim;
				ix2 = (x1+x2)/2 + pdim;
				iy1 = (y1+y2)/2 - pdim;
				iy2 = (y1+y2)/2 + pdim;
			}

			if (ix1 < 0) { ix1 = 0; }
			if (iy1 < 0) { iy1 = 0; }
			if (ix2 >= sourceWidth) { ix2 = sourceWidth - 1; }
			if (iy2 >= sourceHeight) { iy2 = sourceHeight - 1; }			
			
			jrRgbaDrawRectangle((uint32_t*)stagedData, stagedDataLineSize, (float)ix1/stageShrink, (float)iy1/stageShrink, (float)ix2/stageShrink, (float)iy2/stageShrink, pixelVal);
		}
	}
	if (true && lookingBoxReady) {
		// modify contents of our internal data to show border around box
		//mydebug("Overlaying crop box (%d,%d,%d,%d).", bx1, by1, bx2, by2);
		pixelVal = 0xFFFFFFFF;
		x1 = lookingx1;
		y1 = lookingy1;
		x2 = (float)lookingx2;
		y2 = (float)lookingy2;
		//
		if (x1 < 0) { x1 = 0; }
		if (y1 < 0) { y1 = 0; }
		if (x2 > sourceWidth-1) {
			x2 = sourceWidth - 1;
		}
		if (y2 > sourceHeight-1) {
			y2 = sourceHeight - 1;
		}
		jrRgbaDrawRectangle((uint32_t*)stagedData, stagedDataLineSize, (float)x1/stageShrink, (float)y1/stageShrink, (float)x2/stageShrink, (float)y2/stageShrink, pixelVal);
	}
}
	
	
	
void TrackedSource::doRenderFromInternalMemoryToFilterOutput(int outx1, int outy1, int outx2, int outy2) {
	bool flagCreateNewTex = false;
	bool flagUseDrawingTex = true;

	// try to render to screen the previously generated texture (possibly scaling up)

	gs_matrix_push();

	// default OBS plugin since we don't want to do anything
	gs_effect_t *effectDefault = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_texture_t *tex = gs_texrender_get_texture(stagingTexrender);
	gs_texture_t* texOut = NULL;
	gs_texture_t* texRender = tex;

	// copy our data modified pixels into a texture to render
	//mydebug("doRenderFromInternalMemoryToFilterOutput index %d (%d,%d) to target %d,%d-%d,%d.", index, stageWidth, stageHeight,outx1,outy1,outx2,outy2);

	// test write out to drawing texture for plugin output
	// see https://gitlab.hs-bremerhaven.de/jpeters/obs-studio/-/blob/master/test/test-input/sync-pair-vid.c
	// see https://discourse.urho3d.io/t/urho3d-as-plugin-for-obs-studio/6849/7
	uint8_t *ptr;
	uint32_t ilinesize;
	if (flagCreateNewTex) {
		// this will be destroyed at end
		texRender = gs_texture_create(stageWidth, stageHeight, GS_RGBA, 1, NULL, GS_DYNAMIC);
	} else if (flagUseDrawingTex) {
		texRender = stageDrawingTexture;			
	}

	// copy from our internal buffer to the output texture in RGBA format
	if (gs_texture_map(texRender, &ptr, &ilinesize)) {
		memcpy((uint32_t*)ptr, stagedData, ilinesize * stageHeight);
		gs_texture_unmap(texRender);
	}

	// ATTN: test
	int outWidth = outx2 - outx1;
	int outHeight = outy2 - outy1;

	//offset in so we can debug multiple sources on same screen
	gs_matrix_translate3f(outx1, outy1, 0.0f);

	// render from texRender to plugin output
	gs_eparam_t *image = gs_effect_get_param_by_name(effectDefault, "image");
	gs_effect_set_texture(image, texRender);
	// just a simple draw of texture
	// note we are willing to scale it up to full size from smaller stage
	while (gs_effect_loop(effectDefault, "Draw")) {
		gs_draw_sprite(texRender, 0, outWidth, outHeight);
	}

	// free temporary tex?
	if (flagCreateNewTex) {
		gs_texture_destroy(texRender);
	}

	gs_matrix_pop();
}
//---------------------------------------------------------------------------


















//---------------------------------------------------------------------------
void TrackedSource::doRenderAutocropBoxToScreen( obs_source_t* source) {
	// ATTN: TODO - run crop and zoom effect
	JrPlugin* plugin = sourceTrackerp->getPluginp();

	// now render

	// STAGE 1 render SOURCE
	// render into texrender2 (temporary) texture - to a stage x stage dimension
	jrRenderSourceIntoTexture(source, sourceHoldingSpaceTexrender, sourceWidth, sourceHeight, sourceWidth, sourceHeight);


	// STAGE 2 - render effect to output
	if (true) {
		// ATTN: we only need to really do this when box location changes
		//mydebug("Rendering autocrop box for index %d using looking locations: %d,%d - %d,%d", index, lookingx1,lookingy1,lookingx2,lookingy2);
		plugin->doCalculationsForZoomCropEffect(this);
	}
	plugin->autoZoomSetEffectParamsZoomCrop(sourceWidth, sourceHeight);
	// then render effect into target texture texrender
	jrRenderEffectIntoTexture(NULL, plugin->effectZoomCrop, sourceHoldingSpaceTexrender, sourceWidth, sourceHeight);
}
//---------------------------------------------------------------------------






//---------------------------------------------------------------------------
void TrackedSource::doRenderAutocropBoxFadedToScreen( obs_source_t* source, TrackedSource* fromtsp, float fadePosition) {
	// ATTN: TODO - run crop and zoom effect
	JrPlugin* plugin = sourceTrackerp->getPluginp();
	gs_texrender_t* fadeTexRenderA = NULL;
	gs_texrender_t* fadeTexRenderB = NULL;

	float fadeVal = plugin->fadePosition;

	if (false && fadeVal > 0.99f) {
		doRenderAutocropBoxToScreen(source);
		return;
	}

	if (true) {

		// render source A into texture A
		fadeTexRenderA = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
		// note that we use the ALREADY rendered into fromtsp->sourceHoldingSpaceTexrender which is like a screenshot and should be valid if we are fading from it
		//mydebug("Fade render texa");
		// so fromtsp->sourceHoldingSpaceTexrender is considered valid
		// set zoom crop effects then render zoomcrop for this source
		plugin->doCalculationsForZoomCropEffect(fromtsp);
		plugin->autoZoomSetEffectParamsZoomCrop(fromtsp->sourceWidth, fromtsp->sourceHeight);
		jrRenderEffectIntoTexture(fadeTexRenderA, plugin->effectZoomCrop, fromtsp->sourceHoldingSpaceTexrender, fromtsp->sourceWidth, fromtsp->sourceHeight);
		// ok so now we should have a texture with the zoomed in image from the fromtsp (you could even save cpu by screenshotting THIS and not updating each each moment of fade
		// and now we would like to plend this in with output

		// now render source B (us) into texture B
		fadeTexRenderB = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
		// render latest version of source into our sourceHoldingSpaceTexrender
		//mydebug("Fade render texb");
		jrRenderSourceIntoTexture(source, sourceHoldingSpaceTexrender, sourceWidth, sourceHeight, sourceWidth, sourceHeight);
		// set zoom crop effects then render zoomcrop for this source
		plugin->doCalculationsForZoomCropEffect(this);
		plugin->autoZoomSetEffectParamsZoomCrop(sourceWidth, sourceHeight);
		// then render effect into target texture texrender
		jrRenderEffectIntoTexture(fadeTexRenderB, plugin->effectZoomCrop, sourceHoldingSpaceTexrender, sourceWidth, sourceHeight);
		// ok so now we should have a texture with the zoomed in image from the fromtsp (you could even save cpu by screenshotting THIS and not updating each each moment of fade
		// and now we would like to blend these two textures into a final output shown on screeen
		jrRenderEffectIntoTextureFade(NULL, plugin->effectFade, fadeTexRenderB, fadeTexRenderA, fadeVal, sourceWidth, sourceHeight);
		//mydebug("Tried to fade with mix %f.", fadeVal);
	}

	// all done? release fadeTexRenders
	if (fadeTexRenderA) {
		gs_texrender_destroy(fadeTexRenderA);
		fadeTexRenderA = NULL;
	}
	if (fadeTexRenderB) {
		gs_texrender_destroy(fadeTexRenderB);
		fadeTexRenderB = NULL;
	}
}
//---------------------------------------------------------------------------



