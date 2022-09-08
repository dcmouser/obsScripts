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
class SourceTracker;
//---------------------------------------------------------------------------







//---------------------------------------------------------------------------
class TrackedSource {
friend class SourceTracker;
public:
	SourceTracker* sourceTrackerp;
	JrRegionDetector regionDetector;

	obs_weak_source_t* src_ref;
	char src_name[DefNameLenSource];
	int index;

	bool oneTimeInstantSetNewTarget;

	// dbx is the CURRENT DISPLAYED zoomcrop box -- it may be an in-flight moving box, progressing towards a target
	int lookingx1, lookingy1, lookingx2, lookingy2;
	// tbx is the instantaneously unstable current guessed location of marker box (we will not change bx to it until some stable delay)
	int markerx1, markery1, markerx2, markery2;
	// lastvalidmarkerx1 is the last good target before any occlusion; to be used if we lose sight of one of our two markers
	//int lastvalidmarkerx1, lastvalidmarkery1, lastvalidmarkerx2, lastvalidmarkery2;
	// current instant target under consideration
	//int targetx1, targety1, targetx2, targety2;
	// last cycles identified marker targets, useful for comparing to see how much the target is moving
	int lasttargetx1, lasttargety1, lasttargetx2, lasttargety2;
	// sticky location values which act as magnet targets while waiting for new target point to stabilize; so sticky may stay unchanged while instanteneous target candidate bounce around or change
	// note that sticky points are not nesc. the current location of current view (though often it will be); you can stick on a point you are moving to, and stickness keeps you going there.
	// we make these floats so we can more gradually drift them
	float stickygoalx1, stickygoaly1, stickygoalx2, stickygoaly2;
	// locations of last HUNT -- this helps us make sure we hunt when our position changes, even if its slow changes
	int lasthuntx1, lasthunty1, lasthuntx2, lasthunty2;

	int changeTargetMomentumCounter;
	int markerlessStreakCounter;
	int markerlessStreakCycleCounter;
	int settledLookingStreakCounter;
	//bool delayMoveUntilHuntFinishes;
	//
	JrRegionSummary lastGoodMarkerRegion1, lastGoodMarkerRegion2;
	int occludedCornerIndex;
	//
	bool markerBoxReady;
	bool lastTargetBoxReady;
	bool lookingBoxReady;
	bool stickyBoxReady;
	bool markerBoxIsOccluded;
	//
	int targetStableCount;
public:
	gs_texrender_t *stageHoldingSpaceTexrender;
	gs_texrender_t *sourceHoldingSpaceTexrender;
	gs_texrender_t *stagingTexrender;
	gs_texture_t *stageDrawingTexture;
	gs_stagesurf_t *stagingSurface;
	//
	uint8_t *stagedData;
	uint32_t stagedDataLineSize;
	int stageArea;
	float stageShrink;
	//
	uint32_t sourceWidth;
	uint32_t sourceHeight;
	uint32_t stageWidth, stageHeight;
	//
	bool needsRecheckForSize;
public:
	void init(SourceTracker* st, int indexin);
	void clear();
	void freeForDelete();
	void freeGraphicMemoryUse();
	//
	bool hasValidWeakReference() { return (src_ref != NULL); };
	bool hasValidName() { return (src_name[0]!=0); }
	obs_weak_source_t* getWeakSourcep() { return src_ref; };
	obs_source_t* borrowFullSourceFromWeakSource() { return obs_weak_source_get_source(src_ref); };
	//
	char* getName() { return src_name; };
	void setName(const char* name) { strcpy(src_name, name); };
	void setWeakSourcep(obs_weak_source_t* wsp) { src_ref = wsp; };
	//
	uint32_t getSourceWidth() { return sourceWidth; };
	uint32_t getSourceHeight() { return sourceHeight; };
	//
	void freeAndClear() { freeYourWeakSource(); clear(); }
	void freeYourWeakSource() { freeWeakSource(src_ref); src_ref = NULL; }
	//
	void clearAllBoxReadies();
	void clearFoundTrackingBox();
	bool isRegionSimilarBounds(JrRegionSummary* regiona, JrRegionSummary* regionb);
	bool isOneValidRegionAtUnchangedLocation();
public:
	static void freeWeakSource(obs_weak_source_t* wsp);
	static void freeFullSource(obs_source_t* src);
	static obs_source_t* borrowFullSourceFromWeakSource(obs_weak_source_t* src) { return obs_weak_source_get_source(src); };
	static void releaseBorrowedFullSource(obs_source_t* sourcep) { obs_source_release(sourcep); };
public:
	void updateZoomCropBoxFromCurrentCandidate();
	bool findNewCandidateTrackingBox();
public:
	void onOptionsChange();
public:
	bool allocateGraphicsData();
	void freeBeforeReallocateTexRender();
	void freeBeforeReallocateFilterTextures();
	void freeBeforeReallocateNonGraphicData();
	void recheckSizeAndAdjustIfNeeded(obs_source_t* source);
	void reallocateGraphiocsForTrackedSource();
public:
	JrRegionDetector* getRegionDetectorp() { return &regionDetector; }
	void markRegionsQuality();
	bool calcIsValidmarkerRegion(JrRegionSummary* region);
	//
	void analyzeSceneAndFindTrackingBox(uint8_t *data, uint32_t dlinesize, bool isHunting);
	void analyzeSceneFindComponentMarkers(uint8_t *data, uint32_t dlinesize);
	void setStageSize(int instageWidth, int instageHeight);
	//
	void doRenderWorkFromEffectToStageTexRender(gs_effect_t* effectChroma, obs_source_t* source);
	void doRenderWorkFromStageToInternalMemory();
	void doRenderFromInternalMemoryToFilterOutput(int outx1, int outy1, int outx2, int outy2);
	void overlayDebugInfoOnInternalDataBuffer();
	void findTrackingMarkerRegionInSource(obs_source_t* source, bool shouldUpdateTrackingBox, bool isHunting);
public:
	void doRenderAutocropBoxToScreen(obs_source_t* source);
	void doRenderAutocropBoxFadedToScreen(obs_source_t* source, TrackedSource* fromtsp, float fadePosition);
public:
	bool areMarkersBothVisibleNeitherOccluded() { return markerBoxReady && !markerBoxIsOccluded; };
	bool areMarkersBothVisibleOrOneOccluded() { return markerBoxReady; };
	bool areMarkersMissing() { return !markerBoxReady; }
public:
	void travelToMarkerless();
	void setLocationsToMarkerLocations(bool flagInstantLookingPos);
	void updateLastHuntCoordinates();
public:
	void touchRefreshDuringRenderCycle();
};




