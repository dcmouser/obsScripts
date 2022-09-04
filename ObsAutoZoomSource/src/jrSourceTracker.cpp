//---------------------------------------------------------------------------
#include "jrSourceTracker.h"
#include "myPlugin.h"
#include "obsHelpers.h"
//---------------------------------------------------------------------------














//---------------------------------------------------------------------------
void TrackedSource::clearAllBoxReadies() {
	markerBoxReady = false;
	lookingBoxReady = false;
	stickyBoxReady = false;
	lastTargetBoxReady = false;
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
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
bool TrackedSource::isOneValidRegionAtUnchangedLocation() {
	// IMPORTANT; you have to 
	// return true if we found one region at same location as prior tracking box

	JrRegionDetector* rd = sourceTrackerp->getRegionDetectorp();

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
void TrackedSource::updateZoomCropBoxFromCurrentCandidate() {
	// ok we have a CANDIDATE box markerx1,markery1,markerx2,markery2 and/or a planned next place to go
	// now we want to change our actual zoomCrop box towards this

	//mydebug("updateZoomCropBoxFromCurrentCandidate stage .");

	// pointers
	JrRegionDetector* rd = sourceTrackerp->getRegionDetectorp();
	JrPlugin* plugin = sourceTrackerp->getPluginp();

	// shorthand vars we will set
	// current instant target under consideration
	int targetx1, targety1, targetx2, targety2;
	// actually decided and planned goal targets to move towards
	int goalx1, goaly1, goalx2, goaly2;
	//
	bool isTargetMarkerless = false;

	// stickiness
	int changeMomentumCountTrigger = plugin->computedMomentumCounterTargetNormal;

	// ok get current target location (either of found box, or of whole screen if no markers found)
	if (markerBoxReady) {
		// record last good position of raw tracking box location; we will fall back on this if we lose one marker and assume its occluded
		isTargetMarkerless = false;
		//
		lastvalidmarkerx1 = markerx1;
		lastvalidmarkery1 = markery1;
		lastvalidmarkerx2 = markerx2;
		lastvalidmarkery2 = markery2;
		// we have a new box, set current instant targets to it
		targetx1 = markerx1;
		targety1 = markery1;
		targetx2 = markerx2;
		targety2 = markery2;
		//
		// we found a good box again, so cancel any hunting for a wider view..
		sourceTrackerp->stopHuntingIfPullingWide();
	} else {
		// we have not found a valid tracking box
		isTargetMarkerless = true;
		//
		changeMomentumCountTrigger = plugin->computedMomentumCounterTargetMissingMarkers;
		// so we have some options now -- let go of any current zoomcrop, or wait, or smoothly move, etc...
		// should we assume missing markers are a target to turn off all crops?

		// we dont have a new target, but we are zoomcropped to somewhere, so make new target full screen
		// set targets to full size (we can use this if we want to slowly adjust to new target of full screen)
		// IMPORTANT: if we are going to assume no markers mean go wide screen, we should have max delay before changing to this wide screen, in case we've just lost a tracking point

		// if we found one marker (not two), and it's at prior location, just assume the other got briefly occluded and do nothing
		if (isOneValidRegionAtUnchangedLocation()) {
			//info("Checking for isOneValidRegionAtUnchangedLocation returned true.");
			targetx1 = lastvalidmarkerx1;
			targety1 = lastvalidmarkery1;
			targetx2 = lastvalidmarkerx2;
			targety2 = lastvalidmarkery2;
		}
		else {
			// switch to target being our markerless default (full screen normally)
			// ATTN: NEW - we may want to switch to a new source entirely, which is outside our purview of this function
			// in that case, what should we do
			plugin->fillCoordsWithMarkerlessCoords(&targetx1, &targety1, &targetx2, &targety2);
			//mydebug("No markers found setting to markerless coords.");
		}
		// we didnt find a valid box, so start hunting wider, already? or only after some time
		// let's wait...
	}

	// if no current box, then instantly initialize to the whole screen as our starting (current) point
	if (!lookingBoxReady) {
		// we need to initialize where we are looking; use our markerless default (full screen normally)
		// ATTN: NEW - we may want to switch to a new source entirely, which is outside our purview of this function
		// in that case, what should we do
		plugin->fillCoordsWithMarkerlessCoords(&lookingx1, &lookingy1, &lookingx2, &lookingy2);
	}

	// should we immediately move towards our chosen target, or do we want some stickiness at current location or previous target to avoid JrPlugin::jitters and momentary missing markers?
	if (!stickyBoxReady) {
		// we dont have a prior target, so we use current location as our initial default prior "sticky" location
		stickygoalx1 = lookingx1;
		stickygoaly1 = lookingy1;
		stickygoalx2 = lookingx2;
		stickygoaly2 = lookingy2;
		// ready from here on out
		stickyBoxReady = true;
	}


	// at this point we have our current view focus (lookingx1...) and our newly found tracking box (markerx1) and our PRIOR box location (stickygoalx1)

	// ok now decide whether to move to the new box (tx values or stick with next)

	// if we wanted to be sticky and ignore new target box, just leave priorx alone
	// if we want to immediately start moving to new target, set priorx to tx
	bool switchToNewTarget = false;
	bool averageToCloseTarget = false;
	if (plugin->trackingOneShot) {
		switchToNewTarget = true;
	}

	// decide how much distance away triggers a need to change
	float distanceThresholdTriggersMove;
	if (changeTargetMomentumCounter >= changeMomentumCountTrigger) {
		// if we have exceeded stickiness and are now moving, then we insist on getting CLOSER to target than normal before we start moving
		distanceThresholdTriggersMove = plugin->computedChangeMomentumDistanceThresholdMoving;
	} else {
		// once we are stabilized we require more deviation before we break off and start moving again
		distanceThresholdTriggersMove = plugin->computedChangeMomentumDistanceThresholdStabilized;
	}

	// keep track of when target is in motion
	bool newTargetInMotion = true;
	bool targetIsStable = false;
	float thisTargetDelta;
	if (!lastTargetBoxReady) {
		// reset to default
		targetStableCount = 0;
	} else {
		// ok we can compare the new change in target location and accumulate it, so we we figure out when target is in motion
		float thisTargetDelta = abs(targetx1 - lasttargetx1) + abs(targety1 - lasttargety1) + abs(targetx2 - lasttargetx2) + abs(targety2 - lasttargety2);
		if (thisTargetDelta < plugin->computedThresholdTargetStableDistance) {
			// target has stayed still, increase stable count
			++targetStableCount;
			if (targetStableCount > plugin->computedThresholdTargetStableCount) {
				newTargetInMotion = false;
			}
		} else {
			// target moved too much, reset counter
			targetStableCount = 0;
		}
	}
	//
	// remember last target locations so we can see if targets have settled down somewhere
	lasttargetx1 = targetx1;
	lasttargety1 = targety1;
	lasttargetx2 = targetx2;
	lasttargety2 = targety2;
	lastTargetBoxReady = true;

	// compute distance of new target from current position
	float newTargetDeltaBoxAll = abs(targetx1-lookingx1) + abs(targety1-lookingy1) + abs(targetx2-lookingx2) + abs(targety2-lookingy2);
	float newTargetDeltaPriorAll = fabs(targetx1-stickygoalx1) + fabs(targety1-stickygoaly1) + fabs(targetx2-stickygoalx2) + fabs(targety2-stickygoaly2);
	//
	if (newTargetInMotion) {
		// when new target is in motion, we avoid JrPlugin::making any changes
		// instead we wait for target to stabilize then we move
		//mydebug("New target is in motion so not setting averageCloseToTarget.");
	} else if (newTargetDeltaBoxAll > distanceThresholdTriggersMove) {
		// target is fare enough from where we are looking
		//mydebug("Target is fare enough from where we are looking %f > %f.",newTargetDeltaBoxAll, distanceThresholdTriggersMove);
		if (newTargetDeltaPriorAll < distanceThresholdTriggersMove) {
			// but too close to prior goal, so let it drift but dont pick new target, and dont reset counter so we will be fast to react
			// this also helps us more clearly know when we are choosing a dramatically new target and not set switchToNewTarget when doing minor tweaks
			//mydebug("avergageToCloseTarget setting true.");
			averageToCloseTarget = true;
		} else {
			// new target is far enough way from sticky goal, lets build some momentum to switch to it
			//mydebug("new target is far enough way from sticky goal: %f not < %f.",newTargetDeltaBoxAll, distanceThresholdTriggersMove);
			changeTargetMomentumCounter += 1;
			if (changeTargetMomentumCounter >= changeMomentumCountTrigger) {
				// ok we have exceeded stickiness, we are ready to move to it.
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
		// target is stable and where we are very near, so reset momentum
		targetIsStable = true;
		//
		//mydebug("target is stable comparing %f >? %f.",newTargetDeltaBoxAll, plugin->computedChangeMomentumDistanceThresholdDontDrift);
		changeTargetMomentumCounter = 0;
		// but let's drift slowly to new target averaging out
		if (newTargetDeltaBoxAll> plugin->computedChangeMomentumDistanceThresholdDontDrift) {
			//mydebug("Setting averageToCloseTarget true.");
			averageToCloseTarget = true;
		} else {
			// it's close enough that we will not even TRY to adjust
		}
	}

	// one shot mode always snaps to new target location, as if this target was long stable
	if (plugin->trackingOneShot) {
		switchToNewTarget = true;
	}
	if (oneTimeInstantJumpToMarkers) {
		switchToNewTarget = true;
	}

	// ok now we have to decide if we want to update STICKY goal, or leave it where it is
	// so our GOAL during every cycle is to move towards sticky position
	// and the question is just when to update sticky

	// immediately switch over to move to newly found target box?
	if (switchToNewTarget) {
		//mydebug("sitting here at switchToNewTarget true.");
		// make our new target the newly found box
		stickygoalx1 = targetx1;
		stickygoaly1 = targety1;
		stickygoalx2 = targetx2;
		stickygoaly2 = targety2;
		// NOW we will trigger a hunt depending on the nature of the target
		// i dont think we need to worry if travelingToNewTarget is set because i *think* that switchToNewTarget will only set when the target has CHANGED
		if (true || travelingToNewTarget) {
			// this is a new target since we last arrived somewhere -- we don't keep retriggering if target 
			if (isTargetMarkerless) {
				sourceTrackerp->initiateHuntOnTargetLostPullOut();
			}
			else {
				// the problem here is that this would trigger as soon as we decided to go TOWARDS a new target.. i think we would rather trigger this after we arrive?
				// ATTN: TODO trigger on a NEW stable averageToCloseTarget state
				sourceTrackerp->initiateHuntOnNewTargetFoundPushIn();
			}
		}
		// set flag
		travelingToNewTarget = true;
	}
	else if (averageToCloseTarget) {
		//mydebug("In averageToCloseTarget 1.");
		// we consider ourselves close enough to the new target that we arent going to do anything, but we can drift average out towards it
		// the averageing should stop jitter but the drifting should keep us from locking on a point 1 pixel away that is wrong
		float computedAverageSmoothingRateToCloseTarget = plugin->computedAverageSmoothingRateToCloseTarget;
		stickygoalx1 = (computedAverageSmoothingRateToCloseTarget * (float)stickygoalx1)  + ((1.0f - computedAverageSmoothingRateToCloseTarget) * (float)targetx1);
		stickygoaly1 = (computedAverageSmoothingRateToCloseTarget * (float)stickygoaly1)  + ((1.0f - computedAverageSmoothingRateToCloseTarget) * (float)targety1);
		stickygoalx2 = (computedAverageSmoothingRateToCloseTarget * (float)stickygoalx2)  + ((1.0f - computedAverageSmoothingRateToCloseTarget) * (float)targetx2);
		stickygoaly2 = (computedAverageSmoothingRateToCloseTarget * (float)stickygoaly2)  + ((1.0f - computedAverageSmoothingRateToCloseTarget) * (float)targety2);

		// this is weird but letting prior vals be floats causes painful flickering of size when locations flicker
		if (plugin->computedIntegerizeStableAveragingLocation) {
			stickygoalx1 = (int)stickygoalx1;
			stickygoaly1 = (int)stickygoaly1;
			stickygoalx2 = (int)stickygoalx2;
			stickygoaly2 = (int)stickygoaly2;
		}
		if (travelingToNewTarget) {
			// we just arrived at new target we've been traveling to
			//mydebug("In travelingToNewTarget 1.");
			travelingToNewTarget = false;
			if (isTargetMarkerless) {
				sourceTrackerp->initiateHuntOnTargetLostPullOut();
			} else {
				sourceTrackerp->initiateHuntOnNewTargetFoundPushIn();
			}
		}
		// we ALWAYS increase slowCHangeStreakCounter in this case where we are TWEAKING the position
		++slowChangeStreakCounter;
	} else {
		// don't adjust sticky.. leave it where it was
		//mydebug("In NOT averageToCloseTarget.");
		// 
		// a little unusual but if we dont change at all since we checked, then dont increase this; only if we have started a tweak
		if (slowChangeStreakCounter > 0) {
			++slowChangeStreakCounter;
		}
	}

	// occasional check for zoom in camera
	if (targetIsStable) {
		if (slowChangeStreakCounter > DefPollAveragingToNearTargetZoomInCheckMult) {
			// do a push in test as if on new target
			slowChangeStreakCounter = 0;
			sourceTrackerp->initiateHuntOnLongTimeStable();
		}
	} else {
		slowChangeStreakCounter = 0;
	}

	// trigger this when we stabilize to a markerless target
	if (targetIsStable && isTargetMarkerless) {
		// stuck at markerless for long enough, we try to look again for zoomed out
		++markerlessStreakCounter;
		//mydebug("In ++markerlessStreakCounter: %d", markerlessStreakCounter);
		if (markerlessStreakCounter > DefPollMissingMarkersZoomOutCheckMult) {
			markerlessStreakCounter = 0;
			sourceTrackerp->initiateHuntOnLongTimeMarkerless();
		}
	}

	// ok now we have our chosen current TARGET updated in stickygoalx1
	// and we will move towards it in some gradual or instant way
	goalx1 = stickygoalx1;
	goaly1 = stickygoaly1;
	goalx2 = stickygoalx2;
	goaly2 = stickygoaly2;



	// some options at this point are:
	// 1. instantly switch to the new box
	// 2. smoothly move to the new box

	// move towards new box
	// ATTN: TODO easing - https://easings.net/#easeInOutCubic
	if (oneTimeInstantJumpToMarkers || plugin->opt_zcBoxMoveSpeed == 0 || plugin->opt_zcEasing == 0 ) {
		// instant jump to new target
		lookingx1 = goalx1;
		lookingy1 = goaly1;
		lookingx2 = goalx2;
		lookingy2 = goaly2;
	} else if (plugin->opt_zcEasing == 1 || true) {
		// gradually move towards new box

		// we are go to morph/move MIDPOINT to MIDPOINT and dimensions to dimensions
		// delta(change) measurements
		float xmid = (float)(goalx1 + goalx2) / 2.0f;
		float ymid = (float)(goaly1 + goaly2) / 2.0f;
		float bxmid = (float)(lookingx1 + lookingx2) / 2.0f;
		float bymid = (float)(lookingy1 + lookingy2) / 2.0f;
		float deltaXMid = (xmid - bxmid);
		float deltaYMid = (ymid - bymid);
		float bwidth = lookingx2 - lookingx1;
		float bheight = lookingy2 - lookingy1;
		float deltaWidth = abs(goalx2 - goalx1) - fabs(bwidth);
		float deltaHeight = abs(goaly2 - goaly1) - fabs(bheight);
		float largestDelta = jrmax4(fabs(deltaXMid), fabs(deltaYMid), fabs(deltaWidth), fabs(deltaHeight));
		//float largestDelta = jrmax4(abs(deltaXMid), abs(deltaYMid), abs(deltaWidth*deltaHeight));
		float baseMoveSpeed = plugin->opt_zcBoxMoveSpeed;
		// modify speed upward based on distance to cover
		float moveSpeedMult = 50.0f;
		float maxMoveSpeed = baseMoveSpeed * (1.0f + moveSpeedMult * (largestDelta / (float)max(plugin->workingWidth, plugin->workingHeight)));
		float safeDelta = min(largestDelta, maxMoveSpeed);
		float percentMove = min(safeDelta / largestDelta, 1.0);
		// info("Debugging boxmove deltaWidth = %f  deltaHeight = %f  deltaXMid = %f deltaYmid = %f   largestDelta = %f  safeDelta = %f   percentMove = %f.",deltaWidth, deltaHeight, deltaXMid, deltaYMid, largestDelta, safeDelta, percentMove);
		//
		// adjust b midpoints and dimensions
		bxmid += deltaXMid * percentMove;
		bymid += deltaYMid * percentMove;
		bwidth += deltaWidth * percentMove;
		bheight += deltaHeight * percentMove;

		// now set goalx1, etc. baded on width and new dimensions
		lookingx1 = bxmid - (bwidth / 2.0f);
		lookingy1 = bymid - (bheight / 2.0f);
		lookingx2 = lookingx1 + bwidth;
		lookingy2 = lookingy1 + bheight;

	} else if (plugin->opt_zcEasing == 2) {
		// eased movement
	}

	// note that we have a cropable box (this does not go false just because we dont find a new one -- it MIGHT stay valid)
	lookingBoxReady = true;

	if (oneTimeInstantJumpToMarkers) {
		oneTimeInstantJumpToMarkers = false;
	}
}
//---------------------------------------------------------------------------











//---------------------------------------------------------------------------
bool TrackedSource::findNewCandidateTrackingBox() {
	// reset to no box found

	// pointers
	JrRegionDetector* rd = sourceTrackerp->getRegionDetectorp();
	JrPlugin* plugin = sourceTrackerp->getPluginp();

	// helpers
	int outWidth = plugin->outWidth;
	int outHeight = plugin->outHeight;
	int workingWidth = plugin->workingWidth;
	int workingHeight = plugin->workingHeight;
	float stageShrink = plugin->stageShrink;

	clearFoundTrackingBox();

	if (false) {
		// TEST
		markerx1 = 400;
		markery1 = 135;
		markerx2 = 1000;
		markery2 = 535;
		markerBoxReady = true;
		rd->foundRegions = 0;
		return true;
	}

	// mark regions quality to being markers
	sourceTrackerp->markRegionsQuality();

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
	float minDist = ((float)workingWidth / plugin->computedMinDistFractionalBetweenMarkersToReject) / stageShrink;
	if (max(ity2 - ity1, itx2 - itx1) < minDist) {
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
		}
		if (true) {
			// horizontally inline markers?
			float midpointDist = fabs(((float)(region1->y1 + region1->y2) / 2.0f) - ((float)(region2->y1 + region2->y2) / 2.0f));
			float avgRegionSize = (float)((region1->y2 - region1->y1) + (region2->y2 - region2->y1)) / 2.0f;
			float distx = (float)max(abs(markerx2 - markerx1), 1.0f);
			float disty = (float)max(abs(markery2 - markery1), 1.0f);
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
		float sz = ((float)((markerx2 - markerx1) + 1.0f) * aspectRatio) + 1.0f;
		markery2 = (float)markery1 + sz;
		if (markery2 > workingHeight - 1) {
			int dif = markery2 - (workingHeight - 1);
			markery2 = workingHeight - 1;
			markery1 = markery2 - sz;
		}
	}
	else if (vertInline) {
		// so we leave the y values alone and move x2 up to match aspect
		float aspectRatio = (float)outWidth / (float)outHeight;
		float sz = ((float)((markery2 - markery1) + 1.0f) * aspectRatio) - 1.0f;
		markerx2 = (float)markerx1 + sz;
		if (markerx2 > workingWidth-1) {
			int dif = markerx2 - (workingWidth-1);
			markerx2 = workingWidth-1;
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
	if (markerx1 >= workingWidth) {
		markerx1 = workingWidth-1;
	}
	if (markery1 >= workingHeight) {
		markery1 = workingHeight-1;
	}
	if (markerx2 < 0) {
		markerx2 = 0;
	}
	if (markery2 < 0) {
		markery2 = 0;
	}
	if (markerx2 >= workingWidth) {
		markerx2 = workingWidth-1;
	}
	if (markery2 >= workingHeight) {
		markery2 = workingHeight-1;
	}

	// tracking box now has valid data
	markerBoxReady = true;

	// remember last two good regions
	memcpy(&lastGoodMarkerRegion1, region1, sizeof(JrRegionSummary));
	memcpy(&lastGoodMarkerRegion2, region2, sizeof(JrRegionSummary));

	return true;
}
//---------------------------------------------------------------------------
































//---------------------------------------------------------------------------
void SourceTracker::init(JrPlugin* inpluginp) {
	// store pointer to plugin
	plugin = inpluginp;

	//mydebug("In SourceTracker::init.");

	// important that we initialize all to blank
	for (int i=0; i < DefMaxSources; ++i) {
		TrackedSource* tsourcep = get_source(i);
		tsourcep->init(this,i);
	}
	sourceCount = 0;

	// init region detector
	regionDetector.rdInit();

	// clear any hunting
	huntDirection = 0;

	// test
	sourceIndexViewing = 0;
	sourceIndexTracking = 0;
}


void SourceTracker::freeForExit() {
	// tell region detector to free up
	regionDetector.rdFree();

	// release any currently held sources
	for (int i=0; i < sourceCount; ++i) {
		freeAndClearTrackedSourceByIndex(i);
	}

	sourceCount = 0;
}


void SourceTracker::freeBeforeReallocate() {
	// called from plugin recreateStagingMemoryIfNeeded
	// this is superfluous, it will be called immediately after by caller doing a resize
	regionDetector.rdFree();
}


void SourceTracker::setStageSize(int stageWidth, int stageHeight) {
	// tell region detctor about resize
	regionDetector.rdResize(stageWidth, stageHeight);
}


void SourceTracker::reviseSourceCount(int reserveSourceCount) {
	// remove any sources beyound this many (this can happen if user reduces how many sources they say they are using)
	// this will NOT touch any sources under this
	for (int i = reserveSourceCount; i < sourceCount; i++) {
		freeAndClearTrackedSourceByIndex(i);
	}
	// set new source count
	sourceCount = reserveSourceCount;
}


void SourceTracker::updateSourceIndexByName(int ix, const char* src_name) {
	if (!src_name || !*src_name)
		return;

	TrackedSource* tsourcep = get_source(ix);
	if (strcmp(tsourcep->getName(), src_name) == 0) {
		// already set, nothing needs to be done
		return;
	}

	// if we are setting a new name, we free any existing reference in that slot
	if (tsourcep->src_ref) {
		freeWeakSource(tsourcep->src_ref);
		tsourcep->clear();
		// now drop down to save name
	}

	// save name -- this will be used to lookup a source later
	tsourcep->setName(src_name);
}


void SourceTracker::freeWeakSource(obs_weak_source_t* wsp) {
	if (wsp != NULL) {
		obs_weak_source_release(wsp);
	}
}

void SourceTracker::freeFullSource(obs_source_t* src) {
	if (src != NULL) {
		obs_source_release(src);
	}
}
//---------------------------------------------------------------------------





//---------------------------------------------------------------------------
// ATTN: this seems like an expensive set of operations to do on every tick -- see if we can maybe find a better way to do this, maybe only checking a source when we go to use it.
// and much of what we do here seems pointless -- why do we care if the name of the source changes as long as our references do it work
// ATTN: TODO -- stop doing all this
bool SourceTracker::checkAndUpdateAllTrackingSources() {
	bool didChangeSources = false;
	TrackedSource* tsourcep;
	for (int i = 0; i < sourceCount; i++) {
		tsourcep = &tsource[i];
		didChangeSources = didChangeSources | checkAndUpdateTrackingSource(tsourcep);
	}
	return didChangeSources;
}



bool SourceTracker::checkAndUpdateTrackingSource(TrackedSource* tsourcep) {
	// 
	//mydebug("Checking to update tracking source %s.", tsourcep->getName());
	if (!tsourcep->hasValidName()) {
		// this item is not actually being used, just a blank slot in our source list
		return false;
		}

	// has a valid name -- but may not have a valid (or whats there may no longer be valid) weak reference

	// get pointer to stored name of this source
	char* oldSrcName = tsourcep->getName();

	// get a NEW strong ref to our STORED source in this slot; this MUST be released at end of this function
	// note this COULD be returned as null (if weak is null or its gone)
	obs_source_t *oldSrc = tsourcep->getFullSourceFromWeakSource();

	// now we see if our current entry here is the same or needs releasing / restoring
	bool fail = false;
	const char* newSourceName = NULL;
	if (!oldSrc) {
		// we have no valid full pointer to this reference (which has a name)-- it may be because we never did, or what we had is no longer valid
		// this happens on the FIRST time we try to use a source configured by name by user
		fail = true;
	} else {
		// we have a valid full pointer to this source  entry, use its name to RECHECK it; re-get the name in case it has changed??
		const char *realSourceName = obs_source_get_name(oldSrc);
		if (!realSourceName) {
			// failed to get source name for some reason, so just leave it alone and assume same name as before and nothing needs changing
		} else if (strcmp(realSourceName, oldSrcName) != 0) {
			// the name does not match what we expected
			fail = true;
			// we will change to the real source name
			newSourceName = realSourceName;
		} else {
			// same name as existing entry so it's NOT a fail, just drop down and leave it along (but free it)
		}
	}

	if (!fail) {
		// all good, doesn't need changing
		return false;
	}

	// it failed for one of a few reasons:
	// 1. the source is GONE (oldSrc==null) or somehow inaccessible from our weak pointer
	// 2. the source changed name (newSourceName will be different)
	// we need to update/replace this stored source
	//mydebug("Need to update src ref %s due to something changing...", oldSrcName);

	// release currently stored weak pointer (don't clear the name though)
	freeWeakSource(tsourcep->getWeakSourcep());
	tsourcep->src_ref = NULL;

	// update

	// no old source was found from our weak reference
	if (oldSrc == NULL) {
		// can we RE-look it up by name?
		obs_source_t *newSrc = obs_get_source_by_name(oldSrcName);
		if (newSrc) {
			// found it, so update new weak pointer, and leave name unchanged
			tsourcep->setWeakSourcep(obs_source_get_weak_source(newSrc));
			// and then release new source we got
			obs_source_release(newSrc);
		}
		else {
			// we couldnt find the full source from our weak pointer or look it up by name
			// so we assume its gone and just blank it out
			tsourcep->clear();
		}
	} else if (newSourceName) {
		// the weak reference did point us to a real object, but its name didnt match ours
		// so switch us over to the new name? or would we rather re-look up the name we were given, thats an odd choice
		//mydebug("Updating the name of a source, so old name = %s and new name = %s.", oldSrcName,newSourceName);
		tsourcep->setName(newSourceName);
	} else {
		// can this happen
		//obserror("Trying to update stored tracking source, but oldSrc was non-null but we have no name. Old name = %s.", oldSrcName);
		// clear it so we dont use it any more
		freeWeakSource(tsourcep->getWeakSourcep());
		tsourcep->clear();
	}

	// no matter what we have to release the full src we got above
	if (oldSrc) {
		obs_source_release(oldSrc);
	}

	// we changed something
	return true;
}//---------------------------------------------------------------------------










//---------------------------------------------------------------------------
void SourceTracker::cycleWorkingSource() {
	// test to cycle through what source we are viewing -- useful to put on a hotkey to test multiple source cycling stuff
	++sourceIndexViewing;
	if (sourceIndexViewing >= sourceCount) {
		sourceIndexViewing = 0;
	}
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
void SourceTracker::clearAllBoxReadies() {
	// loop and tell them all to clear their box ready flags
	TrackedSource* tsourcep;
	for (int i = 0; i < sourceCount; i++) {
		tsourcep = &tsource[i];
		tsourcep->clearAllBoxReadies();
	}
}
//---------------------------------------------------------------------------








































//---------------------------------------------------------------------------
void SourceTracker::markRegionsQuality() {
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


bool SourceTracker::calcIsValidmarkerRegion(JrRegionSummary* region) {
	// valid region based on plugin options
	if (region->pixelCount == 0) {
		return false;
	}

	if (region->density < plugin->opt_rmTDensityMin) {
		return false;
	}

	if (region->aspect < plugin->opt_rmTAspectMin) {
		return false;
	}

	double relativeArea1000 = (sqrt(region->area) / sqrt(plugin->stageArea)) * 1000.0;
	//mydebug("Commparing region %d area of (%d in %d) relative (%f) against min of %f.", region->label, region->area, stageArea, relativeArea1000, opt_rmTSizeMin);
	if (relativeArea1000 < plugin->opt_rmTSizeMin || (plugin->opt_rmTSizeMax>0 && relativeArea1000 > plugin->opt_rmTSizeMax) ) {
		return false;
	}

	// valid
	return true;
}
//---------------------------------------------------------------------------










//---------------------------------------------------------------------------
void SourceTracker::analyzeSceneAndFindTrackingBox(TrackedSource* tsourcep, uint8_t *data, uint32_t dlinesize, int huntingIndex) {

	// step 1 - build labeling matrix
	// this does the possibly heavy duty computation of Connected Components Contour Tracing, etc.
	analyzeSceneFindComponentMarkers(data,dlinesize);

	// step 2 identify the new candidate tracking box in the view
	// this may find nothing reliable -- that's ok
	//mydebug("in analyzeSceneAndFindTrackingBox (huntingindex=%d with source and track indices =  %d,%d).", huntingIndex,sourceIndexViewing,sourceIndexTracking);
	bool foundGoodRegion = tsourcep->findNewCandidateTrackingBox();
	if (foundGoodRegion) {
		//mydebug("foundgoodRegion.");
		if (huntingIndex != -1) {
			// we were hunting and we found a good new source to switch to
			//mydebug("in analyzeSceneAndFindTrackingBox hit one %d.", huntingIndex);
			foundGoodHuntedSourceIndex(huntingIndex);
		}
		else {
		}
	} else {
		//mydebug("Did not find markers in source with huntingIndex = %d.", huntingIndex);
	}
}




void SourceTracker::analyzeSceneFindComponentMarkers(uint8_t *data, uint32_t dlinesize) {
	// step 1 build initial labels
	int foregroundPixelCount = regionDetector.fillFromStagingMemory(data, dlinesize);

	// step 2 - connected component labelsing
	int regionCountFound = regionDetector.doConnectedComponentLabeling();

	// debug
	//mydebug("JrObs CCL regioncount = %d (%d foregroundpixels) regionsize = [%d,%d].", regionCountFound, foregroundPixelCount, regionDetector.width, regionDetector.height);
}
//---------------------------------------------------------------------------













//---------------------------------------------------------------------------
void SourceTracker::getDualTrackedViews(TrackedSource*& tsourcepView, TrackedSource*& tsourcepTrack) {
	// get the TrackedSources for viewing and tracking
	// if they are the same (ie we aren't currently hunting for a new source(camera)) then tracking source will be NULL, meaning use view source
	//mydebug("getDualTrackedViewsA says view = %d and track = %d.", sourceIndexViewing, sourceIndexTracking);
	if (huntDirection == 0 || sourceIndexViewing==sourceIndexTracking) {
		// no hunting
		tsourcepView = &tsource[sourceIndexViewing];
		tsourcepTrack = NULL;
		return;
	}
	// hunting
	tsourcepView = &tsource[sourceIndexViewing];
	tsourcepTrack = &tsource[sourceIndexTracking];
	//mydebug("getDualTrackedViews says view = %d and track = %d.", sourceIndexViewing, sourceIndexTracking);
}





void SourceTracker::initiateHuntOnNewTargetFoundPushIn() {
	// if we are already closest camera, nothing to do
	if (sourceIndexViewing >= sourceCount-1) {
		return;
	}
	//mydebug("In initiateHuntOnNewTargetFoundPushIn 1.");
	// record view that started us
	huntStopIndex = sourceIndexViewing;
	// now to push in we want the CLOSEST camera that can see the markers, e.g. the HIGHEST index
	// and now the direction to search in(-1 means go wider))
	huntDirection = -1;
	huntingWider = false;
	// start at closest highest index camera
	huntIndex = sourceCount-1;
	setTrackingIndexToHuntIndex();
}


void SourceTracker::initiateHuntOnTargetLostPullOut() {
	// if we are already farthest camera, nothing to do
	//mydebug("-------------------------> In initiateHuntOnTargetLostPullOut [%d].",sourceIndexViewing);
	if (sourceIndexViewing == 0) {
		return;
	}

	// stop if we hit widest
	huntStopIndex = 0;
	// and now the direction to search in (+1 means go closer)
	huntDirection = -1;
	huntingWider = true;
	// start at next farthest camera so we stop as soon as we can see it
	huntIndex = sourceIndexViewing - 1;
	setTrackingIndexToHuntIndex();
}


void SourceTracker::initiateHuntOnLongTimeMarkerless() {
	//mydebug("In initiateHuntOnLongTimeMarkerless1 [%d,%d].",sourceIndexViewing,sourceIndexTracking);
	if (!isHunting()) {
		//mydebug("In initiateHuntOnLongTimeMarkerless2 [%d,%d].",sourceIndexViewing,sourceIndexTracking);
		initiateHuntOnTargetLostPullOut();
	}
}

void SourceTracker::initiateHuntOnLongTimeStable() {
	if (!isHunting()) {
		//mydebug("In initiateHuntOnLongTimeStable [%d,%d].",sourceIndexViewing,sourceIndexTracking);
		initiateHuntOnNewTargetFoundPushIn();
	}
}


bool SourceTracker::AdvanceHuntToNextSource() {
	// this should be called on every render loop before we search for box
	//mydebug("In AdvanceHuntToNextSource dir = %d.", huntDirection);
	if (huntDirection == 0) {
		return false;
	}
	// advance
	huntIndex += huntDirection;
	//mydebug("In AdvanceHuntToNextSourceB huntIndex = %d.", huntIndex);
	if (huntIndex < 0 || huntIndex >= sourceCount || huntIndex==huntStopIndex) {
		// reached end, so stop
		huntDirection = 0;
		return false;
	}
	// yes we moved to another source to try
	setTrackingIndexToHuntIndex();
	//mydebug("In AdvanceHuntToNextSourceC valid.");
	return true;
}
//---------------------------------------------------------------------------







//---------------------------------------------------------------------------
void SourceTracker::foundGoodHuntedSourceIndex(int huntingIndex) {
	// ok we want to disable hunting and then switch the VIEW to this index
	stopHunting();
	sourceIndexViewing = huntingIndex;
	// tell the new source we are switching to to do a one time instant jump
	get_source(huntingIndex)->oneTimeInstantJumpToMarkers = true;
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
void SourceTracker::stopHuntingIfPullingWide() {
	// this will be called if we suddenly find good markers in view and we previously lost target
	if (huntingWider && huntDirection != 0) {
		stopHunting();
	}
}
//---------------------------------------------------------------------------
