//---------------------------------------------------------------------------
#include <cstdio>
//
#include "myPlugin.h"
#include "jrfuncs.h"
//---------------------------------------------------------------------------










//---------------------------------------------------------------------------
void JrPlugin::jrAddPropertListChoices(obs_property_t* comboString, const char** choiceList) {
	for (int i = 0;; ++i) {
		if (choiceList[i] == NULL || strcmp(choiceList[i], "") == 0) {
			break;
		}
	obs_property_list_add_string(comboString, choiceList[i],choiceList[i]);
	}
}

int JrPlugin::jrAddPropertListChoiceFind(const char* strval, const char** choiceList) {
	for (int i = 0;; ++i) {
		if (choiceList[i] == NULL || strcmp(choiceList[i], "") == 0) {
			break;
		}
		if (strcmp(choiceList[i], strval) == 0) {
			return i;
		}
	}
	// not found
	return -1;
}
//---------------------------------------------------------------------------
























//---------------------------------------------------------------------------
void JrPlugin::parseTextCordsString(const char* coordStrIn, int* x1, int* y1, int* x2, int* y2, int defx1, int defy1, int defx2, int defy2, int maxwidth, int maxheight) {
	char coordStr[80];
	strcpy(coordStr, coordStrIn);

	char* cpos;
	if (cpos = strtok(coordStr, ",")) {
		*x1 = parseCoordStr(cpos, maxwidth);
	} else {
		*x1 = defx1 >=0 ? defx1 : maxwidth+defx1;
	}
	if (cpos = strtok(NULL, ",")) {
		*y1 = parseCoordStr(cpos, maxheight);
	} else {
		*y1 = defy1 >=0 ? defy1 : maxheight+defy1;
	}
	if (cpos = strtok(NULL, ",")) {
		*x2 = parseCoordStr(cpos, maxwidth);
	} else {
		*x2 = defx2 >=0 ? defx2 : maxwidth+defx2;
	}
	if (cpos = strtok(NULL, ",")) {
		*y2 = parseCoordStr(cpos, maxheight);
	} else {
		*y2 = defy2 >=0 ? defy2 : maxheight+defy2;
	}

	// info("Parsed cords (%s): %d, %d, %d, %d.", coordStrIn, *x1, *y1, *x2, *y2);
}


int JrPlugin::parseCoordStr(const char* cpos, int max) {
	int ival = atoi(cpos);
	if (ival < 0) {
		return max + ival;
	}
	return ival;
}
//---------------------------------------------------------------------------

































bool JrPlugin::updateMarkerlessCoordsCycle() {
	// the idea of this function is to parse the user list of markerless entries and find the one currently selected by an option and then UPDATE the markerless coords based on the source dimensions and zoom and outputsize of the current view
	// therefore we must invoke this function whenver a tracked source changes size or when user switches the entry # they want to use for markerless.
	// ATTN:TODO - i think we also need to invoke this whenever we SWITCH to the viewing a new source which could be related to the markerless choice

	// markerless source and coords say what should be shown when no markers are found -- whether this is the full source view or something more zoomed
	// parse the opt_markerlessCycleListBuf, and then fill markerlessCoords based on which opt_markerlessCycleIndex user has selected
	// gracefully handle when index is beyond what is found in the list, just correct and modulus it so it cycles around
	// note that ther markerlessCoords are only valid for the source# specified, so it is up to the plugin to switch to the desired source when moving to chosen markerless source and coords
	// note this only gets called on coming back from options or when hotkey triggers a change, so it can be "slowish"
	// format of opt_markerlessCycleListBuf is "1,0 | 2,0 | 2,1 | 2,2 | ...
	//
	bool bretv;
	char markerlessBufTemp[DefMarkerlessCycleListBufMaxSize];
	char* markerlessBufTempp = markerlessBufTemp;
	char alignBuf[80];
	char* cpos;
	int sourceNumber;
	float zoomLevel;

	//mydebug("in updateMarkerlessCoordsCycle: %s (opt_markerlessCycleIndex=%d)",opt_markerlessCycleListBuf, opt_markerlessCycleIndex);

	for (int i = 0; i <= 1; ++i) {
		int entryIndex = -1;
		// go through opt_markerlessCycleListBuf and find the SOURCE#,ZOOMLEVEL pair currently selected
		// when we find it, set markerlesscoords based on that
		// temp copy so we can use strtok
		strcpy(markerlessBufTemp, opt_markerlessCycleListBuf);
		while (cpos = strtok(markerlessBufTempp, "|")) {
			// for strtok
			markerlessBufTempp = NULL;
			// got an entry
			++entryIndex;
			//mydebug("updateMarkerlessCoordsCycle %d vs %d.", opt_markerlessCycleIndex, entryIndex);
			if (opt_markerlessCycleIndex == entryIndex) {
				char entrybuf[255];
				char varname[80];
				char varval[80];
				strcpy(entrybuf, cpos);
				char* entrybufp = entrybuf;
				char* vpos;

				// reset defaults
				sourceNumber = 0;
				zoomLevel = 0;
				strcpy(alignBuf, "");
				// now split into comma separated tuples
				while (vpos = strtok(entrybufp, ",")) {
					// found a tuple now divide by = character
					entrybufp = NULL;
					char* equalpos = strchr(vpos, '=');
					if (!equalpos) {
						continue;
					}
					// now parse
					strncpy(varname, vpos, equalpos - vpos);
					varname[equalpos - vpos] = '\0';
					strcpy(varval, equalpos + 1);
					jrtrim(varname);
					jrtrim(varval);
					// parse it
					if (strcmp(varname,"s")==0) {
						sourceNumber = atoi(varval);
					} else if (strcmp(varname,"z")==0) {
						zoomLevel = (float)atof(varval);;
					} else if (strcmp(varname,"a")==0) {
						strcpy(alignBuf, varval);
					}
				}
				// process entry
				if (sourceNumber > 0) {
					bretv = setMarkerlessCoordsForSourceNumberAndZoomLevel(sourceNumber - 1, zoomLevel, alignBuf);
				}
				// done
				return bretv;
			}
		};
		// we didn't find our entry .. lets make sure specified index is in range
		if (opt_markerlessCycleIndex>=0 && opt_markerlessCycleIndex < entryIndex) {
			// this shouldn't happen
			break;
		} else {
			// index was too high, so cycle around and try again (unless this is second pass)
			// loop again and look for fixed index
			while (opt_markerlessCycleIndex < 0) {
				opt_markerlessCycleIndex += (entryIndex+1);
			}
			opt_markerlessCycleIndex = opt_markerlessCycleIndex % (entryIndex + 1);
			//mydebug("updateMarkerlessCoordsCycle mod change to %d via %d.", opt_markerlessCycleIndex, entryIndex);
			markerlessBufTempp = markerlessBufTemp;
			//mydebug("modding restart at %d.", opt_markerlessCycleIndex);
		}
	}
	// still not found
	return false;
}








bool JrPlugin::setMarkerlessCoordsForSourceNumberAndZoomLevel(int sourceNumber, float zoomLevel, char* alignbuf) {
	// zoomLevel 0 means at original scale, and preserve aspect ratio -- this makes it ok to have black bars outside so the whole source is visible and fits LONGER dimension
	// loomLevel 1 and greater means preserve aspect ratio and ZOOM and fit (center) the SHORTER dimension, meaning there will never be black bars
	// the output aspect is outputWidth and outputHeight
	// and the source is queried source width and source height
	// gracefully handle case where source is zero sizes

	//mydebug("in setMarkerlessCoordsForSourceNumberAndZoomLevel: sourcenum=%d, zoom=%f align=%s.",sourceNumber, zoomLevel, alignbuf);

	// and lastly record the source number for going markerless
	markerlessSourceIndex = sourceNumber;

	// sanity check
	if (outputWidthPlugin <= 0 || outputHeightPlugin <= 0) {
		return false;
	}

	//mydebug("setMarkerlessCoordsForSourceNumberAndZoomLevel stage 2");

	if (sourceNumber < 0 || sourceNumber >= stracker.getSourceCount()) {
		return false;
	}


	//mydebug("setMarkerlessCoordsForSourceNumberAndZoomLevel stage 3");

	// first get the source specified and its dimensions
	TrackedSource* tsourcep = stracker.getTrackedSourceByIndex(sourceNumber);
	uint32_t sourceWidth = tsourcep->getSourceWidth();
	uint32_t sourceHeight = tsourcep->getSourceHeight();

	//mydebug("source dimensions are: %d x %d", sourceWidth, sourceHeight);

	// sanity check
	if (sourceWidth <= 0 || sourceHeight <= 0) {
		return false;
	}

	//mydebug("setMarkerlessCoordsForSourceNumberAndZoomLevel stage 4");
	float alignxmod, alignymod;
	if (strcmp(alignbuf, "ul") == 0) {
		alignxmod = 0;
		alignymod = 0;
	} else if (strcmp(alignbuf, "uc") == 0) {
		alignxmod = 1;
		alignymod = 0;
	} else if (strcmp(alignbuf, "ur") == 0) {
		alignxmod = 2;
		alignymod = 0;
	} else if (strcmp(alignbuf, "ml") == 0) {
		alignxmod = 0;
		alignymod = 1;
	} else if (strcmp(alignbuf, "mc") == 0) {
		alignxmod = 1;
		alignymod = 1;
	} else if (strcmp(alignbuf, "mr") == 0) {
		alignxmod = 2;
		alignymod = 1;
	} else if (strcmp(alignbuf, "ll") == 0) {
		alignxmod = 0;
		alignymod = 2;
	} else if (strcmp(alignbuf, "lc") == 0) {
		alignxmod = 1;
		alignymod = 2;
	} else if (strcmp(alignbuf, "lr") == 0) {
		alignxmod = 2;
		alignymod = 2;
	} else {
		alignxmod = 1;
		alignymod = 1;
	}

	// now compute markerless coords for this source
	float outAspect = (float)outputWidthPlugin / (float)outputHeightPlugin;
	float sourceAspect = (float)sourceWidth / (float)sourceHeight;
	if (zoomLevel <= 0.0f) {
		// show entire source scaled to fit longest dimension (normal algorithm will figure out how to center and zoom to fit this)
		markerlessCoordsX1 = 0;
		markerlessCoordsX2 = sourceWidth;
		markerlessCoordsY1 = 0;
		markerlessCoordsY2 = sourceHeight;
	} else {
		float xdim, ydim;
		// ok here we WANT a crop 
		if (sourceAspect > outAspect) {
			// we are longer aspect than the output, so we want to choose full height and crop out some of the sides
			ydim = (float)sourceHeight / zoomLevel;
			xdim = ydim * outAspect;
		}
		else {
			// we are taller than the output
			xdim = (float)sourceWidth / zoomLevel;
			ydim = xdim / outAspect;
		}
		float xmargin = (float)(sourceWidth - xdim) / 2.0f;
		float ymargin = (float)(sourceHeight - ydim) / 2.0f;
		markerlessCoordsX1 = (int) xmargin * alignxmod;
		markerlessCoordsX2 = sourceWidth - (int)xmargin * (2-alignxmod);
		markerlessCoordsY1 = (int) ymargin * alignymod;
		markerlessCoordsY2 = sourceHeight - (int)ymargin * (2-alignymod);
	}

	//mydebug("Set markerless details: source #%d coords (%d,%d)-(%d,%d).", markerlessSourceIndex, markerlessCoordsX1, markerlessCoordsY1, markerlessCoordsX2, markerlessCoordsY2);

	// ATTN: why are we setting this stuff below??
	/*
	// set this flag so we jump to new target when we change markerless
	if (tsourcep) {
		//mydebug("Set oneTimeInstantSetNewTarget true for this tracked source.");
		tsourcep->oneTimeInstantSetNewTarget = true;
	}
	*/

	// successfully set markerless coords
	return true;
}


void JrPlugin::markerlessCycleListAdvanceForward() {
	markerlessCycleListAdvanceDelta(1);
}
void JrPlugin::markerlessCycleListAdvanceBackward() {
	markerlessCycleListAdvanceDelta(-1);
}


void JrPlugin::markerlessCycleListAdvanceDelta(int delta) {
	//mydebug("in markerlessCycleListAdvance.");
	int old_markerlessSourceIndex = markerlessSourceIndex;
	int old_markerlessCoordsX1 = markerlessCoordsX1;
	int old_markerlessCoordsY1 = markerlessCoordsY1;
	int old_markerlessCoordsX2 = markerlessCoordsX2;
	int old_markerlessCoordsY2=markerlessCoordsY2;
	int old_opt_markerlessCycleIndex = opt_markerlessCycleIndex;

	// we try a few times to skip over settings that are equivelent which can happen if we are not useing a forced output size to change dimension of output
	int maxtries = 10;
	for (int i=0;i<maxtries;++i) {
		opt_markerlessCycleIndex += delta;
		updateMarkerlessCoordsCycle();
		if (old_opt_markerlessCycleIndex == opt_markerlessCycleIndex) {
			// we cycled around
			break;
		}
		if (old_markerlessSourceIndex != markerlessSourceIndex || old_markerlessCoordsX1 != markerlessCoordsX1 || old_markerlessCoordsY1 != markerlessCoordsY1 || old_markerlessCoordsX2 != markerlessCoordsX2 || old_markerlessCoordsY2 != markerlessCoordsY2) {
			// something changed
			break;
		}
	}
}
//---------------------------------------------------------------------------



