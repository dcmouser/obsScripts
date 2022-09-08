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
	bool isHunting = false;

	// get the TrackedSources for viewing and tracking
	// if they are the same (ie we aren't currently hunting for a new source(camera)) then tracking source will be NULL, meaning use view source
	TrackedSource* tsourcepView;
	TrackedSource* tsourcepTrack;
	stracker.getDualTrackedViews(tsourcepView, tsourcepTrack);



	// now full sources from the weak references -- remember these need to be released before we leave this function
	obs_source_t* sourceView = tsourcepView ? tsourcepView->borrowFullSourceFromWeakSource() : NULL;
	obs_source_t* sourceTrack = tsourcepTrack ? tsourcepTrack->borrowFullSourceFromWeakSource() : NULL;

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


	// do a quick size check on each source to recreate its memory if it changes sizes or on startuo
	// ATTN: TODO see if we should do this in tick() or can automatically do it more efficiently only occasionally
	if (tsourcepView) {
		if (true || tsourcepView->needsRecheckForSize) {
			tsourcepView->recheckSizeAndAdjustIfNeeded(sourceView);
		}
	}
	if (tsourcepTrack) {
		if (true || tsourcepTrack->needsRecheckForSize) {
			tsourcepTrack->recheckSizeAndAdjustIfNeeded(sourceTrack);
		}
	}
	
	// default source to track
	obs_source_t* sourcePointerToTrack = tsourcepTrack ? sourceTrack : sourceView;
	TrackedSource* trackedSourcePointerToTrack = tsourcepTrack ? tsourcepTrack : tsourcepView;
	int trackedIndexToTrack =  tsourcepTrack ? stracker.getTrackSourceIndex() : stracker.getViewSourceIndex();

	
	
	
	
	
	// record the hunting index - this is an ugly consequence of advancing the hunt before the end of this function
	//mydebug("In dorender stage 1b.");
	if (tsourcepTrack) {
		// we are hunting, record the index that we are currently checking, BEFORE we advance
		isHunting = true;
	}



	bool needsRendering = false;
	//mydebug("after possible advance we have isHunting = %d and viewindex = %d and trackindex = %d and trackedIndexToTrack = %d.", (int)isHunting, stracker.sourceIndexViewing, stracker.sourceIndexTracking, trackedIndexToTrack);


	// OVERRIDE display
	if (sourceView != NULL) {
		// we have a source view; basic info about source
		sourceViewWidth = tsourcepView->getSourceWidth();
		sourceViewHeight = tsourcepView->getSourceHeight();
		//mydebug("in plugin_render 2b with source %s size: %dx%d outw = %d,%d.", tsourcepView->getName(), sourceViewWidth, sourceViewHeight, outputWidthAutomatic, outputHeightAutomatic);

		if (sourceViewWidth && sourceViewHeight) {
			// it's a valid source valid source

			// bypass mode says do NOTHING just render source to output (note this may resize it)
			// you can force this to always be true to bypass most processing for debug testing
			if (opt_filterBypass) {
				// just render out the source untouched, and quickly
				jrRenderSourceOut(sourceView, sourceViewWidth, sourceViewHeight);
			} else {
				// needs real rendering, just drop down
				needsRendering = true;
			}
		}
	}


	if (tsourcepTrack) {
		// tracking a different source
		sourceTrackWidth = tsourcepTrack->getSourceWidth();
		sourceTrackHeight = tsourcepTrack->getSourceHeight();
	}
	else {
		// we are tracking the same source as viewing
		sourceTrackWidth = sourceViewWidth;
		sourceTrackHeight = sourceViewHeight;
	}


	//mydebug("In dorender stage 3.");
	// this first test should always be true, unless source is NULL
	if (sourcePointerToTrack && (sourceTrackWidth && sourceTrackHeight)) {
		if (shouldUpdateTrackingBox) {
			//mydebug("Calling hunted findTrackingMarkerRegionInSource with source index #%d (=%d)", trackedIndexToTrack, trackedSourcePointerToTrack->index);

			// test to see if rendering to output area might encourage tracked source to not fall out of date
			// this seems to prevent other output unfortunately
			/*
			if (DefDrawingUpdateBugfix3) {
				jrRenderSourceOutAtSizeLoc(sourcePointerToTrack, sourceTrackWidth, sourceTrackHeight, 0,0,10,10);
			}
			*/

			trackedSourcePointerToTrack->findTrackingMarkerRegionInSource(sourcePointerToTrack, shouldUpdateTrackingBox, isHunting);
			didUpdateTrackingBox = true;
		}
	}


	// ATTN: WE *MUST* CALL ADVANCE HERE (early)-- otherwise if we try doing this at end of function, and in the meanwhile initiate a NEW hunt from tsourcepView->updateZoomCropBoxFromCurrentCandidate() we will advance over it before we test the first hunt index
	// this is awkward
	// advance hunt index
	if (didUpdateTrackingBox) {
		// if we were tracking something other than our own (ie we were hunting), then advance any hunting index
		if (tsourcepTrack) {
			//mydebug("Calling advance hunt...");
			stracker.AdvanceHuntToNextSource();
		}
	}


	bool needsUpdateZoomBox = true;
	if (needsUpdateZoomBox && sourceView) {
		// update target zoom box from tracking box -- note this may happen EVEN when we are not doing a tracking box update, to provide smooth movement to target
		// ATTN: BUT.. do we want to do this on tracking source or viewing source?
		// ATTN: new -- calling it on VIEW
		// ATTN: WARNING - this can trigger a new hunt, so make sure AdvanceHuntToNextSource is called first so we dont advance over new hunt's first index before the next cycle
		tsourcepView->updateZoomCropBoxFromCurrentCandidate();
	}

	//mydebug("In dorender stage 4.");

	// now final rendering (either with debug info, or with zoom crop effect, unless we bypasses above)
	if (needsRendering) {
		//mydebug("In dorender stage 4b.");
		// if we are in debugDisplayMode then we just display the debug overlay as our plugin and do not crop
		if (opt_debugDisplay) {
			// show EVERY source in a multiview format, with overlay debug info on internal chroma buffer; this uses TRACKED size
			//  then render from internal planning data texture to display for debug purposes
			// note that we do NOT force recalculation of boxes and marker detection here -- that is done only as normal, so the non-viewed sources here will be slow to update and seem freeze frames on their last tracked/hunted state
			// which is what we want so we can see how frequently they are being hunted.
			//mydebug("in plugin_render 4b did DEBUG on %s size: %dx%d.", trackedSourcePointerToTrack->getName(), sourceTrackWidth, sourceTrackHeight);
			if (true) {
				int sourceCount = stracker.getSourceCount();
				int dbgw = outputWidthPlugin / sourceCount;
				int dbgh = outputHeightPlugin / sourceCount;
				for (int i = 0; i < sourceCount; ++i) {
					TrackedSource* tsp = stracker.getTrackedSourceByIndex(i);
					if (tsp && tsp->sourceWidth > 0) {
						// new test, lets also force tracking update on EVERY cycle
						if (DefDebugUpdateTrackingOfEverySourceDuringRender && tsp!=trackedSourcePointerToTrack /*&& tsp != tsourcepView && tsp != tsourcepTrack*/) {
							obs_source_t* osp = tsp->borrowFullSourceFromWeakSource();
							tsp->findTrackingMarkerRegionInSource(osp, true, false);
							tsp->releaseBorrowedFullSource(osp);
						}
						// ok render this source into multiview
						int dbgx1 = i * dbgw;
						int dbgy1 = 0;
						int dbgx2 = dbgx1 + dbgw;
						int dbgy2 = outputHeightPlugin;
						tsp->overlayDebugInfoOnInternalDataBuffer();
						tsp->doRenderFromInternalMemoryToFilterOutput(dbgx1, dbgy1, dbgx2, dbgy2);
					}
				}
			} 
		} else if (DefBypassZoomOnUntouchedOutput && (!tsourcepView->lookingBoxReady || (tsourcepView->lookingx1 == 0 && tsourcepView->lookingy1 == 0 && tsourcepView->lookingx2 == outputWidthPlugin-1 && tsourcepView->lookingy2 == outputHeightPlugin-1))
			&& (tsourcepView->sourceWidth==outputWidthPlugin && tsourcepView->sourceHeight==outputHeightPlugin && outputWidthPlugin == outputWidthAutomatic && outputHeightPlugin == outputHeightAutomatic)) {
			//mydebug("in plugin_render 4c did render zoomcrop on %s size: %dx%d.", tsourcepView->getName(), sourceViewWidth, sourceViewHeight);
			// we could disable this check if it's not worth it -- we should make sure if occasionally gets used
			// this check is done here just to make things go faster in case of passing through
			// nothing to do at all -- either we have not decided a place to look OR we are looking at the entire view of a non-adjusted view, so nothing needs to be done to the view
			jrRenderSourceOut(sourceView, sourceViewWidth, sourceViewHeight);
		}
		else {
			// render the zoom and crop if any; THIS IS THE MAIN REAL ZOOMCROP RENDER
			if (fadeEndingSourceIndex == tsourcepView->index && updateFadePosition()) {
				// fade between some other source and tsourcepView
				tsourcepView->doRenderAutocropBoxFadedToScreen(sourceView, stracker.getTrackedSourceByIndex(fadeStartingSourceIndex), fadePosition);
			}
			else {
				// normal view
				tsourcepView->doRenderAutocropBoxToScreen(sourceView);
			}

		}
		needsRendering = false;
	}


	// turn off one shot tracking
	if (trackingOneShot && !stracker.isHunting()) {
		// turn off one-shot once we stop hunting
		trackingOneShot = false;
	}

	//mydebug("In dorender stage 5.");

	// this is a kludge needed for some obs bug(?) which does not update sources when we do a one-shot peek at them, so we have to continuously poll them on every cycle
	// new 9/7/22 im only doing this when we are on an updatecycle, so update rate will slow this down too
	if (DefDebugTouchAllSourcesOnRenderCycle && shouldUpdateTrackingBox) {
		// this is a test to see if "touching" all other sources every cycle keeps them from getting stuck in old frames
		int sourceCount = stracker.getSourceCount();
		for (int i = 0; i < sourceCount; ++i) {
			TrackedSource* tsp = stracker.getTrackedSourceByIndex(i);
			if (true && tsp != tsourcepView && tsp != tsourcepTrack) {
				tsp->touchRefreshDuringRenderCycle();
			}
		}
	}


	// release source (it's ok if we pass NULL)
	tsourcepView->releaseBorrowedFullSource(sourceView); sourceView = NULL;
	tsourcepTrack->releaseBorrowedFullSource(sourceTrack); sourceTrack = NULL;
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
bool JrPlugin::doCalculationsForZoomCropEffect(TrackedSource* tsourcep) {
	// this code need refactoring
	// a good part of this code is dealing with situation when we have a forced output size different from the full size of our source

	float sourceWidth = tsourcep->getSourceWidth();
	float sourceHeight = tsourcep->getSourceHeight();

	float oWidth = outputWidthPlugin;
	float oHeight = outputHeightPlugin;


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

