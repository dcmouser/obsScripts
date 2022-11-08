-- version 1.3 by mouser@donationcoder.com

-- MIT License
--
-- Copyright (c) Geert Eikelboom, Mark Lagendijk
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
-- SOFTWARE.

-- based on https://github.com/marklagendijk/obs-scene-execute-command-script/blob/master/scene_execute_command.lua

-- for example mute system volume: C:\ProgramFiles\nircmd-x64\nircmd.exe mutesysvolume 1



obs = obslua
settings = {}

-- Script hook for defining the script description
function script_description()
	return [[
Mute system volume when streaming or recording starts
]]
end

-- Script hook for defining the settings that can be configured for the script
function script_properties()
	local props = obs.obs_properties_create()

	obs.obs_properties_add_text(props, "command", "Command", obs.OBS_TEXT_DEFAULT)
	
	return props
end

-- Script hook that is called whenver the script settings change
function script_update(_settings)	
	settings = _settings
end

-- Script hook that is called when the script is loaded
function script_load(settings)
	obs.obs_frontend_add_event_callback(handle_event)

  -- my custom event adding to obs -- requires custom built obs
  --obs.script_log(obs.LOG_INFO, "jrOnSreamStart loaded.")
	if (obs.OBS_FRONTEND_EVENT_BROADCAST_STARTED == nil) then
		obs.script_log(obs.LOG_ERROR, "jrOnStreamStart - the obs.OBS_FRONTEND_EVENT_BROADCAST_STARTED event is not defined, will not trigger auto restart of music and hotkey on broadcast start.")
	end
	if (obs.OBS_FRONTEND_EVENT_BROADCAST_STARTING == nil) then
		obs.script_log(obs.LOG_ERROR, "jrOnStreamStart - the obs.OBS_FRONTEND_EVENT_BROADCAST_STARTING event is not defined, will not trigger auto restart of music and hotkey on broadcast start.")
	end

end

function handle_event(event)

	if event == obs.OBS_FRONTEND_EVENT_RECORDING_STARTED then
		schedule_trigger_command()	
	end
	if event == obs.OBS_FRONTEND_EVENT_STREAMING_STARTED then
		schedule_trigger_command()	
	end


	if (event == obs.OBS_FRONTEND_EVENT_BROADCAST_STARTED and obs.OBS_FRONTEND_EVENT_BROADCAST_STARTED ~= nil) then
		schedule_trigger_broadcastStarted()	
	end
	if (event == obs.OBS_FRONTEND_EVENT_BROADCAST_STARTING and obs.OBS_FRONTEND_EVENT_BROADCAST_STARTING ~= nil) then
	  -- should we try calling directly to avoid delay? doesnt help enough to be worth it
		-- handle_trigger_broadcastStarting()
		schedule_trigger_broadcastStarting()	
	end

end


function schedule_trigger_command()
		obs.timer_remove(handle_trigger_command)
		obs.timer_add(handle_trigger_command, 1000)
end


function schedule_trigger_broadcastStarted()
		obs.timer_remove(handle_trigger_broadcastStarted)
		obs.timer_add(handle_trigger_broadcastStarted, 100)
end

function schedule_trigger_broadcastStarting()
		obs.timer_remove(handle_trigger_broadcastStarting)
		obs.timer_add(handle_trigger_broadcastStarting, 100)
end



function handle_trigger_command()
		obs.remove_current_callback()

		-- reset media position for main music
    doRestartMusic()

		-- run local tool command
		local command = obs.obs_data_get_string(settings, "command")
		if (command == "") then
			return
		end
		obs.script_log(obs.LOG_INFO, "jrOnStreamStart - Triggering command at start of streaming/recording: " .. command)
		-- command = 'start "runonstart" ' .. '"' .. command .. '"'
		os.execute(command)

end



function handle_trigger_broadcastStarted()
		obs.remove_current_callback()

    obs.script_log(obs.LOG_INFO, "BROADCAST STARTED in JrOnStreamStart script..")
		-- we now do all our work in broadcastStarting because it happens FIRST before start; this event gets called a little late after streaming has been happening for a few seconds
    
end

function handle_trigger_broadcastStarting()
		obs.remove_current_callback()

    doRestartMusic()

    obs.script_log(obs.LOG_INFO, "BROADCAST STARTING in JrOnStreamStart script..")

		-- trigger hotkey for infowriter event since we dont know how to trigger it manually
    -- send_hotkey("OBS_KEY_I",{shift=true,control=true})
end





function doRestartMusic()
    -- restart music
    doRestartMediaSource("CftMusic")
    doRestartMediaSource("CftMusicCreepy")
end


function doRestartMediaSource(mediaSourceName)
		local source = obs.obs_get_source_by_name(mediaSourceName)
		--
		-- see https://fossies.org/linux/obs-studio/UI/media-controls.cpp
		-- see https://obsproject.com/forum/threads/python-pause-the-media-playback-of-a-source.139144/
		-- https://github.com/exeldro/obs-lua/blob/master/media-cue.lua
		obs.obs_source_media_restart(source)
		--		
		obs.obs_source_release(source)
end




-- see https://obsproject.com/forum/threads/tips-and-tricks-for-lua-scripts.132256/
function send_hotkey(hotkey_id_name,key_modifiers)
  shift = key_modifiers.shift or false
  control = key_modifiers.control or false
  alt = key_modifiers.alt or false
  command = key_modifiers.command or false
  modifiers = 0

  if shift then modifiers = bit.bor(modifiers,obs.INTERACT_SHIFT_KEY ) end
  if control then modifiers = bit.bor(modifiers,obs.INTERACT_CONTROL_KEY ) end
  if alt then modifiers = bit.bor(modifiers,obs.INTERACT_ALT_KEY ) end
  if command then modifiers = bit.bor(modifiers,obs.INTERACT_COMMAND_KEY ) end

  combo = obs.obs_key_combination()
  combo.modifiers = modifiers
  combo.key = obs.obs_key_from_name(hotkey_id_name)

  if not modifiers and
    (combo.key == obs.OBS_KEY_NONE or combo.key >= obs.OBS_KEY_LAST_VALUE) then
    return error('invalid key-modifier combination')
  end

  obs.obs_hotkey_inject_event(combo,false)
  obs.obs_hotkey_inject_event(combo,true)
  obs.obs_hotkey_inject_event(combo,false)
end