#pragma once

//---------------------------------------------------------------------------
// obs
#include <obs-module.h>
//
#include "myPluginDefs.h"
//
#include "jrRegionDetector.h"
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// forward declarations to avoid recursive includes
class JrPlugin;
//---------------------------------------------------------------------------





//---------------------------------------------------------------------------
class TrackedSource {
friend class SourceTracker;
public:
	SourceTracker* sourceTrackerp;
	obs_weak_source_t* src_ref;
	char src_name[DefNameLenSource];
	int index;

	uint32_t width;
	uint32_t height;
	//
	bool travelingToNewTarget;
	bool oneTimeInstantJumpToMarkers;

	// dbx is the CURRENT DISPLAYED zoomcrop box -- it may be an in-flight moving box, progressing towards a target
	int lookingx1, lookingy1, lookingx2, lookingy2;
	// tbx is the instantaneously unstable current guessed location of marker box (we will not change bx to it until some stable delay)
	int markerx1, markery1, markerx2, markery2;
	// lastvalidmarkerx1 is the last good target before any occlusion; to be used if we lose sight of one of our two markers
	int lastvalidmarkerx1, lastvalidmarkery1, lastvalidmarkerx2, lastvalidmarkery2;
	// last cycles identified marker targets, useful for comparing to see how much the target is moving
	int lasttargetx1, lasttargety1, lasttargetx2, lasttargety2;
	// sticky location values which act as magnet targets while waiting for new target point to stabilize; so sticky may stay unchanged while instanteneous target candidate bounce around or change
	// note that sticky points are not nesc. the current location of current view (though often it will be); you can stick on a point you are moving to, and stickness keeps you going there.
	float stickygoalx1, stickygoaly1, stickygoalx2, stickygoaly2;
	//
	int changeTargetMomentumCounter;
	int markerlessStreakCounter;
	int slowChangeStreakCounter;
	//
	JrRegionSummary lastGoodMarkerRegion1, lastGoodMarkerRegion2;
	//
	bool markerBoxReady;
	bool lastTargetBoxReady;
	bool lookingBoxReady;
	bool stickyBoxReady;
	//
	int targetStableCount;
public:
	void init(SourceTracker* st, int i) { sourceTrackerp = st;  index = i;  clear(); };
	void clear() { src_ref = NULL; strcpy(src_name, ""); travelingToNewTarget = false; oneTimeInstantJumpToMarkers = false; markerlessStreakCounter = slowChangeStreakCounter = 0; targetStableCount = 0; changeTargetMomentumCounter = 0;  }
	//
	bool hasValidWeakReference() { return (src_ref != NULL); };
	bool hasValidName() { return (src_name[0]!=0); }
	obs_weak_source_t* getWeakSourcep() { return src_ref; };
	obs_source_t* getFullSourceFromWeakSource() { return obs_weak_source_get_source(src_ref); };
	char* getName() { return src_name; };
	void setName(const char* name) { strcpy(src_name, name); };
	void setWeakSourcep(obs_weak_source_t* wsp) { src_ref = wsp; };
	//
	void clearAllBoxReadies();
	void clearFoundTrackingBox();
	bool isRegionSimilarBounds(JrRegionSummary* regiona, JrRegionSummary* regionb);
	bool isOneValidRegionAtUnchangedLocation();
public:
	void updateZoomCropBoxFromCurrentCandidate();
	bool findNewCandidateTrackingBox();
};







class SourceTracker {
public:
	JrPlugin* plugin;
	JrRegionDetector regionDetector;
public:
	// auto switching between sources (cameras)
	int huntDirection;
	int huntIndex;
	int huntStopIndex;
	bool huntingWider;
public:
	TrackedSource tsource[DefMaxSources];
	int sourceCount;
	int sourceIndexViewing;
	int sourceIndexTracking;
public:
	void init(JrPlugin* inpluginp);
	void freeForExit();
	//
	void freeBeforeReallocate();
	void setStageSize(int stageWidth, int stageHeight);
	//
	JrRegionDetector* getRegionDetectorp() { return &regionDetector; }
	JrPlugin* getPluginp() { return plugin; };
	//
	TrackedSource* getCurrentSourceViewing() { return &tsource[sourceIndexViewing]; };
	TrackedSource* getCurrentSourceTracking() { return &tsource[sourceIndexTracking]; };
	TrackedSource* get_source(int ix)  { return &tsource[ix]; };
	int getSourceCount() { return sourceCount; };
	void freeAndClearTrackedSourceByIndex(int ix) { freeWeakSource(tsource[ix].src_ref); tsource[ix].clear(); }
	obs_weak_source_t* getWeakSourcepByIndex(int ix) { return tsource[ix].src_ref; };
	obs_source_t* getFullSourceFromWeakSourcepByIndex(int ix) { return obs_weak_source_get_source(tsource[ix].src_ref); };
	//
	void freeWeakSource(obs_weak_source_t* wsp);
	void freeFullSource(obs_source_t* src);
	//
	void reviseSourceCount(int reserveSourceCount);
	void updateSourceIndexByName(int ix, const char* name);
	//
	bool checkAndUpdateAllTrackingSources();
	bool checkAndUpdateTrackingSource(TrackedSource* tsourcep);
public:
	void cycleWorkingSource();
	void clearAllBoxReadies();
public:
	void analyzeSceneAndFindTrackingBox(TrackedSource* tsourcep, uint8_t *data, uint32_t dlinesize, int huntingIndex);
	void analyzeSceneFindComponentMarkers(uint8_t *data, uint32_t dlinesize);
	//
	void markRegionsQuality();
	bool calcIsValidmarkerRegion(JrRegionSummary* region);
public:
	void getDualTrackedViews(TrackedSource* &tsourcepView, TrackedSource* &tsourcepTrack);
	void initiateHuntOnNewTargetFoundPushIn();
	void initiateHuntOnTargetLostPullOut();
	void initiateHuntOnLongTimeMarkerless();
	void initiateHuntOnLongTimeStable();
	bool AdvanceHuntToNextSource();
	bool isHunting() { return (huntDirection != 0); };
	void foundGoodHuntedSourceIndex(int huntingIndex);
	void stopHunting() { huntDirection = 0; }
	void stopHuntingIfPullingWide();
	void setTrackingIndexToHuntIndex() { sourceIndexTracking = huntIndex;  }
};
//---------------------------------------------------------------------------

