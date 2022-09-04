//---------------------------------------------------------------------------
#include <cstdio>
//
#include "myPlugin.h"
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
// for debugging
void JrPlugin::RgbaDrawPixel(uint32_t *textureData, int ilinesize, int x, int y, int pixelVal) {
	textureData[y * (ilinesize/4) + x] = pixelVal;
}

void JrPlugin::RgbaDrawRectangle(uint32_t *textureData, int ilinesize, int x1, int y1, int x2, int y2, int pixelVal) {
	for (int iy=y1; iy<=y2; ++iy) {
		RgbaDrawPixel(textureData, ilinesize, x1, iy, pixelVal);
		RgbaDrawPixel(textureData, ilinesize, x2, iy, pixelVal);
	}
	for (int ix=x1; ix<=x2; ++ix) {
		RgbaDrawPixel(textureData, ilinesize, ix, y1, pixelVal);
		RgbaDrawPixel(textureData, ilinesize, ix, y2, pixelVal);
	}
}
//---------------------------------------------------------------------------













//---------------------------------------------------------------------------
int JrPlugin::jrSourceGetWidth(obs_source_t* src) {
	//return obs_source_get_base_width(src);
	return obs_source_get_width(src);
}

int JrPlugin::jrSourceGetHeight(obs_source_t* src) {
	//return obs_source_get_base_height(src);
	return obs_source_get_height(src);
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
















//---------------------------------------------------------------------------
// reaallocate staging memory stuff if needed
void JrPlugin::recreateStagingMemoryIfNeeded(obs_source_t* source) {
	if (!source) {
		// delete EVERYTHING if we can't find source
		workingWidth = 0;
		workingHeight = 0;
		//
		clearAllBoxReadies();
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

	// update if size changes
	bool update = false;
	uint32_t sourceWidth = jrSourceGetWidth(source);
	uint32_t sourceHeight = jrSourceGetHeight(source);
	//mydebug("recreateStagingMemoryIfNeeded 2 with dimensions %dx%d.", sourceWidth, sourceHeight);

	if (needsRebuildStaging || workingWidth != sourceWidth || workingHeight != sourceHeight || opt_rmStageShrink != (int)stageShrink) {
		//mydebug("recreateStagingMemoryIfNeeded 3.");		

		// did size change? if so we will reset boxes
		bool sizeChanged = false;
		if (workingWidth != sourceWidth || workingHeight != sourceHeight || opt_rmStageShrink != (int)stageShrink) {
			sizeChanged = true;
		}

		update = true;

		// mutex surround -- not sure if needed
		WaitForSingleObject(mutex, INFINITE);

		// set width and height based on source
		workingWidth = sourceWidth;
		workingHeight = sourceHeight;
		// output size reduction
		stageShrink = opt_rmStageShrink;
		stageWidth = workingWidth / stageShrink;
		stageHeight = workingHeight / stageShrink;
		//
		stageArea = stageWidth * stageHeight;

		// custom outputsize stuff
		updateCoordsAndOutputSize();

		// mark current boxes as invalid
		if (sizeChanged) {
			clearAllBoxReadies();
		}

		obs_enter_graphics();
		freeBeforeReallocateFilterTextures();
		//
		// allocate staging area based on DESTINATION size?
		staging_texture = gs_stagesurface_create(stageWidth, stageHeight, GS_RGBA);
		// new drawing texture
		drawing_texture = gs_texture_create(stageWidth, stageHeight, GS_RGBA, 1, NULL, GS_DYNAMIC);
		//
		obs_leave_graphics();

		info("JR Created drawing and staging texture %d by %d (%d x %d): %x", workingWidth, workingHeight, stageWidth, stageHeight,  staging_texture);


		// ATTN: IMPORTANT - this bug this is making current code blank
		freeBeforeReallocateNonGraphicData();


		// realloc internal memory
		data = (uint8_t *) bzalloc((stageWidth + 32) * stageHeight * 4);

		// source tracker and region detect helper
		stracker.setStageSize(stageWidth, stageHeight);

		// clear flag saying we need rebuild
		needsRebuildStaging = false;

		// done in protected mutex area
		ReleaseMutex(mutex);
	}
}
//---------------------------------------------------------------------------

