#pragma once

//---------------------------------------------------------------------------
// obs
#include <obs-module.h>
//
#include "myPluginDefs.h"
//
#include "jrRegionDetector.h"
#include "jrTrackedSource.h"
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// forward declarations to avoid recursive includes
class JrPlugin;
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
enum EnumHuntType { EnumHuntType_None, EnumHuntType_NewTargetPushIn, EnumHuntType_TargetLostPullOut, EnumHuntType_LongTimeMarkerless, EnumHuntType_LongTimeStable };
//---------------------------------------------------------------------------


class SourceTracker {
public:
	JrPlugin* plugin;
public:
	// auto switching between sources (cameras)
	int huntDirection;
	int huntIndex;
	int huntStopIndex;
	bool huntingWider;
	enum EnumHuntType huntType;
	bool shouldConcludeNoMarkersAnywhereAndSwitch;
public:
	TrackedSource tsource[DefMaxSources];
	int sourceCount;
	int sourceIndexViewing;
	int sourceIndexTracking;
public:
	bool oneTimeInstantChangeSource;
	int oneTimeInstantChangeSourceIndex;
public:
	void init(JrPlugin* inpluginp);
	void freeForExit();

	JrPlugin* getPluginp() { return plugin; };
	//
	int getViewSourceIndex() { return sourceIndexViewing; }
	int getTrackSourceIndex() { return sourceIndexTracking; }
	TrackedSource* getCurrentSourceViewing() { return &tsource[sourceIndexViewing]; };
	TrackedSource* getCurrentSourceTracking() { return &tsource[sourceIndexTracking]; };
	TrackedSource* getTrackedSourceByIndex(int ix)  { return &tsource[ix]; };
	int getSourceCount() { return sourceCount; };
	void freeAndClearTrackedSourceByIndex(int ix) { tsource[ix].freeForDelete(); }
	obs_weak_source_t* getWeakSourcepByIndex(int ix) { return tsource[ix].src_ref; };
	void setViewSourceIndex(int ix, bool useFade);
	void setTrackSourceIndex(int ix) { sourceIndexTracking = ix; }
	//
	obs_source_t* borrowFullSourceFromWeakSourcepByIndex(int ix) { return obs_weak_source_get_source(tsource[ix].src_ref); };
	//
	void reviseSourceCount(int reserveSourceCount);
	void updateSourceIndexByName(int ix, const char* name);
	//
	bool checkAndUpdateAllTrackingSources();
	bool checkAndUpdateTrackingSource(TrackedSource* tsourcep);
public:
	void cycleWorkingSource();
	void clearAllBoxReadies();
	void onOptionsChange();
	void computerMaxSourceDimensions(int& maxw, int& maxh);
public:
	void getDualTrackedViews(TrackedSource* &tsourcepView, TrackedSource* &tsourcepTrack);
	//
	void initiateHunt(EnumHuntType htype, bool inshouldConcludeNoMarkersAnywhereAndSwitch);
	void internalInitiateHuntOnNewTargetFoundPushIn();
	void internalInitiateHuntOnTargetLostPullOut();
	//
	bool AdvanceHuntToNextSource();
	bool isHunting() { return (huntDirection != 0); };
	void foundGoodHuntedSourceIndex(int huntingIndex);
	void abortHunting();
	void abortHuntingIfPullingWide();
	void setTrackingIndexToHuntIndex();
	//
	void onHuntFinished();
	void onPullOutHuntFailedToFindMarkersAnywhere();
};
//---------------------------------------------------------------------------

