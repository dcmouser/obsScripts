#pragma once


//---------------------------------------------------------------------------
// chroma options taken from chroma plugin
#define SETTING_COLOR_TYPE						"key_color_type"
#define TEXT_COLOR_TYPE							obs_module_text("KeyColorType")
#define SETTING_Def_COLOR_TYPE					"green"
#define SETTING_KEY_COLOR						"key_color"
#define TEXT_KEY_COLOR							obs_module_text("KeyColor")
#define SETTING_SIMILARITY						"similarity"
#define TEXT_SIMILARITY							obs_module_text("Similarity")
#define SETTING_SMOOTHNESS						"smoothness"
#define TEXT_SMOOTHNESS							obs_module_text("Smoothness")
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
#define SETTING_filterBypass					"filterBypass"
#define TEXT_filterBypass						obs_module_text("filterBypass")
#define SETTING_debugDisplay					"dbgDisplay"
#define TEXT_debugDisplay						obs_module_text("dbgDisplay")
#define SETTING_enableAutoUpdate				"enableAutoUpdate"
#define TEXT_enableAutoUpdate					obs_module_text("enableAutoUpdate")
#define SETTING_updateRate						"updateRate"
#define TEXT_updateRate							obs_module_text("updateRate")
//
// valid region filtering
#define SETTING_rmDensityMin					"rmTDensityMin"
#define TEXT_rmDensityMin						obs_module_text("rmTDensityMin")
#define SETTING_Def_rmDensityMin				50
#define SETTING_rmAspectMin						"rmTAspectMin"
#define TEXT_rmAspectMin						obs_module_text("rmTAspectMin")
#define SETTING_Def_rmAspectMin					70
#define SETTING_rmSizeMin						"rmTSizeMin"
#define TEXT_rmSizeMin							obs_module_text("rmTSizeMin")
#define SETTING_Def_rmSizeMin					4
#define SETTING_rmSizetMax						"rmTSizeMax"
#define TEXT_rmSizetMax							obs_module_text("rmTSizeMax")
#define SETTING_Def_rmSizetMax					30
#define SETTING_rmStageShrink					"rmStageShrink"
#define TEXT_rmStageShrink						obs_module_text("rmStageShrink")
#define SETTING_Def_rmStageShrink				4

// zoomCrop stuff
#define SETTING_zcPreserveAspectRatio			"zcPreserveAspectRatio"
#define TEXT_zcPreserveAspectRatio				obs_module_text("zcPreserveAspectRatio")

#define SETTING_zcMarkerPos						"zcMarkerPos"
#define TEXT_zcMarkerPos						obs_module_text("zcMarkerPos")
#define SETTING_Def_MarkerPos					"center"

#define SETTING_zcBoxMargin						"zcBoxMargin"
#define TEXT_zcBoxMargin						obs_module_text("zcBoxMargin")
#define SETTING_Def_zcBoxMargin					0
#define SETTING_zcBoxMoveSpeed					"zcBoxMoveSpeed"
#define TEXT_zcBoxMoveSpeed						obs_module_text("zcBoxMoveSpeed")
#define SETTING_Def_zcBoxMoveSpeed				8
#define SETTING_zcBoxMoveDistance				"zcBoxMoveDistance"
#define TEXT_zcBoxMoveDistance					obs_module_text("zcBoxMoveDistance")
#define SETTING_Def_zcBoxMoveDistance			45
#define SETTING_zcBoxMoveDelay					"zcBoxMoveDelay"
#define TEXT_zcBoxMoveDelay						obs_module_text("zcBoxMoveDelay")
#define SETTING_Def_zcBoxMoveDelay				10


#define SETTING_zcAlignment						"Alignment"
#define TEXT_zcAlignment						obs_module_text("Alignment")

#define SETTING_Def_zcAlignment					"center"

#define SETTING_zcMode							"ZcMode"
#define TEXT_zcMode								obs_module_text("ZcMode")

#define SETTING_Def_zcMode						"zoomCrop"
#define Def_zcMode_ZoomCrop	0
#define Def_zcMode_OnlyCrop	1
#define Def_zcMode_OnlyZoom	2
#define SETTING_zcMaxZoom						"MaxZoom"
#define TEXT_zcMaxZoom							obs_module_text("MaxZoom")
#define SETTING_zcForcedOutputRes				"ForcedOutputRes"
#define TEXT_zcForcedOutputRes					obs_module_text("ForcedOutputRes")
#define SETTING_Def_zcForcedOutputRes			""

#define SETTING_zcMarkerlessCycleList			"markerlessCycleList"
#define TEXT_zcMarkerlessCycleList				obs_module_text("markerlessCycleList")
#define SETTING_Def_zcMarkerlessCycleList		"1,0 | 2,0 | 2,1 | 2,2"
#define DefMarkerlessCycleListBufMaxSize		255
/*
#define SETTING_zcMarkerlessCoords				"markerlessCoords"
#define TEXT_zcMarkerlessCoords					obs_module_text("markerlessCoords")
#define SETTING_Def_zcMarkerlessCoords			"0,0,-1,-1"
#define SETTING_zcMarkerlessCoordsButton		"zcMarkerlessCoordsButton"
#define TEXT_zcMarkerlessCoordsButton			obs_module_text("setMarkerLessCoordsFromCurrentBox")
#define SETTING_zcMarkerlessSourceIndex			"markerlessSourceIndex"
#define TEXT_zcMarkerlessSourceIndex			obs_module_text("markerlessSourceIndex")
#define SETTING_Def_zcMarkerlessSourceIndex		0
*/


#define SETTING_zcEasing						"zcEasing"
#define TEXT_zcEasing							obs_module_text("zcEasing")

#define SETTING_Def_zcEasing					"eased"
#define SETTING_zcHomeBox						"zcHomeBox"
#define TEXT_zcHomeBox							obs_module_text("zcHomeBox")

//
#define SETTING_srcN							"NumSources"
#define TEXT_srcN								obs_module_text("NumSources")
#define SETTING_Def_srcN						2
//---------------------------------------------------------------------------









//---------------------------------------------------------------------------
// non-confiruable options

// for the showImage effect option alpha value
#define DefEffectAlphaShowImage							0.50f
// the larget the slower the box moves as max speed
#define DefMaxMoveBoxSpeedRatio							15.0f
// momentum thresholds
#define DefMomentumCounterTargetNormal					5
#define DefMomentumCounterTargetMissingMarkers			30
//

#define DefChangeMomentumDistanceThresholdMoving		(opt_zcBoxMoveDistance/2)
#define DefChangeMomentumDistanceThresholdStabilized	(opt_zcBoxMoveDistance/1.5)
//
#define DefThresholdTargetStableDistance				(opt_zcBoxMoveDistance)
//
#define DefChangeMomentumDistanceThresholdDontDrift		(opt_zcBoxMoveDistance/4)
#define DefIntegerizeStableAveragingLocation			false
//
#define DefThresholdTargetStableCount					(opt_zcBoxMoveDelay/2)
//
#define DefAverageSmoothingRateToCloseTarget			0.90f
//
#define DefThresholdMovingTargetCount					(opt_zcBoxMoveDelay)
//
#define DefThresholdDistanceForPhantomMarker			(opt_zcBoxMoveDistance/4)
//
#define DefMinSizeMoveSpeedReduce						0.50f
//
#define DefMinDistFractionalBetweenMarkersToReject		100.0f
//
#define DefMaxSources									4
#define DefNameLenSource								16
//
#define DefAlwaysUpdateTrackingWhenHunting				true
//
#define DefPollMissingMarkersZoomOutCheckMult			10
#define DefPollAveragingToNearTargetZoomInCheckMult		20
//---------------------------------------------------------------------------









//---------------------------------------------------------------------------
#define jrmax4(a,b,c,d) max(max(a,b),max(c,d))
//---------------------------------------------------------------------------


