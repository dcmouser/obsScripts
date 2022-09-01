//---------------------------------------------------------------------------
// see https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.58.3213&rep=rep1&type=pdf
//---------------------------------------------------------------------------


#pragma once

//---------------------------------------------------------------------------
// obs
#include <obs-module.h>
//---------------------------------------------------------------------------





//---------------------------------------------------------------------------
// defines for region connected component labeling algorithm
#define DefPixelType unsigned char
#define DefLabelType short
#define DefMaxRegionsStore 100
#define DefMaxRegionsAbort 25
#define DefMaxForegroundPixelsPercentAbort 0.10
#define DefMaxLoopEmergencyExitRatio 2
//
#define DefRegPixelColorBackground 0
#define DefRegPixelColorForeground 1
#define DefRegLabelUnset 0
#define DefRegLabelMarked -1
//
#define DefMaxLoopsBeforeEmergencyAbort 2000
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
#define regiondebug(format, ...) mydebug(format, ##__VA_ARGS__)
//---------------------------------------------------------------------------






//---------------------------------------------------------------------------
class JrRegionSummary {
public:
	// a region is defined by a bounding box and a count of how many pixels were found inside it
	int pixelCount;
	int x1, y1, x2, y2;
	float density;
	int area;
	float aspect;
	int label;
	bool valid;
};



class JrRegionDetector {
public:
	JrRegionSummary regions[DefMaxRegionsStore];
	//
	int width, height;
	DefPixelType* pixels;
	DefLabelType* labels;
	DefLabelType currentLabel;
	int foundRegions;
	int foundRegionsValid;
	int maxLoopCountBeforeAbort;

public:
	void rdInit();
	void rdFree();
	//
	void rdResize(int rwidth, int rheight);
	int fillFromStagingMemory(uint8_t* internalObsRgbAData, uint32_t linesize);
	//
	int doConnectedComponentLabeling();
	//
	void initializeRegionData(DefLabelType labelIndex);
	void updateRegionDataOnForegroundPixel(DefLabelType labelIndex, int x, int y);
	void postProcessRegionData();
	//
	void doContourTrace(uint8_t external, DefLabelType currentLabel, int16_t x, int16_t y);
	//
	inline int CompPointOffset(int x, int y) { return width * y + x; }
	inline int CompRgbaByteOffset(int x, int y, int linesize) { return y * linesize + x * 4; };
};
//---------------------------------------------------------------------------
