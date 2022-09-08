#pragma once

//---------------------------------------------------------------------------
#include <obs-module.h>
//
#include "pluginInfo.h"
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
#define do_log(level, format, ...) blog(level, "[" ## DefMyPluginLabel ## "] " format, ##__VA_ARGS__)
//
#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)
#define obserror(format, ...) do_log(LOG_ERROR, format, ##__VA_ARGS__)
//
#define mydebug(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
//#define mydebug(format, ...) 
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
void jrRgbaDrawPixel(uint32_t* textureData, int ilinesize, int x, int y, int pixelVal);
void jrRgbaDrawRectangle(uint32_t* textureData, int ilinesize, int x1, int y1, int x2, int y2, int pixelVal);
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
extern int jrSourceCalculateWidth(obs_source_t* src);
extern int jrSourceCalculateHeight(obs_source_t* src);
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
void jrRenderSourceIntoTexture(obs_source_t* source, gs_texrender_t* tex, uint32_t stageWidth, uint32_t stageHeight, uint32_t sourceWidth, uint32_t sourceHeight);
void jrRenderEffectIntoTexture(gs_texrender_t* tex, gs_effect_t* effect, gs_texrender_t* inputTex, uint32_t stageWidth, uint32_t stageHeight);
void jrRenderSourceOut(obs_source_t* source, uint32_t rwidth, uint32_t rheight);
//
void jrRenderSourceOutAtSizeLoc(obs_source_t* source, uint32_t rwidth, uint32_t rheight, int x1, int y1, int outWidth, int outHeight);
void jrRenderSourceIntoTextureAtSizeLoc(obs_source_t* source, gs_texrender_t* tex, uint32_t stageWidth, uint32_t stageHeight, uint32_t sourceWidth, uint32_t sourceHeight, int x1, int y1, int outWidth, int outHeight);
//
void jrRenderEffectIntoTextureFade(gs_texrender_t* tex, gs_effect_t* effect, gs_texrender_t* texa, gs_texrender_t* texb, float fade_val, uint32_t stageWidth, uint32_t stageHeight);
//---------------------------------------------------------------------------

