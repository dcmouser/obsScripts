-- see https://github.com/bfxdev/OBS/blob/master/obslua.lua


-- version 1.3
obs                = obslua
hotkey_id          = obs.OBS_INVALID_HOTKEY_ID
saveDirectory      = ""

----------------------------------------------------------

function triggerHotkey(pressed)
  if not pressed then
    return
  end
  local previewMode = false
  if preview and obs.obs_frontend_preview_program_mode_active() then
    previewMode = true
  end
  local scenes = obs.obs_frontend_get_scenes()
  local current_scene = nil
  local scene_function = nil
  if previewMode then
    current_scene = obs.obs_frontend_get_current_preview_scene()
    scene_function = obs.obs_frontend_set_current_preview_scene
  else
    current_scene = obs.obs_frontend_get_current_scene()
    scene_function = obs.obs_frontend_set_current_scene
  end


  -- code here
  -- go to Tools -> Scripts -> Script Log to see

	doResetAudioMonitorInScene(current_scene)

  obs.obs_source_release(current_scene)
  obs.source_list_release(scenes)
end







function doResetAudioMonitorInScene(sceneSource)
	-- see https://obsproject.com/docs/reference-sources.html?highlight=monitor#c.obs_source_get_monitoring_type

  local scene_name = obs.obs_source_get_name(sceneSource)
	--print("jrFixAudioMonitorsInScene checking scene: " .. scene_name)

  local scene = obs.obs_scene_from_source(sceneSource)
  local scene_items = obs.obs_scene_enum_items(scene)
  if scene_items ~= nil then
		for _, scene_item in ipairs(scene_items) do
			local source = obs.obs_sceneitem_get_source(scene_item)
			doResetAudioMonitorForSource(source)
		end
	end

	obs.sceneitem_list_release(scene_items)
end






function doResetAudioMonitorForSource(source)
	local name = obs.obs_source_get_name(source)
	-- print("jrFixAudioMonitorsInScene Lua checking source: " .. name)

	-- if this source in a nested scene, we need to call doResetAudioMonitorInScene in it
	local sourceType = obs.obs_source_get_type(source)
	if (sourceType == obs.OBS_SOURCE_TYPE_SCENE) then
		doResetAudioMonitorInScene(source)
		return
	end

	local curMonitoringType = obs.obs_source_get_monitoring_type(source)
	if (curMonitoringType==obs.OBS_MONITORING_TYPE_NONE) then
		return
	end
	
	print("jrFixAudioMonitorsInScene Lua reseting audio for monitored source: " .. name)
	-- set to none
	obs.obs_source_set_monitoring_type(source, obs.OBS_MONITORING_TYPE_NONE);
	-- then set back
	obs.obs_source_set_monitoring_type(source, curMonitoringType);	
end



















--- Loaded on startup
function script_load(settings)
  print("Loading jrScreenCap script")
  --- Register hotkeys
  hotkey_id = obs.obs_hotkey_register_frontend("jrFixAudioMonitorsInScene.trigger", "Fix audio monitors in scene", triggerHotkey)
  local hotkey_save_array = obs.obs_data_get_array(settings, "jrFixAudioMonitorsInScene.trigger")
  obs.obs_hotkey_load(hotkey_id, hotkey_save_array)
  obs.obs_data_array_release(hotkey_save_array)
end

function script_save(settings)
  local hotkey_save_array = obs.obs_hotkey_save(hotkey_id)
  obs.obs_data_set_array(settings, "jrFixAudioMonitorsInScene.trigger", hotkey_save_array)
  obs.obs_data_array_release(hotkey_save_array)
end

-- A function named script_properties defines the properties that the user
-- can change for the entire script module itself
function script_properties()
	local props = obs.obs_properties_create()
	-- obs.obs_properties_add_text(props, "saveDirectory", "Save Directory", obs.OBS_TEXT_DEFAULT)
	return props
end

-- A function named script_update will be called when settings are changed
function script_update(settings)
	-- saveDirectory = obs.obs_data_get_string(settings, "saveDirectory")
end

-- A function named script_defaults will be called to set the default settings
function script_defaults(settings)
	-- obs.obs_data_set_default_string(settings, "saveDirectory", "C:\\")
end

-- A function named script_description returns the description shown to
-- the user
function script_description()
  return "jrFixAudioMonitorsInScene script triggers on a hotkey and foreces a reset of the audio for any sources in the current scene whose audio is set to monitor only or monitor and output."
end