//---------------------------------------------------------------------------
#include "jrSourceTracker.h"
#include "myPlugin.h"
#include "obsHelpers.h"
//---------------------------------------------------------------------------





//---------------------------------------------------------------------------
void SourceTracker::init(JrPlugin* inpluginp) {
	// store pointer to plugin
	plugin = inpluginp;

	//mydebug("In SourceTracker::init.");

	// important that we initialize all to blank
	for (int i=0; i < DefMaxSources; ++i) {
		TrackedSource* tsourcep = getTrackedSourceByIndex(i);
		tsourcep->init(this,i);
	}
	sourceCount = 0;

	// clear any hunting
	huntDirection = 0;
	huntType = EnumHuntType_None;

	// test
	sourceIndexViewing = 0;
	sourceIndexTracking = 0;

	// init
	oneTimeInstantChangeSource = false;
}


void SourceTracker::freeForExit() {
	// release any currently held sources
	// note we call this on MAX SOURCES since we initialized them all at start
	// ATTN: would it be better to init and free as we go instead?
	for (int i=0; i < DefMaxSources; ++i) {
		freeAndClearTrackedSourceByIndex(i);
	}
	sourceCount = 0;
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

	TrackedSource* tsourcep = getTrackedSourceByIndex(ix);
	if (strcmp(tsourcep->getName(), src_name) == 0) {
		// already set, nothing needs to be done
		return;
	}

	// if we are setting a new name, we free any existing reference in that slot
	if (tsourcep->src_ref) {
		tsourcep->freeAndClear();
		// now drop down to save name
	}

	// save name -- this will be used to lookup a source later
	tsourcep->setName(src_name);
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
	if (DefDebugTest) {
		return false;
	}

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
	obs_source_t *oldSrc = tsourcep->borrowFullSourceFromWeakSource();

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
		// all good, doesn't need changing -- but make sure we return the full source!
		tsourcep->releaseBorrowedFullSource(oldSrc);
		return false;
	}

	// it failed for one of a few reasons:
	// 1. the source is GONE (oldSrc==null) or somehow inaccessible from our weak pointer
	// 2. the source changed name (newSourceName will be different)
	// we need to update/replace this stored source
	//mydebug("Need to update src ref %s due to something changing...", oldSrcName);

	// release currently stored weak pointer (don't clear the name though)
	tsourcep->freeYourWeakSource();

	// update

	// no old source was found from our weak reference
	if (oldSrc == NULL) {
		// can we RE-look it up by name?
		// this gets a FULL source into newSrc?
		obs_source_t *newSrc = obs_get_source_by_name(oldSrcName);
		if (newSrc) {
			// found it, so update new weak pointer, and leave name unchanged
			tsourcep->setWeakSourcep(obs_source_get_weak_source(newSrc));
			// ATTN: EXPERIMENT -- mark this source as being owned by us
			obs_source_add_active_child(plugin->getThisPluginSource(), newSrc);
			//mydebug("checkAndUpdateTrackingSource: Updating true src info for %s.", oldSrcName);
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
		//mydebug("checkAndUpdateTrackingSource: updating name to %s.", newSourceName);
	} else {
		// can this happen
		//obserror("Trying to update stored tracking source, but oldSrc was non-null but we have no name. Old name = %s.", oldSrcName);
		// clear it so we dont use it any more
		tsourcep->freeAndClear();
	}

	// no matter what we have to release the full src we got above
	if (oldSrc) {
		tsourcep->releaseBorrowedFullSource(oldSrc);
	}

	// set flag to tell source to update things like it's automatic size calculation when there has been a change to a source
	plugin->setSourcesChanged(true);

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
void SourceTracker::onOptionsChange() {
	// we might tell every tracked source that options have changed
	TrackedSource* tsourcep;
	for (int i = 0; i < sourceCount; i++) {
		tsourcep = &tsource[i];
		tsourcep->onOptionsChange();
	}
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
void SourceTracker::computerMaxSourceDimensions(int& maxw, int& maxh) {
	maxw = 0;
	maxh = 0;
	int w, h;
	TrackedSource* tsourcep;
	for (int i = 0; i < sourceCount; i++) {
		tsourcep = &tsource[i];
		w = tsourcep->getSourceWidth();
		h = tsourcep->getSourceHeight();
		if (w > maxw) {
			maxw = w;
		}
		if (h > maxh) {
			maxh = h;
		}
	}
}
//---------------------------------------------------------------------------























































//---------------------------------------------------------------------------
void SourceTracker::getDualTrackedViews(TrackedSource*& tsourcepView, TrackedSource*& tsourcepTrack) {
	// get the TrackedSources for viewing and tracking
	// if they are the same (ie we aren't currently hunting for a new source(camera)) then tracking source will be NULL, meaning use view source
	//mydebug("getDualTrackedViewsA says view = %d and track = %d.", sourceIndexViewing, sourceIndexTracking);
	if (!isHunting() || sourceIndexViewing==sourceIndexTracking) {
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
//---------------------------------------------------------------------------










//---------------------------------------------------------------------------
void SourceTracker::initiateHunt(EnumHuntType htype, bool inshouldConcludeNoMarkersAnywhereAndSwitch) {
	if (!plugin->opt_enableAutoSourceSwitching) {
		return;
	}

	//mydebug("Initiating hunt type %d.", htype);

	// set hunt type
	huntType = htype;
	shouldConcludeNoMarkersAnywhereAndSwitch = inshouldConcludeNoMarkersAnywhereAndSwitch;
	if (huntType == EnumHuntType_NewTargetPushIn) {
		internalInitiateHuntOnNewTargetFoundPushIn();
	} else 	if (huntType == EnumHuntType_TargetLostPullOut) {
		internalInitiateHuntOnTargetLostPullOut();
	} else 	if (huntType == EnumHuntType_LongTimeMarkerless) {
		// same hunt as targetLost
		// but only restart if not hunting already
		if (!isHunting()) {
			internalInitiateHuntOnTargetLostPullOut();
		}
	} else 	if (huntType == EnumHuntType_LongTimeStable) {
		// same as newtarget
		// but only restart if not hunting already
		if (!isHunting()) {
			internalInitiateHuntOnNewTargetFoundPushIn();
		}
	} else {
		// unknown
	}
}



void SourceTracker::internalInitiateHuntOnNewTargetFoundPushIn() {
	// if we are already closest camera, nothing to do
	if (sourceIndexViewing >= sourceCount-1) {
		onHuntFinished();
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


void SourceTracker::internalInitiateHuntOnTargetLostPullOut() {
	// if we are already farthest camera, nothing to do
	if (sourceIndexViewing == 0) {
		// but we may need to trigger onHuntFinished
		onHuntFinished();
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
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
bool SourceTracker::AdvanceHuntToNextSource() {
	// this should be called on every render loop before we search for box
	if (!isHunting()) {
		return false;
	}

	// advance
	huntIndex += huntDirection;

	//mydebug("In AdvanceHuntToNextSource landed on %d with dir = %d.", huntIndex, huntDirection);

	//mydebug("In AdvanceHuntToNextSourceB huntIndex = %d.", huntIndex);
	if (huntIndex < 0 || huntIndex >= sourceCount || huntIndex==huntStopIndex) {
		// reached end, so stop; and now the questions is whether we need to do anything more
		onHuntFinished();
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

	// fade between source only if we are pulling OUT not if we are pushing in, because pushing in should be almost invisible?
	bool shouldFade = (huntType == EnumHuntType_TargetLostPullOut || huntType == EnumHuntType_LongTimeMarkerless);
	setViewSourceIndex(huntingIndex,shouldFade);

	//mydebug("foundGoodHuntedSourceIndex: %d.  Aborting hunting, switching to instant jump due to dif view same? = %d.", huntingIndex, (int)alreadyViewing);
	bool alreadyViewing = (sourceIndexViewing == huntingIndex);
	if (alreadyViewing) {
		//mydebug("!!!!!!!!! WARNING - WHY DID WE FIND A GOOD HUNTED SOURCE IF WE ARE ALREADY VIEIWING THIS SOURCE??????");
	}

	// tell the new source we are switching to to do a one time instant jump
	// ATTEMPT TO FIND BUG
	//getTrackedSourceByIndex(huntingIndex)->forceTargetToMarkersNow(!alreadyViewing);
	getTrackedSourceByIndex(huntingIndex)->setLocationsToMarkerLocations(true);

	// all done
	abortHunting();
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
void SourceTracker::abortHuntingIfPullingWide() {
	// this will be called if we suddenly find good markers in view and we previously lost target
	if (isHunting() && huntingWider) {
		abortHunting();
	}
}
//---------------------------------------------------------------------------









//---------------------------------------------------------------------------
void SourceTracker::abortHunting() {
	//mydebug("Aborting hunt.");
	huntDirection = 0;
	huntType = EnumHuntType_None;
	// we don't trigger onHuntFinished as it is aborted
}



void SourceTracker::onHuntFinished() {
	// a hunt (wide or close) has finished; we don't know the result
	//mydebug("Hunt finished.");
	if ((huntType == EnumHuntType_TargetLostPullOut || huntType == EnumHuntType_LongTimeMarkerless)) {
		// the hunt was to find markers
		TrackedSource* tsp = getTrackedSourceByIndex(sourceIndexViewing);
		if (tsp && tsp->areMarkersMissing()) {
			// we failed to find markers
			onPullOutHuntFailedToFindMarkersAnywhere();
		}
	}

	// clear flags
	huntDirection = 0;
	huntType = EnumHuntType_None;
}


void SourceTracker::onPullOutHuntFailedToFindMarkersAnywhere() {
	// this is triggered when we have finished a hunt for markers and failed to find them
	// which MAY require we intervene and change the source depending on what the markerless sounrce is
	if (!plugin->opt_enableMarkerlessCoordinates || !shouldConcludeNoMarkersAnywhereAndSwitch) {
		// nothing needs to be done
		return;
	}

	// ATTN: test diable use of markerless jumping
	if (DefDebugDisableMarkerlessJumping) {
		return;
	}

	//mydebug("onPullOutHuntFailedToFindMarkersAnywhere switching to markerless source %d.", plugin->markerlessSourceIndex);
	TrackedSource* tsp = getTrackedSourceByIndex(plugin->markerlessSourceIndex);
	tsp->travelToMarkerless();
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
void SourceTracker::setTrackingIndexToHuntIndex() {
	//mydebug("Setting tracking source index to hunt: %d.", huntIndex);
	sourceIndexTracking = huntIndex;
}
//---------------------------------------------------------------------------





//---------------------------------------------------------------------------
void SourceTracker::setViewSourceIndex(int ix, bool useFade) {
	if (sourceIndexViewing != ix && useFade) {
		plugin->initiateFade(sourceIndexViewing, ix);
	}
	// set it
	sourceIndexViewing = ix;
}
//---------------------------------------------------------------------------
