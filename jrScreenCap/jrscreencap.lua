-- see https://github.com/bfxdev/OBS/blob/master/obslua.lua


-- version 1.3
obs                        = obslua
capture_hotkey_id          = obs.OBS_INVALID_HOTKEY_ID
saveDirectory              = ""

----------------------------------------------------------

function triggerCaptureHotkey(pressed)
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
  local current_scene_name = obs.obs_source_get_name(current_scene)

  -- code here
  -- go to Tools -> Scripts -> Script Log to see
	print("Hello world from JrScreenCap Lua.  Trying to capture scene: " .. current_scene_name)
	doCaptureScene(current_scene)


  obs.obs_source_release(current_scene)
  obs.source_list_release(scenes)
end



function doCaptureScene(scene)
  -- get frame data for source
  local f

	--local sceneSource = obs.obs_scene_get_source(scene)
	--print("Local scene source name = " .. obs.obs_source_get_name(sceneSource))
  
  f = obs.obs_source_get_frame(scene)
 
  if (f == nil) then
    print("F IS NIL.")
    -- test
--    source = obs.obs_get_source_by_name("VideoCap0")
--    f = obs.obs_source_get_frame(source)
  end

		local w = obs.obs_source_get_width(scene)
		print("scene width = ")
		print (w)

		local t = obs.obs_source_get_type(scene)
		print("scene type = ")
		print (t)
		
		local frame
		obs.obs_source_output_video(scene, frame)
		print("FRAME:")
		print(frame)
		
		-- what about obs_add_raw_video_callback
		-- and obs_add_main_render_callback

  --print("Width: ")
  --print(f.width)
  
end





-- see https://github.com/bfxdev/OBS/blob/master/obslua.lua

--function mainRenderCallback(param, cx, cy)
function mainRenderCallback()
	--print("In mainRenderCallback")
	 local source
	 --source = obs.obs_get_source_by_name("VideoCap0")
	 --source = obs.obs_get_source_by_name("Front Cam")
	 source = obs.obs_frontend_get_current_scene()
   --print("Local scene source name = " .. obs.obs_source_get_name(source))
   
   local video = obslua.obs_get_video()
   if (video == nil) then
     print("video IS NIL.")
   else
     --print("video is not nil.")
   end
   
   
--   print("CX = ")
--   print(cx)
--   print("PARAM=")
--   print(param)

--   f = obs.obs_source_get_frame(source)
--  if (f == nil) then
--    print("F IS NIL.")
--  else
--    print("F is not nil.")
--  end
end


function rawVideoCallback(param, frame)
	print("in rawVideoCallback")
	return nil
end








--- Loaded on startup
function script_load(settings)
  print("Loading jrScreenCap script")
  --- Register hotkeys
  capture_hotkey_id = obs.obs_hotkey_register_frontend("triggerCapture.trigger", "JrScreenCap Capture", triggerCaptureHotkey)
  local capture_hotkey_save_array = obs.obs_data_get_array(settings, "triggerCapture.trigger")
  obs.obs_hotkey_load(capture_hotkey_id, capture_hotkey_save_array)
  obs.obs_data_array_release(capture_hotkey_save_array)
  
  
  
  obslua.obs_add_main_render_callback(mainRenderCallback)
  --obslua.obs_add_raw_video_callback(nil, rawVideoCallback , nil)
  
  
end

function script_save(settings)
  local capture_hotkey_save_array = obs.obs_hotkey_save(capture_hotkey_id)
  obs.obs_data_set_array(settings, "triggerCapture.trigger", capture_hotkey_save_array)
  obs.obs_data_array_release(capture_hotkey_save_array)
end

-- A function named script_properties defines the properties that the user
-- can change for the entire script module itself
function script_properties()
	local props = obs.obs_properties_create()
	obs.obs_properties_add_text(props, "saveDirectory", "Save Directory", obs.OBS_TEXT_DEFAULT)
	return props
end

-- A function named script_update will be called when settings are changed
function script_update(settings)
	saveDirectory = obs.obs_data_get_string(settings, "saveDirectory")
end

-- A function named script_defaults will be called to set the default settings
function script_defaults(settings)
	obs.obs_data_set_default_string(settings, "saveDirectory", "C:\\")
end

-- A function named script_description returns the description shown to
-- the user
function script_description()
  return "JrScreenCap script triggers on a hotkey and saves a screenshot of the current source to the configured directory at its full native resolution."
end