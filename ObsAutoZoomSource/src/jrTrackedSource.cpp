//---------------------------------------------------------------------------
#include "jrTrackedSource.h"
#include "myPlugin.h"
#include "obsHelpers.h"
#include "jrfuncs.h"
//---------------------------------------------------------------------------








//---------------------------------------------------------------------------
void TrackedSource::init(SourceTracker* st, int indexin) {
	sourceTrackerp = st;
	index = indexin;
	//
	stageHoldingSpaceTexrender = NULL;
	sourceHoldingSpaceTexrender = NULL;
	stagingTexrender = NULL;
	stagingSurface = NULL;
	stageDrawingTexture = NULL;
	stagedData = NULL;
	//
	stageShrink = -1;
	stageArea = 0;
	stagedDataLineSize = 0;

	// init region detector
	regionDetector.rdInit();

	// allocate texrender spaces - 9/7/22 leaking memory
	allocateGraphicsData();

	clear();
}

void TrackedSource::clear() {
	src_ref = NULL;
	strcpy(src_name, "");
	sourceWidth = 0;
	sourceHeight = 0;
	//
	oneTimeInstantSetNewTarget = false;
	markerlessStreakCounter = markerlessStreakCycleCounter = settledLookingStreakCounter = 0;
	targetStableCount = 0; changeTargetMomentumCounter = 0;
	//delayMoveUntilHuntFinishes = false;
	//
	needsRecheckForSize = true;
	//
	clearAllBoxReadies();
}


void TrackedSource::freeForDelete() {
	// free the weak source reference
	freeWeakSource(src_ref); 
	src_ref = NULL;

	// free memory use
	freeGraphicMemoryUse();

	// clear internal data
	clear();

	// tell region detector to free up
	regionDetector.rdFree();
}



void TrackedSource::freeGraphicMemoryUse() {
	obs_enter_graphics();
	freeBeforeReallocateFilterTextures();
	freeBeforeReallocateTexRender();
	obs_leave_graphics();

	freeBeforeReallocateNonGraphicData();
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
bool TrackedSource::allocateGraphicsData() {
	// ATTN: must this be done in graphics context?
	freeBeforeReallocateTexRender();

	// create texrender
	stageHoldingSpaceTexrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	sourceHoldingSpaceTexrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	stagingTexrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);

	// success
	return true;
}


void TrackedSource::freeBeforeReallocateTexRender() {
	if (stageHoldingSpaceTexrender) {
		gs_texrender_destroy(stageHoldingSpaceTexrender);
		stageHoldingSpaceTexrender = NULL;
	}
	if (sourceHoldingSpaceTexrender) {
		gs_texrender_destroy(sourceHoldingSpaceTexrender);
		sourceHoldingSpaceTexrender = NULL;
	}	
	if (stagingTexrender) {
		gs_texrender_destroy(stagingTexrender);
		stagingTexrender = NULL;
	}
}

void TrackedSource::freeBeforeReallocateFilterTextures() {
	if (stagingSurface) {
		gs_stagesurface_destroy(stagingSurface);
		stagingSurface = NULL;
	}
	if (stageDrawingTexture) {
		gs_texture_destroy(stageDrawingTexture);
		stageDrawingTexture = NULL;
	}
}

void TrackedSource::freeBeforeReallocateNonGraphicData() {
	// this may have to be done LAST?
	if (stagedData) {
		bfree(stagedData);
		stagedData = NULL;
	}
}
//---------------------------------------------------------------------------































































//---------------------------------------------------------------------------
void TrackedSource::clearAllBoxReadies() {
	markerBoxReady = false;
	lookingBoxReady = false;
	stickyBoxReady = false;
	lastTargetBoxReady = false;
	markerBoxIsOccluded = false;
	//
	lookingx1 = lookingy1 = lookingx2 = lookingy2 = 0;
	markerx1 = markery1 = markerx2 = markery2 = 0;
	stickygoalx1 = stickygoaly1 = stickygoalx2 = stickygoaly2 = 0;
	//
	changeTargetMomentumCounter = 0;
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
void TrackedSource::clearFoundTrackingBox() {
	markerx1 = markery1 = markerx2 = markery2 = -1;
	markerBoxReady = false;
	markerBoxIsOccluded = false;
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
bool TrackedSource::isOneValidRegionAtUnchangedLocation() {
	// IMPORTANT; you have to 
	// return true if we found one region at same location as prior tracking box
	JrRegionDetector* rd = &regionDetector;

	if (!stickyBoxReady) {
		return false;
	}
	if (rd->foundRegionsValid != 1) {
		return false;
	}

	JrRegionSummary* region;
	for (int i = 0; i < rd->foundRegions; ++i) {
		region =&rd->regions[i];
		if (region->valid) {
			// found the valid region
			// is one of the corners of the region possible at stickygoalx1 or stickygoalx2?
			if (isRegionSimilarBounds(region, &lastGoodMarkerRegion1)) {
				occludedCornerIndex = 2;
				return true;
			}
			if (isRegionSimilarBounds(region, &lastGoodMarkerRegion2)) {
				occludedCornerIndex = 1;
				return true;
			}
			break;
		}
	}
	return false;
}


bool TrackedSource::isRegionSimilarBounds(JrRegionSummary* regiona, JrRegionSummary* regionb) {
	JrPlugin* plugin = sourceTrackerp->getPluginp();
	int distanceThreshold = plugin->opt_zcBoxMoveDistance;

	float delta = abs(regiona->x1 - regionb->x1) + abs(regiona->x2 - regionb->x2) + abs(regiona->y1 - regionb->y1) + abs(regiona->y2 - regionb->y2);
	//
	//info("Checking for isOneValidRegionAtUnchangedLocation A comp (%d,%d) vs (%d,%d) and (%d,%d) vs (%d,%d) dist = %f.",regiona->x1,regiona->y1,regionb->x1,regionb->y1, regiona->x2,regiona->y2,regionb->x2,regionb->y2,delta);
	if (delta <= distanceThreshold) {
		// close enough
		return true;
	}
	return false;
}
//---------------------------------------------------------------------------







//---------------------------------------------------------------------------
bool TrackedSource::findNewCandidateTrackingBox() {
	// pointers
	JrRegionDetector* rd = &regionDetector;
	JrPlugin* plugin = sourceTrackerp->getPluginp();


	// for testing
	if (false) {
		markerx1 = 400;
		markery1 = 135;
		markerx2 = 1000;
		markery2 = 535;
		markerBoxReady = true;
		rd->foundRegions = 0;
		return true;
	}

	// helpers
	int outWidth = plugin->outputWidthPlugin;
	int outHeight = plugin->outputHeightPlugin;

	// reset
	clearFoundTrackingBox();

	// mark regions quality to being markers
	markRegionsQuality();

	JrRegionSummary* region;
	JrRegionSummary* region1;
	JrRegionSummary* region2;

	//mydebug("Number valid regions found: %d and total found: %d.", rd->foundRegionsValid, rd->foundRegions);

	// abort if not at least 2 regions
	if (rd->foundRegionsValid != 2) {
		// we need exactly 2 regions of validity to identify a new box
		// ATTN: TODO - we might actually be able to work with less or more IFF our current markers are CONSISTENT
		// move one region code to here

		if (DefForSingleValidRegionAssumePrevious && isOneValidRegionAtUnchangedLocation()) {
			// use last regions
			region1 = &lastGoodMarkerRegion1;
			region2 = &lastGoodMarkerRegion2;
			markerBoxIsOccluded = true;
			//mydebug("Treating one as occluded.");
			// now drop down
		} else {
			return false;
		}
	} else {
		// we found exactly 2 valid regions; get pointers to them
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
		region1=&rd->regions[validRegionIndices[0]];
		region2=&rd->regions[validRegionIndices[1]];
	}



	int posType = plugin->opt_zcMarkerPos;
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
	float minDist = ((float)sourceWidth / plugin->computedMinDistFractionalBetweenMarkersToReject) / stageShrink;
	if (max(ity2 - ity1, itx2 - itx1) < minDist) {
		//mydebug("Regions too close returning false.");
		return false;
	}


	// rescale from stage to original resolution
	markerx1 = tx1 * stageShrink + plugin->opt_zcBoxMargin * posMult;
	markery1 = ty1 * stageShrink + plugin->opt_zcBoxMargin * posMult;
	markerx2 = tx2 * stageShrink - plugin->opt_zcBoxMargin * posMult;
	markery2 = ty2 * stageShrink - plugin->opt_zcBoxMargin * posMult;


	// new check if they are inline then we treat them as indicator of bottom or left
	bool horzInline = false;
	bool vertInline = false;
	//
	// new test
	// we check if midpoints are as close as avg region size, with a bonus slack allowed more distance if the aspect ratio is very skewed

	// vertically inline markers?
	float midpointDist = fabs(((float)(region1->x1 + region1->x2) / 2.0f) - ((float)(region2->x1 + region2->x2) / 2.0f));
	float avgRegionSize = fabs(((float)(region1->x2 - region1->x1) + (region2->x2 - region2->x1)) / 2.0f);
	//float aspectBonus = abs(min((float)(markery2 - markery1),1.0f) / min((float)(markerx2 - markerx1),1.0f));
	float distx = (float)max(abs(markerx2 - markerx1), 1.0f);
	float disty = (float)max(abs(markery2 - markery1), 1.0f);
	float aspectBonus = disty / distx;
	float distThresh = avgRegionSize * (1.0f + 5.0f * min(aspectBonus, 20.0) / 20.0f);
	//info("minpointdist = %f  Avg RegionSize = %f  aspectbonus = %f distthresh = %f (distx = %f, disty=%f)", midpointDist, avgRegionSize, aspectBonus, distThresh,distx,disty);
	if (midpointDist <= distThresh) {
		vertInline = true;
	}

	// horizontally inline markers?
	midpointDist = fabs(((float)(region1->y1 + region1->y2) / 2.0f) - ((float)(region2->y1 + region2->y2) / 2.0f));
	avgRegionSize = (float)((region1->y2 - region1->y1) + (region2->y2 - region2->y1)) / 2.0f;
	distx = (float)max(abs(markerx2 - markerx1), 1.0f);
	disty = (float)max(abs(markery2 - markery1), 1.0f);
	aspectBonus = distx / disty;
	distThresh = avgRegionSize * (1.0f + 5.0f * min(aspectBonus, 20.0) / 20.0f);
	if (midpointDist <= distThresh) {
		horzInline = true;
	}


	// inline aspect ratios
	if (horzInline) {
		// so we leave the x values alone and move y2 up to match aspect
		float aspectRatio = (float)outHeight / (float)outWidth;
		float sz = ((float)((markerx2 - markerx1) + 1.0f) * aspectRatio) + 1.0f;
		markery2 = (float)markery1 + sz;
		if (markery2 > sourceHeight - 1) {
			int dif = markery2 - (sourceHeight - 1);
			markery2 = sourceHeight - 1;
			markery1 = markery2 - sz;
		}
	}
	else if (vertInline) {
		// so we leave the y values alone and move x2 up to match aspect
		float aspectRatio = (float)outWidth / (float)outHeight;
		float sz = ((float)((markery2 - markery1) + 1.0f) * aspectRatio) - 1.0f;
		markerx2 = (float)markerx1 + sz;
		if (markerx2 > sourceWidth-1) {
			int dif = markerx2 - (sourceWidth-1);
			markerx2 = sourceWidth-1;
			markerx1 = markerx2 - sz;
		}
	}

	// clean up bounds
	if (markerx1 < 0) {
		markerx1 = 0;
	}
	if (markery1 < 0) {
		markery1 = 0;
	}
	if (markerx1 >= sourceWidth) {
		markerx1 = sourceWidth-1;
	}
	if (markery1 >= sourceHeight) {
		markery1 = sourceHeight-1;
	}
	if (markerx2 < 0) {
		markerx2 = 0;
	}
	if (markery2 < 0) {
		markery2 = 0;
	}
	if (markerx2 >= sourceWidth) {
		markerx2 = sourceWidth-1;
	}
	if (markery2 >= sourceHeight) {
		markery2 = sourceHeight-1;
	}

	// remember last two good regions (unless we are already using them due to finding a single marker)
	if (&lastGoodMarkerRegion1 != region1) {
		memcpy(&lastGoodMarkerRegion1, region1, sizeof(JrRegionSummary));
	}
	if (&lastGoodMarkerRegion2 != region1) {
		memcpy(&lastGoodMarkerRegion2, region2, sizeof(JrRegionSummary));
	}

	//mydebug("Returning true from region marker find.");

	// tracking box now has valid data
	markerBoxReady = true;
	return markerBoxReady;
}
//---------------------------------------------------------------------------


























//---------------------------------------------------------------------------
void TrackedSource::markRegionsQuality() {
	JrRegionDetector* rd = &regionDetector;
	JrRegionSummary* region;

	int foundValidCount = 0;
	for (int i = 0; i < rd->foundRegions; ++i) {
		region = &rd->regions[i];
		region->valid = calcIsValidmarkerRegion(region);
		if (region->valid) {
			++foundValidCount;
		}
	}
	rd->foundRegionsValid = foundValidCount;
}


bool TrackedSource::calcIsValidmarkerRegion(JrRegionSummary* region) {
	// valid region based on plugin options
	JrPlugin* plugin = sourceTrackerp->getPluginp();

	if (region->pixelCount == 0) {
		return false;
	}

	if (region->density < plugin->opt_rmTDensityMin) {
		return false;
	}

	if (region->aspect < plugin->opt_rmTAspectMin) {
		return false;
	}

	double relativeArea1000 = (sqrt(region->area) / sqrt(stageArea)) * 1000.0;
	//mydebug("Commparing region %d area of (%d in %d) relative (%f) against min of %f.", region->label, region->area, stageArea, relativeArea1000, opt_rmTSizeMin);
	if (relativeArea1000 < plugin->opt_rmTSizeMin || (plugin->opt_rmTSizeMax>0 && relativeArea1000 > plugin->opt_rmTSizeMax) ) {
		return false;
	}

	// valid
	return true;
}




void TrackedSource::analyzeSceneAndFindTrackingBox( uint8_t *data, uint32_t dlinesize, bool isHunting) {

	// step 1 - build labeling matrix
	// this does the possibly heavy duty computation of Connected Components Contour Tracing, etc.
	analyzeSceneFindComponentMarkers(data,dlinesize);

	// step 2 identify the new candidate tracking box in the view
	// this may find nothing reliable -- that's ok
	//mydebug("in analyzeSceneAndFindTrackingBox in index %d.", index);
	findNewCandidateTrackingBox();
	if (areMarkersBothVisibleNeitherOccluded()) {
		//mydebug("found good marker Region in index %d.",index);
		if (isHunting) {
			// we were hunting and we found a good new source to switch to
			//mydebug("!!!!!!!!!!!!!!!! in analyzeSceneAndFindTrackingBox hit one index %d.", index);
			sourceTrackerp->foundGoodHuntedSourceIndex(index);
		}
		else {
			//mydebug("in analyzeScene but isHunting false so doing nothing.");
		}
	} else {
		//mydebug("Did not find markers in source with index = %d and huntingIndex = %d, validRegions = %d of %d.", index, huntingIndex, regionDetector.foundRegionsValid, regionDetector.foundRegions);
		//mydebug("Did not find markers in source with index = %d, validRegions = %d of %d.", index, regionDetector.foundRegionsValid, regionDetector.foundRegions);
	}
}




void TrackedSource::analyzeSceneFindComponentMarkers(uint8_t *data, uint32_t dlinesize) {
	JrRegionDetector* rd = &regionDetector;

	// step 1 build initial labels
	int foregroundPixelCount = rd->fillFromStagingMemory(data, dlinesize);

	// step 2 - connected component labelsing
	int regionCountFound = rd->doConnectedComponentLabeling();

	// debug
	//mydebug("JrObs CCL regioncount = %d (%d foregroundpixels) regionsize = [%d,%d].", regionCountFound, foregroundPixelCount, rd->width, rd->height);
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
void TrackedSource::setStageSize(int instageWidth, int instageHeight) {
	// tell region detctor about resize
	stageWidth = instageWidth;
	stageHeight = instageHeight;
	stageArea = stageWidth * stageHeight;
	regionDetector.rdResize(instageWidth, instageHeight);
}
//---------------------------------------------------------------------------










//---------------------------------------------------------------------------
void TrackedSource::freeWeakSource(obs_weak_source_t* wsp) {
	if (wsp != NULL) {
		obs_weak_source_release(wsp);
	}
}

void TrackedSource::freeFullSource(obs_source_t* src) {
	if (src != NULL) {
		obs_source_release(src);
	}
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
void TrackedSource::onOptionsChange() {
	needsRecheckForSize = true;
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
void TrackedSource::travelToMarkerless() {
	// this is called when we hunt for markers in a different view and FAIL to find them.. so it is what now says finally that we should travel to markerless coordinates (and optionally change sources)
	// note the caller is responsible for setting sourcetracker viewindex to us
	//mydebug("In travelToMarkerless for source index #%d.", index);
	JrPlugin* plugin = sourceTrackerp->getPluginp();

	// set markerless coords
	plugin->fillCoordsWithMarkerlessCoords(&markerx1, &markery1, &markerx2, &markery2);
	plugin->fillCoordsWithMarkerlessCoords(&stickygoalx1, &stickygoaly1, &stickygoalx2, &stickygoaly2);
	//mydebug("Set coords %d,%d - %d,%d.", markerx1, markery1, markerx2, markery2);


	// now depending on whether we are already viewing this source or not we may need to do more
	if (sourceTrackerp->getViewSourceIndex() == index) {
		// we are already the view source, so we just need to set target locations, not switch over to this source; we will graduall move to the lasttarget locations set above
		setLocationsToMarkerLocations(false);
		//mydebug("only setting base locations not looking.");
	} else {
		// initiate a FADE to the new source
		plugin->initiateFade(sourceTrackerp->getViewSourceIndex(), index);
		// so we need to switch viewing source to us;
		sourceTrackerp->setViewSourceIndex(index,true);
		// force looking coordinates here in case we have a view render before the next tracking step
		setLocationsToMarkerLocations(true);
		//mydebug("setting looking and instants.");
	}
}



void TrackedSource::setLocationsToMarkerLocations(bool flagInstantLookingPos) {
	lasttargetx1 = markerx1;
	lasttargety1 = markery1;
	lasttargetx2 = markerx2;
	lasttargety2 = markery2;
	// new 9/7/22
	if (true) {
		// is this responsible for a big weird jump when zooming in switching to closeup view?
		stickygoalx1 = markerx1;
		stickygoaly1 = markery1;
		stickygoalx2 = markerx2;
		stickygoaly2 = markery2;
	}

	if (flagInstantLookingPos) {
		lookingx1 = markerx1;
		lookingy1 = markery1;
		lookingx2 = markerx2;
		lookingy2 = markery2;
		// this should not be needed
		oneTimeInstantSetNewTarget = true;
	}
}
//---------------------------------------------------------------------------










//---------------------------------------------------------------------------
void TrackedSource::updateLastHuntCoordinates() {
	lasthuntx1 = markerx1;
	lasthunty1 = markery1;
	lasthuntx2 = markerx2;
	lasthunty2 = markery2;
}
//---------------------------------------------------------------------------












//---------------------------------------------------------------------------
void TrackedSource::touchRefreshDuringRenderCycle() {
	JrPlugin* plugin = sourceTrackerp->getPluginp();

	// lease it
	obs_source_t* osp = borrowFullSourceFromWeakSource();

	// want to do something else?
	obs_source_inc_showing(osp);

	// both of these seem necesary in order to ensure that we have fresh data reloaded for the source when we need it
	// why would both be needed??
	doRenderWorkFromEffectToStageTexRender(plugin->effectChroma, osp);
	doRenderWorkFromStageToInternalMemory();

	obs_source_dec_showing(osp);

	// release it
	releaseBorrowedFullSource(osp);
}
//---------------------------------------------------------------------------
