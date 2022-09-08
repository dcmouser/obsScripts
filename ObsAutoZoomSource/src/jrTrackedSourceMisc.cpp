//---------------------------------------------------------------------------
#include "jrSourceTracker.h"
#include "myPlugin.h"
#include "obsHelpers.h"
#include "jrfuncs.h"
//---------------------------------------------------------------------------

































//---------------------------------------------------------------------------
// reaallocate staging memory stuff if needed
void TrackedSource::recheckSizeAndAdjustIfNeeded(obs_source_t* source) {
	JrPlugin* plugin = sourceTrackerp->getPluginp();

	//mydebug("In TrackedSource::recheckSizeAndAdjustIfNeeded with index: %d.", index);

	// reset flag since we are doing so now
	needsRecheckForSize = false;

	// update if size changes
	bool update = false;

	// borrow full source so we can check it's size?
	//obs_source_t* source = borrowFullSourceFromWeakSource();
	//
	if (!source) {
		// clear EVERYTHING if we can't find source (this will not yet free any memory...)
		clear();
		//
		// is this part really needed? or will we do this when we need to resize..
		if (true) {
			obs_enter_graphics();
			freeBeforeReallocateFilterTextures();
			obs_leave_graphics();
			freeBeforeReallocateNonGraphicData();
		}
		return;
	}

	// now we can get size
	int newSourceWidth = jrSourceCalculateWidth(source);
	int newSourceHeight = jrSourceCalculateHeight(source);
	int newStageShrink = max(plugin->opt_rmStageShrink,0.01f);
	int newStageWidth = newSourceWidth / newStageShrink;
	int newStageHeight = newSourceHeight / newStageShrink;
	// release full source
	//	releaseBorrowedFullSource(source);


	//mydebug("recheckSizeAndAdjustIfNeeded 2 with new dimensions %dx%d vs old of %dx%d.", newSourceWidth, newSourceHeight, sourceWidth, sourceHeight);

	if (newSourceWidth != sourceWidth || newSourceHeight != sourceHeight || newStageWidth != stageWidth || newStageHeight != stageHeight) {
		update = true;

		//mydebug("Ok size has changed, we need to reallocate.");

		// mutex surround -- not sure if needed
		WaitForSingleObject(plugin->mutex, INFINITE);

		// update width and height based on source
		sourceWidth = newSourceWidth;
		sourceHeight = newSourceHeight;
		stageWidth = newStageWidth;
		stageHeight = newStageHeight;
		stageShrink = newStageShrink;
		stageArea = stageWidth * stageHeight;

		// always true
		bool sizeChanged = true;

		// mark current boxes as invalid
		if (sizeChanged) {
			clearAllBoxReadies();
			// ok so if the markerless options say to go to this source to a specific location, we may need to recalculate that info
			// we normally wouldnt have to rescan unless 
			if (plugin->markerlessSourceIndex == this->index) {
				plugin->updateMarkerlessCoordsCycle();
			}
		}

		// now we need to reallocate graphics
		reallocateGraphiocsForTrackedSource();

		// source tracker and region detect helper
		setStageSize(stageWidth, stageHeight);

		// done in protected mutex area
		ReleaseMutex(plugin->mutex);
	}
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
void TrackedSource::reallocateGraphiocsForTrackedSource() {
		obs_enter_graphics();
		freeBeforeReallocateFilterTextures();
		// allocate staging area based on DESTINATION size?
		stagingSurface = gs_stagesurface_create(stageWidth, stageHeight, GS_RGBA);
		// new drawing texture
		stageDrawingTexture = gs_texture_create(stageWidth, stageHeight, GS_RGBA, 1, NULL, GS_DYNAMIC);
		obs_leave_graphics();

		info("JR Created drawing and staging texture %d by %d (%d x %d).", sourceWidth, sourceHeight, stageWidth, stageHeight);

		// ATTN: IMPORTANT - this bug this is making current code blank
		freeBeforeReallocateNonGraphicData();

		// realloc internal memory
		stagedData = (uint8_t *) bzalloc((stageWidth + 32) * stageHeight * 4);
}
//---------------------------------------------------------------------------










//---------------------------------------------------------------------------
void TrackedSource::findTrackingMarkerRegionInSource(obs_source_t* source,  bool shouldUpdateTrackingBox, bool isHunting) {
	JrPlugin* plugin = sourceTrackerp->getPluginp();

	// experiment
	obs_source_inc_showing(source);

	// part 1
	//mydebug("findTrackingMarkerRegionInSource - part 1 [%d ishunting = %d]  [source showing = %d]",index,(int)isHunting,(int)obs_source_showing(source));
	// Render to intermediate target texrender instead of output of plugin (screen)
	doRenderWorkFromEffectToStageTexRender(plugin->effectChroma, source);

	// part 2
	// ok now the output is in texrender texture where we can map it and copy it to private user memory
	//mydebug("findTrackingMarkerRegionInSource - part 2");	
	doRenderWorkFromStageToInternalMemory();

	// part 3
	// update autotracking by doing machine vision on internal memory copy of effectChroma output
	if (shouldUpdateTrackingBox) {
		//mydebug("Doing part 3 stracker.analyzeSceneAndFindTrackingBox.");
		//mydebug("findTrackingMarkerRegionInSource - part 3");
		analyzeSceneAndFindTrackingBox(stagedData, stagedDataLineSize, isHunting);
	}

	// experiment
	obs_source_dec_showing(source);

	if (index == 1 && isHunting) {
		//mydebugbeep();
	}
	
	//mydebug("In findTrackingMarkerRegionInSource with index %d and isHunting = %d and shouldupdateTracking = *** %d ***  regions are (%d,%d).", index, (bool)isHunting, (bool)shouldUpdateTrackingBox, regionDetector.foundRegionsValid, regionDetector.foundRegions);
}
//---------------------------------------------------------------------------


