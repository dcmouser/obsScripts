//---------------------------------------------------------------------------
#include "jrRegionDetector.h"
#include "obsHelpers.h"
//---------------------------------------------------------------------------



void JrRegionDetector::rdInit() {
	pixels = NULL;
	labels = NULL;
	width = 0;
	height = 0;
    foundRegions = 0;
    foundRegionsValid = 0;
}

void JrRegionDetector::rdFree() {
	if (pixels) {
		bfree(pixels);
		pixels = NULL;
	}
	if (labels) {
		bfree(labels);
		labels = NULL;
	}
}



void JrRegionDetector::rdResize(int rwidth, int rheight) {
	// free existing pixel data
	rdFree();

	width = rwidth;
	height = rheight;

	// allocate enough space for border of pixels around, for easier neighborhood checking
	int pixelcount = (width * height);
	pixels = (unsigned char*) bzalloc(sizeof(DefPixelType) * pixelcount);
	labels = (short*) bzalloc(sizeof(DefLabelType) * pixelcount);

	// initialize all to 0 (borders will stay 0 even as we add new data)?
	//memset(pixels, 0, pixelcount * sizeof(DefPixelType));
    foundRegions = 0;
    foundRegionsValid = 0;
    //
    maxLoopCountBeforeAbort = (float)(width + height) * DefMaxLoopEmergencyExitRatio;
}


int JrRegionDetector::fillFromStagingMemory(uint8_t* internalObsRgbAData, uint32_t linesize) {
	// ATTN: later optimize this for speed
    DefPixelType pixelColor;
    int foregroundPixelCount = 0;
	//
	// pixelThreshold test
	//uint8_t pixelThreshold = 254;
	uint8_t pixelThreshold = 254;
    int maxLoopPixelsBeforeAbort = ((float)width * (float)height) * DefMaxForegroundPixelsPercentAbort;

    //regiondebug("NOTE: filling fillFromStagingMemory.");

	//
	// we pad a border around the entire grid
	//
    int pixelcount = width * height;
	//
	// fill our internal pixel grid (never touch outer border which should stay 0); this is just 0s and 1s
	for (int iy = 0; iy < height; ++iy) {
        for (int ix = 0; ix < width; ++ix) {
            pixelColor = internalObsRgbAData[CompRgbaByteOffset(ix, iy, linesize)+3] > pixelThreshold ? DefRegPixelColorForeground : DefRegPixelColorBackground;
            // set it to 1 if its our marker or 0 otherwise
            pixels[CompPointOffset(ix,iy)] = pixelColor;
            // debug
            if (pixelColor == DefRegPixelColorForeground) {
                ++foregroundPixelCount;
                if (foregroundPixelCount > maxLoopPixelsBeforeAbort && maxLoopPixelsBeforeAbort>0) {
                    iy = height;
                    ix = width;
                    //regiondebug("Aborting fillFromStagingMemory because of too many foreground pixels (%d vs %d).",foregroundPixelCount,maxLoopPixelsBeforeAbort);
                    break;
                }
            }
		}
	}

	// reset labels all to Unset
	memset(labels, DefRegLabelUnset, pixelcount * sizeof(DefLabelType));

    // return number of foreground pixels found, for debugging
    return foregroundPixelCount;
}

















// see https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.58.3213&rep=rep1&type=pdf
// see https://github.com/BlockoS/blob

/* Extract blob contour (external or internal). */
void JrRegionDetector::doContourTrace(uint8_t external, DefLabelType currentLabel, int16_t x, int16_t y) {
    int16_t dx[8] = { 1, 1, 0,-1,-1,-1, 0, 1 };
    int16_t dy[8] = { 0, 1, 1, 1, 0,-1,-1,-1 };

    //regiondebug("ATTN: IN DoContourTrace.");


    // ATTN: NEW TEST via paper
    // !!!!! 8/26/22 this seems to fix it(!)
    //int i = external ? 7 : 3;
    int i = external ? 0 : 1;

    int j;

    int16_t x0 = x;
    int16_t y0 = y;

    int16_t xx = -1;
    int16_t yy = -1;

    int done = 0;

    // bug debug
    int loopcount = 0;

    // assign label this point
    labels[CompPointOffset(x0, y0)] = currentLabel;
    updateRegionDataOnForegroundPixel(currentLabel, x0, y0);

    while (!done)
    {
        if (++loopcount > DefMaxLoopsBeforeEmergencyAbort && DefMaxLoopsBeforeEmergencyAbort>0) {
            //regiondebug("Emergency doContourTrace abort regDetectorDoContourTrace due to high loop count.");
            break;
        }

        /* Scan around current pixel in clockwise order. */
        for(j=0; j<8; j++, i=(i+1)&7)
        {
            const int16_t x1 = x0 + dx[i];
            const int16_t y1 = y0 + dy[i];

            if ((x1 < 0) || (x1 >= width)) { continue; }
            if ((y1 < 0) || (y1 >= height)) { continue; }

            const int offset = CompPointOffset(x1, y1);

            // check for foreground or background color?
            if (pixels[offset] == DefRegPixelColorForeground) {
                // assign label to this exterior foreground border pixel
                labels[offset] = currentLabel;
                updateRegionDataOnForegroundPixel(currentLabel, x1, y1);

                if ((xx < 0) && (yy < 0)) {
                    // initial.. why is this being tested in such an inefficient fashion??
                    xx = x1;
                    yy = y1;
                }
                else {
                    /* We are done if we crossed the first 2 contour points again. */
                    done =    ((x == x0) && (xx == x1)) 
                           && ((y == y0) && (yy == y1));
                }
                // update x0,y0
                x0 = x1;
                y0 = y1;
                break;
            } else {
                labels[offset] = DefRegLabelMarked;
            }
        }
        /* Isolated point. */
        if (8 == j) {
            done = 1;
        }
        /* Compute next start position. */
        /* 1. Compute the neighbour index of the previous point. */

        // ATTN: NEW TEST via paper
        int previous = (i+4) % 8;
        //int previous = (i+4) & 7;
        /* 2. Next search index is previous + 2 (mod 8). */
        //i = (previous + 2) & 7;
        i = (previous + 2) % 8;
    }

    //regiondebug("ATTN: OUT doContourTrace.");    
}









void JrRegionDetector::initializeRegionData(DefLabelType labelIndex) {
    if (labelIndex >= DefMaxRegionsStore) {
        return;
    }
    //regiondebug("ATTN: initializing region label %d.", labelIndex);
    JrRegionSummary* region = &regions[labelIndex-1];
    region->pixelCount = 0;
}


void JrRegionDetector::updateRegionDataOnForegroundPixel(DefLabelType labelIndex, int x, int y) {
    if (labelIndex >= DefMaxRegionsStore) {
        return;
    }
    JrRegionSummary* region = &regions[labelIndex-1];
    if (region->pixelCount == 0) {
        // first pixel in region, initialize bounding box
        region->x1 = region->x2 = x;
        region->y1 = region->y2 = y;
        //
        if (labelIndex > foundRegions) {
            foundRegions = labelIndex;
        }
    }
    else {
        // adjust bounding box
        if (x < region->x1) {
            region->x1 = x;
        }
        if (x > region->x2) {
            region->x2 = x;
        }
        if (y < region->y1) {
            region->y1 = y;
        }
        if (y > region->y2) {
            region->y2 = y;
        }
    }
    // increment pixel count
    region->pixelCount++;
}





int JrRegionDetector::doConnectedComponentLabeling() {
	// start with label 1
    foundRegions = 0;
    foundRegionsValid = 0;
    currentLabel = 1;

    initializeRegionData(currentLabel);

    //regiondebug("ATTN: IN doConnectedComponentLabeling.");

	// walk pixels top to bottom, then left to right
	for (int j = 0; j < height; j++)
	{
		for (int i = 0; i < width; i++)
		{
			if (DefRegPixelColorBackground == pixels[CompPointOffset(i, j)]) { continue; }

            const DefPixelType abovePixel = (j > 0) ? pixels[CompPointOffset(i, j - 1)] : DefRegPixelColorBackground;
			const DefPixelType belowPixel = (j < height-1) ? pixels[CompPointOffset(i, j + 1)] : DefRegPixelColorBackground;
            const DefLabelType belowLabel = (j < height-1) ? labels[CompPointOffset(i, j + 1)] : DefRegLabelMarked;

			const DefLabelType label = labels[CompPointOffset(i, j)];

            /* 1. new external countour */
            if ((DefRegLabelUnset == label) && (DefRegPixelColorBackground == abovePixel))
            {
                // main work done here
				// a new region exterior border
                // trace around it and label exterior border
                doContourTrace(1, currentLabel, i, j);
				// advance blob label
                ++currentLabel;
                // initialize new region data
                initializeRegionData(currentLabel);
                if (currentLabel > DefMaxRegionsAbort) {
                    // aborting because there are just too many
                    //regiondebug("ATTN: aborting doConnectedComponentLabeling due to too many regions found.");
                    i = width;
                    j = height;
                    break;
                }
            }
            /* 2. new internal countour */
            else if ((DefRegPixelColorBackground == belowPixel) && (DefRegLabelUnset == belowLabel))
            {
                // sanity text
				DefLabelType workingPointLabel = (label != DefRegLabelUnset) ? label : labels[CompPointOffset(i-1, j)];
                /* add a new internal contour to the corresponding blob. */
                doContourTrace(0, workingPointLabel, i, j);
            }
            /* 3. internal element */
            else if (DefRegLabelUnset == label)
            {
                // label interior element
                const DefLabelType interiorLabel = (i > 0) ? labels[CompPointOffset(i - 1, j)] : DefRegLabelUnset;
                labels[CompPointOffset(i, j)] = interiorLabel;
                if (interiorLabel != DefRegLabelUnset) {
                    updateRegionDataOnForegroundPixel(interiorLabel, i, j);
                }
            }
		}
	}

    //regiondebug("ATTN: OUT doConnectedComponentLabeling.");

    // post process
    postProcessRegionData();

    // return # regions gound
    return foundRegions;
}




void JrRegionDetector::postProcessRegionData() {
    JrRegionSummary* region;
    for (int i = 0; i < foundRegions; ++i) {
        region = &regions[i];
        int rwidth = (region->x2 - region->x1) + 1;
        int rheight = (region->y2 - region->y1) + 1;
        //
        region->label = i;
        region->area = rheight * rwidth;
        region->aspect = rheight < rwidth ? (float)rheight / (float)rwidth : (float)rwidth / (float)rheight;
        if (region->pixelCount == 0) {
            region->density = 0.0;
        } else {
            region->density = (float)region->pixelCount / (float)region->area;
        }
    }
}

