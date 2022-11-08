obs = obslua
hotkey_id = obs.OBS_INVALID_HOTKEY_ID

target_scene = ""

function refresh_browsers_trigger(pressed)
	if not pressed then
		return
    end

    refresh_scene_browsers()
end

function refresh_scene_browsers()
    local scene_source = obs.obs_get_source_by_name(target_scene)
    local scene = obs.obs_scene_from_source(scene_source)
    obs.obs_source_release(scene_source)
    local items = obs.obs_scene_enum_items(scene)

    if items ~= nil then
        for _, item in ipairs(items) do
            local source = obs.obs_sceneitem_get_source(item)
            local source_id = obs.obs_source_get_unversioned_id(source)
            if source_id == "browser_source" then
                local settings = obs.obs_source_get_settings(source)
                local fps = obs.obs_data_get_int(settings, "fps")
                if fps % 2 == 0 then
                    obs.obs_data_set_int(settings,"fps",fps + 1)
                else
                    obs.obs_data_set_int(settings,"fps",fps - 1)
                end
                obs.obs_source_update(source, settings)
                obs.obs_data_release(settings)
            end
        end
    end
    obs.source_list_release(sources)
end

function script_description()
	return "Adds hotkey to refesh all browsers"
end

function script_update(settings)
	target_scene = obs.obs_data_get_string(settings, "target_scene")

end

function script_load(settings)
    hotkey_id = obs.obs_hotkey_register_frontend("refresh_browsers_trigger","Refresh all browsers", refresh_browsers_trigger)
    local hotkey_save_array = obs.obs_data_get_array(settings, "refresh_browsers_trigger")
	obs.obs_hotkey_load(hotkey_id, hotkey_save_array)
	obs.obs_data_array_release(hotkey_save_array)
end

function script_properties()
	local props = obs.obs_properties_create()

    local target_scene = obs.obs_properties_add_list(props, "target_scene", "Target Scene", obs.OBS_COMBO_TYPE_EDITABLE, obs.OBS_COMBO_FORMAT_STRING)
    local scenes = obs.obs_frontend_get_scene_names()
    if scenes ~= nil then
        for _, scene in ipairs(scenes) do
            obs.obs_property_list_add_string(target_scene, scene, scene)
        end
        obs.bfree(scene)
    end

    obs.obs_properties_add_button(props,"refresh_browsers","Refresh Browsers",refresh_browsers_trigger)
	return props
end

function script_save(settings)
	local hotkey_save_array = obs.obs_hotkey_save(hotkey_id)
	obs.obs_data_set_array(settings, "refresh_browsers_trigger", hotkey_save_array)
	obs.obs_data_array_release(hotkey_save_array)
end