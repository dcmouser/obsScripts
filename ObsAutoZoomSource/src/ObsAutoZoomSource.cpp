// base OBS plugin callbacks, etc.
// extern C wrapped code that can be called by OBS
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
#include <cstdio>
//
// obs helper
#include <util/dstr.h>
//
#include "ObsAutoZoomSource.h"
#include "ObsAutoZoomSourceDefs.h"
#include "obsHelpers.h"
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(DefMyPluginName, "en-US")
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// GLOBAL CHAR* ARRAYS USED IN SETTINGS PROPERTIES (would be better to have these as simple defines but awkward)
char* SETTING_zcMarkerPos_choices[] =			{ "outer", "innner", "center", NULL };
char* SETTING_zcAlignment_choices[] =			{ "topLeft", "topCenter", "topRight", "middleLeft", "center", "middleRight", "bottomLeft", "bottomCenter", "bottomRight", NULL };
char* SETTING_zcMode_choices[] =				{ "zoomCrop", "onlyCrop", "onlyZoom", NULL };
char* SETTING_zcEasing_choices[] = 				{ "instant","linear","eased", NULL };
//---------------------------------------------------------------------------











//---------------------------------------------------------------------------
// CUSTOM REGISTERED OBS CALLBACKS
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
bool OnPropertyKeyTypeChangeCallback(obs_properties_t *props, obs_property_t *p, obs_data_t *settings) {
	UNUSED_PARAMETER(p);
	const char *type = obs_data_get_string(settings, SETTING_COLOR_TYPE);
	bool custom = strcmp(type, "custom") == 0;
	obs_property_set_visible(obs_properties_get(props, SETTING_KEY_COLOR), custom);
	return true;
}


bool OnPropertyNSrcModifiedCallback(obs_properties_t *props, obs_property_t *property, obs_data_t *settings) {
	int n_src = obs_data_get_int(settings, SETTING_srcN);
	for (int i = 0; i < DefMaxSources; i++) {
		char name[16];
		snprintf(name, sizeof(name), "src%d", i);
		obs_property_t *p = obs_properties_get(props, name);
		obs_property_set_enabled(p, i < n_src);
	}
	return true;
}


void onHotkeyCallback(void *data, obs_hotkey_id id, obs_hotkey_t *key, bool pressed) {
	if (!pressed)
		return;

	JrPlugin *plugin = (JrPlugin*) data;
	//
	WaitForSingleObject(plugin->mutex, INFINITE);
	//
	// trigger hotkey
	if (id == plugin->hotkeyId_ToggleAutoUpdate) {
		plugin->opt_enableAutoUpdate = !plugin->opt_enableAutoUpdate;
	} else if (id == plugin->hotkeyId_OneShotZoomCrop) {
		plugin->trackingOneShot = true;
		plugin->hotkeyId_ToggleAutoUpdate = false;
	} else if (id == plugin->hotkeyId_ToggleCropping) {
		if (plugin->opt_zcMode == Def_zcMode_OnlyZoom) {
			plugin->opt_zcMode = Def_zcMode_ZoomCrop;
		} else {
			plugin->opt_zcMode = Def_zcMode_OnlyZoom;
		}
	} else if (id == plugin->hotkeyId_ToggleDebugDisplay) {
		plugin->opt_debugDisplay = !plugin->opt_debugDisplay;
	} else if (id == plugin->hotkeyId_ToggleBypass) {
		plugin->opt_filterBypass = !plugin->opt_filterBypass;
	} else {
		// unknown hotkey
	}
	//
	ReleaseMutex(plugin->mutex);
}
//---------------------------------------------------------------------------













//---------------------------------------------------------------------------
// PLUGIN REGISTERED AND INVOKED FUNCTIONS
//---------------------------------------------------------------------------





//---------------------------------------------------------------------------
obs_properties_t *plugin_properties(void* data)
{
	JrPlugin *plugin = (JrPlugin*) data;
	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;

	obs_property_t* comboString = obs_properties_add_list(props, SETTING_zcMode, TEXT_zcMode, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	plugin->jrAddPropertListChoices(comboString, (const char**)SETTING_zcMode_choices);
	//
	obs_properties_add_bool(props, SETTING_filterBypass, TEXT_filterBypass);
	obs_properties_add_bool(props, SETTING_debugDisplay, TEXT_debugDisplay);
	//
	obs_properties_add_bool(props, SETTING_enableAutoUpdate, TEXT_enableAutoUpdate);
	obs_properties_add_int_slider(props, SETTING_updateRate, TEXT_updateRate, 1, 120, 1);

	comboString = obs_properties_add_list(props, SETTING_zcMarkerPos, TEXT_zcMarkerPos, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	plugin->jrAddPropertListChoices(comboString, (const char**)SETTING_zcMarkerPos_choices);
	//
	comboString = obs_properties_add_list(props, SETTING_zcEasing, TEXT_zcEasing, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	plugin->jrAddPropertListChoices(comboString, (const char**)SETTING_zcEasing_choices);
	//
	comboString = obs_properties_add_list(props, SETTING_zcAlignment, TEXT_zcAlignment, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	plugin->jrAddPropertListChoices(comboString, (const char**)SETTING_zcAlignment_choices);
	obs_properties_add_int_slider(props, SETTING_zcMaxZoom, TEXT_zcMaxZoom, 0, 1000, 1);
	//
	obs_properties_add_int_slider(props, SETTING_zcBoxMargin, TEXT_zcBoxMargin, -100, 200, 1);
	obs_properties_add_int_slider(props, SETTING_zcBoxMoveSpeed, TEXT_zcBoxMoveSpeed, 0, 100, 1);
	obs_properties_add_int_slider(props, SETTING_zcBoxMoveDistance, TEXT_zcBoxMoveDistance, 1, 200, 1);
	obs_properties_add_int_slider(props, SETTING_zcBoxMoveDelay, TEXT_zcBoxMoveDelay, 1, 50, 1);
	//
	obs_properties_add_bool(props, SETTING_zcPreserveAspectRatio, TEXT_zcPreserveAspectRatio);
	//
	obs_properties_add_text(props, SETTING_zcForcedOutputRes, TEXT_zcForcedOutputRes, OBS_TEXT_DEFAULT);

	obs_properties_add_int(props, SETTING_zcMarkerlessSourceIndex, TEXT_zcMarkerlessSourceIndex, 0, DefMaxSources-1,1);
	//
	char labelWithInfo[80];
	sprintf(labelWithInfo, "%s (%d,%d,%d,%d)", TEXT_zcMarkerlessCoords, plugin->dbx1, plugin->dby1, plugin->dbx2 == plugin->workingWidth-1 ? -1 : plugin->dbx2, plugin->dby2 == plugin->workingHeight-1 ? -1 : plugin->dby2);	
	obs_properties_add_text(props, SETTING_zcMarkerlessCoords, labelWithInfo, OBS_TEXT_DEFAULT);
	//	obs_properties_add_button(props, SETTING_zcMarkerlessCoordsButton, TEXT_zcMarkerlessCoordsButton, onMarkerlessCoordsButtonPress);
	//
	p = obs_properties_add_list(props, SETTING_COLOR_TYPE, TEXT_COLOR_TYPE, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, obs_module_text("Green"), "green");
	obs_property_list_add_string(p, obs_module_text("Blue"), "blue");
	obs_property_list_add_string(p, obs_module_text("Magenta"), "magenta");
	obs_property_list_add_string(p, obs_module_text("Custom"), "custom");
	obs_property_set_modified_callback(p, OnPropertyKeyTypeChangeCallback);
	//
	obs_properties_add_color(props, SETTING_KEY_COLOR, TEXT_KEY_COLOR);
	obs_properties_add_int_slider(props, SETTING_SIMILARITY, TEXT_SIMILARITY, 1, 1000, 1);
	obs_properties_add_int_slider(props, SETTING_SMOOTHNESS, TEXT_SMOOTHNESS, 1, 1000, 1);
	//
	obs_properties_add_int_slider(props, SETTING_rmDensityMin, TEXT_rmDensityMin, 1, 100, 1);
	obs_properties_add_int_slider(props, SETTING_rmAspectMin, TEXT_rmAspectMin, 0, 100, 1);
	obs_properties_add_int_slider(props, SETTING_rmSizeMin, TEXT_rmSizeMin, 0, 200, 1);
	obs_properties_add_int_slider(props, SETTING_rmSizetMax, TEXT_rmSizetMax, 0, 400, 1);
	obs_properties_add_int_slider(props, SETTING_rmStageShrink, TEXT_rmStageShrink,1,16,1);

	// multi sources
	p = obs_properties_add_int(props, SETTING_srcN, TEXT_srcN, 1, DefMaxSources, 1);
	obs_property_set_modified_callback(p, OnPropertyNSrcModifiedCallback);
	for (int i = 0; i < DefMaxSources; i++) {
		char name[16];
		snprintf(name, sizeof(name), "src%d", i);
		struct dstr desc = { 0 };
		dstr_printf(&desc, obs_module_text("Source %d"), i);
		plugin->properties_add_source(props, name, desc.array);
		dstr_free(&desc);
	}

	return props;
}



void plugin_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, SETTING_KEY_COLOR, 0x00FF00);
	obs_data_set_default_string(settings, SETTING_COLOR_TYPE, SETTING_Def_COLOR_TYPE);
	obs_data_set_default_int(settings, SETTING_SIMILARITY, 400);
	obs_data_set_default_int(settings, SETTING_SMOOTHNESS, 80);
	//
	obs_data_set_default_bool(settings, SETTING_filterBypass, false);
	obs_data_set_default_bool(settings, SETTING_debugDisplay, true);
	//
	obs_data_set_default_bool(settings, SETTING_enableAutoUpdate, true);
	obs_data_set_default_int(settings, SETTING_updateRate, 5);
	obs_data_set_default_bool(settings, SETTING_zcPreserveAspectRatio, true);
	obs_data_set_default_int(settings, SETTING_zcMarkerPos, 2);
	obs_data_set_default_int(settings, SETTING_zcBoxMargin, SETTING_Def_zcBoxMargin);
	obs_data_set_default_int(settings, SETTING_zcBoxMoveSpeed, SETTING_Def_zcBoxMoveSpeed);
	obs_data_set_default_int(settings, SETTING_zcBoxMoveDistance, SETTING_Def_zcBoxMoveDistance);
	obs_data_set_default_int(settings, SETTING_zcBoxMoveDelay, SETTING_Def_zcBoxMoveDelay);
	//
	obs_data_set_default_string(settings, SETTING_zcEasing, SETTING_Def_zcEasing);
	//
	obs_data_set_default_int(settings, SETTING_rmDensityMin, SETTING_Def_rmDensityMin);
	obs_data_set_default_int(settings, SETTING_rmAspectMin, SETTING_Def_rmAspectMin);
	obs_data_set_default_int(settings, SETTING_rmSizeMin, SETTING_Def_rmSizeMin);
	obs_data_set_default_int(settings, SETTING_rmSizetMax, SETTING_Def_rmSizetMax);
	obs_data_set_default_int(settings, SETTING_rmStageShrink, SETTING_Def_rmStageShrink);
	//
	obs_data_set_default_string(settings, SETTING_zcAlignment, SETTING_Def_zcAlignment);
	obs_data_set_default_string(settings, SETTING_zcMode, SETTING_Def_zcMode);
	obs_data_set_default_int(settings, SETTING_zcMaxZoom, 0);
	//
	obs_data_set_default_string(settings, SETTING_zcForcedOutputRes, SETTING_Def_zcForcedOutputRes);
	obs_data_set_default_int(settings, SETTING_zcMarkerlessSourceIndex, 0);
	obs_data_set_default_string(settings, SETTING_zcMarkerlessCoords, SETTING_Def_zcMarkerlessCoords);
	//
	obs_data_set_default_int(settings, SETTING_srcN, SETTING_Def_srcN);
}
//---------------------------------------------------------------------------





//---------------------------------------------------------------------------
void plugin_update(void *data, obs_data_t *settings) {
	JrPlugin *plugin = (JrPlugin*) data;

	// reload any changed settings vals
	plugin->updateSettingsOnChange(settings);

	// ATTN: we could maybe call HERE the call recreateStagingMemoryIfNeeded RATHER than in tick()
}
//---------------------------------------------------------------------------









//---------------------------------------------------------------------------
void plugin_load(void *data, obs_data_t *settings) {
	JrPlugin* plugin = (JrPlugin*) data;
	plugin->reRegisterHotkeys();
}


void plugin_destroy(void *data)
{
	JrPlugin *plugin = (JrPlugin*) data;

	WaitForSingleObject(plugin->mutex, INFINITE);
	//
	obs_enter_graphics();
	plugin->freeDestroyEffectsAndTexRender();
	plugin->freeDestroyFilterTextures();
	obs_leave_graphics();

	plugin->freeDestroyNonGraphicData();

	ReleaseMutex(plugin->mutex);
	CloseHandle(plugin->mutex);

	// let go of ourselves
	bfree(data);
}



void *plugin_create(obs_data_t *settings, obs_source_t *context)
{
	// CREATE (allocate space for) the new plugin and point to it; this is our plugin class instance that will be used throughout via plugin pointer
	JrPlugin *plugin = (JrPlugin *) bzalloc(sizeof(JrPlugin));

	// store pointer to obs context
	plugin->context = context;

	// init stuff inside graphics
	obs_enter_graphics();
	plugin->initFilterInGraphicsContext();
	obs_leave_graphics();

	if (!plugin->effectChroma || !plugin->effectZoomCrop) {
		obserror("Aborting load due to failure to load effect files.");
		plugin_destroy(plugin);
		return NULL;
	}

	// one time startup init outside graphics context
	plugin->initFilterOutsideGraphicsContext(context);

	// update? im not sure this is ready yet..
	if (true) {
		plugin_update(plugin, settings);
	}

	// return the plugin instance we have created
	return plugin;
}
//---------------------------------------------------------------------------





















//---------------------------------------------------------------------------
void plugin_tick(void *data, float t) {
	JrPlugin *plugin = (JrPlugin*) data;

	// ATTN: there is code in render() that could be placed here..

	// ATTN: rebuild sources from multisource effect plugin code -- needed every tick?
	plugin->checkAndUpdateAllSources();
}
//---------------------------------------------------------------------------












//---------------------------------------------------------------------------
void plugin_render(void* data, gs_effect_t* obsoleteFilterEffect) {
	UNUSED_PARAMETER(obsoleteFilterEffect);
	JrPlugin* plugin = (JrPlugin*) data;

	// get current source -- note this must be released when done
	obs_source_t* source = plugin->getCurrentSource();
	if (!source) {
		//mydebug("ERROR: NULL autoZoom source.");
		return;
	}

	//basic info about source
	uint32_t width = plugin->jrSourceGetWidth(source);
	uint32_t height = plugin->jrSourceGetHeight(source);
	mydebug("in plugin_render 2b with source size: %dx%d.", width, height);
	if (!width || !height) {
		// release source
		plugin->freeSource(source);
		//mydebug("ERROR: autoZoom source is non-NULL but 0 width or height.");
		return;
	}

	// resize to height if it's changed
	plugin->recreateStagingMemoryIfNeeded(source);

	// bypass mode says do NOTHING just render source to output (note this may resize it)
	// you can force this to always be true to bypass most processing for debug testing
	if (false || plugin->opt_filterBypass) {
		// just render out the source untouched
		plugin->doRenderSourceOut(source);
		// release source, all done
		plugin->freeSource(source);
		return;
	}

	// are we doing tracking? and should we update our box location based on some machine vision?
	bool shouldUpdateTrackingBox = false;
	//
	if (plugin->opt_enableAutoUpdate || plugin->trackingOneShot) {
		if (++plugin->trackingUpdateCounter >= plugin->opt_updateRate || plugin->trackingOneShot) {
			// frequence to update tracking box
			plugin->trackingUpdateCounter = 0;
			//
			shouldUpdateTrackingBox = true;
		}
	}

	// ok so there are two cases where we need to call doRenderWorkUpdateTracking
	//  one is when we are updating the machine vision detection of the tracking box; another is when we want to SHOW the current tracking box even if its not being update
	if (shouldUpdateTrackingBox || plugin->opt_debugDisplay) {
		plugin->doRenderWorkUpdateTracking(source, width, height, shouldUpdateTrackingBox);
	}

	// update target zoom box from tracking box -- note this may happen EVEN when we are not doing a tracking box update, to provide smooth movement to target
	plugin->updateZoomCropBoxFromCurrentCandidate();

	// if we are in debugDisplayMode then we just display the debug overlay as our plugin and do not crop
	if (plugin->opt_debugDisplay) {
		// overlay debug info on internatl buffer
		plugin->overlayDebugInfoOnInternalDataBuffer();
		//  then render from internal planning data texture to display for debug purposes
		plugin->doRenderFromInternalMemoryToFilterOutput();
	} else if (plugin->cropBoxReady && !(plugin->dbx1 == 0 && plugin->dby1 == 0 && plugin->dbx2 == plugin->workingWidth-1 && plugin->dby2 == plugin->workingHeight-1 && plugin->outWidth == plugin->workingWidth && plugin->outHeight == plugin->workingHeight)) {
		// we dont do a debug overlay render but instead crop in on the tracking box
		plugin->doRenderAutocropBox(source, width, height);
	}
	else {
		// nothing to do at all
		plugin->doRenderSourceOut(source);
	}

	if (plugin->trackingOneShot) {
		// turn off one-shot
		plugin->trackingOneShot = false;
	}

	// release source
	plugin->freeSource(source);
}
//---------------------------------------------------------------------------















//---------------------------------------------------------------------------
void plugin_enum_sources(void *data, obs_source_enum_proc_t enum_callback, void *param) {
	JrPlugin* plugin = (JrPlugin*) data;

	if (plugin->in_enum)
		return;
	plugin->in_enum = true;

	for (int i = 0; i < DefMaxSources && i < plugin->n_src; i++) {
		obs_source_t *src =plugin-> get_source(i);
		if (!src)
			continue;
		enum_callback(plugin->context, src, param);
		plugin->freeSource(src);
	}

	plugin->in_enum = false;
}
//---------------------------------------------------------------------------







//---------------------------------------------------------------------------
bool plugin_audio_render(void* data, uint64_t* ts_out, struct obs_source_audio_mix* audio_output, uint32_t mixers, size_t channels, size_t sample_rate) {
	// dont want any audio
	// ATTN: does this do anything
	return false;
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
enum gs_color_space plugin_get_color_space(void *data, size_t count,  const enum gs_color_space *preferred_spaces) {
	JrPlugin *plugin = (JrPlugin*) data;

	const enum gs_color_space potential_spaces[] = {
		GS_CS_SRGB,
		GS_CS_SRGB_16F,
		GS_CS_709_EXTENDED,
	};

	// get current source (be sure to free it before we leave)
	obs_source_t* source = plugin->getCurrentSource();
	if (!source) {
		//mydebug("ERROR: NULL source in plugin_get_color_space.");
		return GS_CS_SRGB;
	}
	const enum gs_color_space source_space = obs_source_get_color_space(source,	OBS_COUNTOF(potential_spaces), potential_spaces);

	plugin->freeSource(source);
	return source_space;
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
uint32_t plugin_width(void *data)
{
	JrPlugin *plugin = (JrPlugin*) data;
	if (plugin->opt_filterBypass) {
		// when bypassing, pass through original dimension and dont obey any forced output size
		return (uint32_t)plugin->workingWidth;
	}
	return (uint32_t)plugin->outWidth;
}

uint32_t plugin_height(void *data)
{
	JrPlugin *plugin = (JrPlugin*) data;
	if (plugin->opt_filterBypass) {
		// when bypassing, pass through original dimension and dont obey any forced output size
		return (uint32_t)plugin->workingHeight;
	}
	return (uint32_t)plugin->outHeight;
}
//---------------------------------------------------------------------------


















//---------------------------------------------------------------------------
// for registering the plugin with OBS

const char *plugin_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("ObsAutoZoomSrc");
}


// global plugin for obs
struct obs_source_info autoZoomSourcePlugin;
//
bool obs_module_load(void)
{
	// set params
	autoZoomSourcePlugin.id = DefMyPluginName;
	autoZoomSourcePlugin.version = 2;
	autoZoomSourcePlugin.type = OBS_SOURCE_TYPE_INPUT;
	// note that the OBS_SOURCE_COMPOSITE flag below is supposed to be used by sources that composite multiple children, which we seem to do -- not sure its needed though
	autoZoomSourcePlugin.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_COMPOSITE;
	autoZoomSourcePlugin.get_name = plugin_name;
	autoZoomSourcePlugin.create = plugin_create;
	autoZoomSourcePlugin.destroy = plugin_destroy;
	autoZoomSourcePlugin.load = plugin_load;
	autoZoomSourcePlugin.video_tick = plugin_tick;
	autoZoomSourcePlugin.video_render = plugin_render;
	autoZoomSourcePlugin.update = plugin_update;
	autoZoomSourcePlugin.get_properties = plugin_properties;
	autoZoomSourcePlugin.get_defaults = plugin_defaults;
	autoZoomSourcePlugin.video_get_color_space = plugin_get_color_space;
	autoZoomSourcePlugin.get_width = plugin_width;
	autoZoomSourcePlugin.get_height = plugin_height;
	autoZoomSourcePlugin.enum_active_sources = plugin_enum_sources;
	autoZoomSourcePlugin.audio_render = plugin_audio_render;

	// register it with obs
	obs_register_source(&autoZoomSourcePlugin);
	//
	return true;
}
//---------------------------------------------------------------------------






//---------------------------------------------------------------------------
#ifdef __cplusplus
} // extern "C"
#endif
//---------------------------------------------------------------------------
