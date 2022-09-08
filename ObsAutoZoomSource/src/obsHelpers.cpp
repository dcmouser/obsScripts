#include "obsHelpers.h"
#include "myPluginDefs.h"





//---------------------------------------------------------------------------
int jrSourceCalculateWidth(obs_source_t* src) {
	//return obs_source_get_base_width(src);
	return obs_source_get_width(src);
}

int jrSourceCalculateHeight(obs_source_t* src) {
	//return obs_source_get_base_height(src);
	return obs_source_get_height(src);
}
//---------------------------------------------------------------------------








//---------------------------------------------------------------------------
// this took me about 7 hours of pure depressive hell to figure out
//
void jrRenderSourceIntoTexture(obs_source_t* source, gs_texrender_t *tex, uint32_t stageWidth, uint32_t stageHeight, uint32_t sourceWidth, uint32_t sourceHeight) {
	// render source onto a texture

	//mydebug("myRenderSourceIntoTexture stage=(%d,%d) source=(%d,%d).", stageWidth, stageHeight, sourceWidth, sourceHeight);

	// seems critical wtf
	if (tex) {
		gs_texrender_reset(tex);
	}
	if (tex && !gs_texrender_begin(tex, stageWidth, stageHeight)) {
		mydebug("ERROR -------------> failed in jrRenderSourceIntoTexture to gs_texrender_begin.");
		return;
	}

	// save blend state
	gs_blend_state_push();

	if (DefDrawingUpdateBugfix1) {
		gs_reset_blend_state();
		//gs_reset_viewport();
	}

	//mydebug("jrRenderSourceIntoTexture performing...");

	// blend mode
	//gs_blend_function_separate(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA, GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	//bool custom_draw = (OBS_SOURCE_CUSTOM_DRAW) != 0;
	//bool async = (0 & OBS_SOURCE_ASYNC) != 0;

	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	struct vec4 clear_color;
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
	gs_ortho(0.0f, (float)sourceWidth, 0.0f, (float)sourceHeight, -100.0f, 100.0f);
	// RENDER THE SOURCE
	obs_source_video_render(source);

	// test
	if (DefDrawingUpdateBugfix2) {
		gs_present();
	}
	//
	// end stuff
	gs_blend_state_pop();
	if (tex) {
		gs_texrender_end(tex);
	}

}


void jrRenderEffectIntoTexture(gs_texrender_t *tex, gs_effect_t* effect, gs_texrender_t *inputTex, uint32_t stageWidth, uint32_t stageHeight) {
	// render effect onto texture using an input texture (set effect params before invoking)

	if (tex) {
		gs_texrender_reset(tex);
	}
	if (tex && !gs_texrender_begin(tex, stageWidth, stageHeight)) {
		mydebug("ERROR ----> failure in jrRenderEffectIntoTexture to gs_texrender_begin.");
		return;
	}

	// save blend state
	gs_blend_state_push();

	if (DefDrawingUpdateBugfix1) {
		gs_reset_blend_state();
		//gs_reset_viewport();
	}

	// blend mode
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	// tell chroma effect to use the rendered image as its input
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	if (!image) {
		mydebug("ERROR jrRenderEffectIntoTexture null image!!");
	}
	gs_texture_t *obsInputTex = gs_texrender_get_texture(inputTex);
	if (!obsInputTex) {
		mydebug("ERROR jrRenderEffectIntoTexture null obsInputTex!!");
	}
	gs_effect_set_texture(image, obsInputTex);

	struct vec4 clear_color;
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
	//
	// ATTN: this seems to be important for plugin property dialog preview
	gs_ortho(0.0f, (float)stageWidth, 0.0f, (float)stageHeight, -100.0f, 100.0f);

	int loopcount = 0;
	while (gs_effect_loop(effect, "Draw")) {
		gs_draw_sprite(NULL, 0, stageWidth, stageHeight);
		++loopcount;
	}

	// test
	if (DefDrawingUpdateBugfix2) {
		gs_present();
	}

	// end stuff
	gs_blend_state_pop();
	//
	if (tex) {
		gs_texrender_end(tex);
	}

	//mydebug("Finishing jrRenderEffectIntoTexture with loopcount = %d.", loopcount);
}











void jrRenderEffectIntoTextureFade(gs_texrender_t *tex, gs_effect_t* effect, gs_texrender_t *texa, gs_texrender_t *texb, float fade_val, uint32_t stageWidth, uint32_t stageHeight) {
	// render effect onto texture using an input texture (set effect params before invoking)

	if (tex) {
		gs_texrender_reset(tex);
	}
	if (tex && !gs_texrender_begin(tex, stageWidth, stageHeight)) {
		mydebug("ERROR ----> failure in jrRenderEffectIntoTexture to gs_texrender_begin.");
		return;
	}

	// save blend state
	gs_blend_state_push();

	if (DefDrawingUpdateBugfix1) {
		gs_reset_blend_state();
		//gs_reset_viewport();
	}

	// blend mode
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	gs_eparam_t* a_param = gs_effect_get_param_by_name(effect, "tex_a");
	gs_eparam_t* b_param = gs_effect_get_param_by_name(effect, "tex_b");
	gs_eparam_t* fade_param = gs_effect_get_param_by_name(effect, "fade_val");


	gs_texture_t *texaInput = gs_texrender_get_texture(texa);
	gs_texture_t *texbInput = gs_texrender_get_texture(texb);
	if (!texaInput) {
		mydebug("ERROR texaInput null");
	}
	if (!texbInput) {
		mydebug("ERROR texbInput null");
	}

	gs_effect_set_texture(a_param, texaInput);
	gs_effect_set_texture(b_param, texbInput);
	gs_effect_set_float(fade_param, fade_val);

	struct vec4 clear_color;
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
	//
	// ATTN: this seems to be important for plugin property dialog preview
	gs_ortho(0.0f, (float)stageWidth, 0.0f, (float)stageHeight, -100.0f, 100.0f);

	char *tech_name = "FadeLinear";
	int loopcount = 0;
	while (gs_effect_loop(effect,tech_name)) {
		gs_draw_sprite(NULL, 0, stageWidth, stageHeight);
		++loopcount;
	}

	// test
	if (DefDrawingUpdateBugfix2) {
		gs_present();
	}

	// end stuff
	gs_blend_state_pop();
	//
	if (tex) {
		gs_texrender_end(tex);
	}

	//mydebug("Finishing jrRenderEffectIntoTexture with loopcount = %d.", loopcount);
}











// see source_render
void jrRenderSourceOut(obs_source_t* source, uint32_t rwidth, uint32_t rheight) {
	// attempts to make it size properly in properties
	//mydebug("Rendering out source of size %d x %d.", rwidth, rheight);
	if (rwidth == 0 || rheight == 0) {
		//mydebug("in doRenderSourceOut aborting because of dimensions 0.");
		return;
	}

	// render it out
	jrRenderSourceIntoTexture(source, NULL, rwidth, rheight, rwidth, rheight);
}
//---------------------------------------------------------------------------












//---------------------------------------------------------------------------


void jrRenderSourceOutAtSizeLoc(obs_source_t* source, uint32_t rwidth, uint32_t rheight, int x1, int y1, int outwidth, int outheight) {
	if (rwidth == 0 || rheight == 0) {
		return;
	}
	// render it out
	jrRenderSourceIntoTextureAtSizeLoc(source, NULL, rwidth, rheight, rwidth, rheight, x1,y1,outwidth,outheight);
}

void jrRenderSourceIntoTextureAtSizeLoc(obs_source_t* source, gs_texrender_t *tex, uint32_t stageWidth, uint32_t stageHeight, uint32_t sourceWidth, uint32_t sourceHeight, int x1, int y1, int outWidth, int outHeight) {
	// render source onto a texture

	//mydebug("myRenderSourceIntoTexture stage=(%d,%d) source=(%d,%d).", stageWidth, stageHeight, sourceWidth, sourceHeight);

	// seems critical wtf
	if (tex) {
		gs_texrender_reset(tex);
	}
	if (tex && !gs_texrender_begin(tex, stageWidth, stageHeight)) {
		mydebug("ERROR -------------> failed in jrRenderSourceIntoTexture to gs_texrender_begin.");
		return;
	}

	// save blend state
	gs_blend_state_push();

	//mydebug("jrRenderSourceIntoTexture performing...");

	// blend mode
	//gs_blend_function_separate(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA, GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	//bool custom_draw = (OBS_SOURCE_CUSTOM_DRAW) != 0;
	//bool async = (0 & OBS_SOURCE_ASYNC) != 0;

	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	struct vec4 clear_color;
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
	gs_ortho(0.0f, (float)sourceWidth, 0.0f, (float)sourceHeight, -100.0f, 100.0f);

	gs_matrix_scale3f((float)outWidth/(float)sourceWidth, (float)outHeight/(float)sourceHeight,0);
	//gs_resize(outWidth, outHeight);

	//offset in so we can debug multiple sources on same screen
	gs_matrix_translate3f(x1, y1, 0.0f);


	// RENDER THE SOURCE
	obs_source_video_render(source);

	// test
	//
	// end stuff
	gs_blend_state_pop();
	if (tex) {
		gs_texrender_end(tex);
	}
}
//---------------------------------------------------------------------------









//---------------------------------------------------------------------------
// for debugging
void jrRgbaDrawPixel(uint32_t *textureData, int ilinesize, int x, int y, int pixelVal) {
	textureData[y * (ilinesize/4) + x] = pixelVal;
}

void jrRgbaDrawRectangle(uint32_t *textureData, int ilinesize, int x1, int y1, int x2, int y2, int pixelVal) {
	for (int iy=y1; iy<=y2; ++iy) {
		jrRgbaDrawPixel(textureData, ilinesize, x1, iy, pixelVal);
		jrRgbaDrawPixel(textureData, ilinesize, x2, iy, pixelVal);
	}
	for (int ix=x1; ix<=x2; ++ix) {
		jrRgbaDrawPixel(textureData, ilinesize, ix, y1, pixelVal);
		jrRgbaDrawPixel(textureData, ilinesize, ix, y2, pixelVal);
	}
}
//---------------------------------------------------------------------------