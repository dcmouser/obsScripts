-- Modified by mouser@donationcoder.com
-- the original advanced-timer.lua was a huge cpu hog and impacted fps and average time to render and frames missed
-- see https://obsproject.com/forum/threads/advanced-timer.81539/page-5
-- NOTE: we do NOT support global timers with this new mod


obs           = obslua
source_name   = ""

stop_text     = ""
mode          = ""
a_mode        = ""
format        = ""
activated     = false
global        = false
timer_active  = false
settings_     = nil
orig_time     = 0
cur_time      = 0
cur_ns        = 0
up_when_finished = false
up            = false
paused        = false



lastText = ""
timerMsDelay = 250
globalAppIsExiting = false



hotkey_id_reset     = obs.OBS_INVALID_HOTKEY_ID
hotkey_id_pause     = obs.OBS_INVALID_HOTKEY_ID

function delta_time(year, month, day, hour, minute, second)
	local now = os.time()

	if (year == -1) then
		year = os.date("%Y", now)
	end

	if (month == -1) then
		month = os.date("%m", now)
	end

	if (day == -1) then
		day = os.date("%d", now)
	end

	local future = os.time{year=year, month=month, day=day, hour=hour, min=minute, sec=second}
	local seconds = os.difftime(future, now)

	if (seconds < 0) then
		-- ATTN: to stop weird behavior when time already passed
		-- seconds = seconds + 84600
		seconds = 0;
	end

	return seconds * 1000000000
end

function set_time_text(ns, text)
	local ms = math.floor(ns / 1000000)
	
	local flagRemoveLeading0s = true

	if string.match(text, "%%0H") then
		local hours_infinite = math.floor(ms / 3600000)
		minutes_infinite = string.format("%02d", hours_infinite)
		text = string.gsub(text, "%%0H", hours_infinite)
	end

	if string.match(text, "%%0M") then
		local minutes_infinite = math.floor(ms / 60000)
		minutes_infinite = string.format("%02d", minutes_infinite)
		text = string.gsub(text, "%%0M", minutes_infinite)
	end

	if string.match(text, "%%0S") then
		local seconds_infinite = math.floor(ms / 1000)
		seconds_infinite = string.format("%02d", seconds_infinite)
		text = string.gsub(text, "%%0S", seconds_infinite)
	end

	if string.match(text, "%%0h") then
		local hours = math.floor(ms / 3600000) % 24
		hours = string.format("%02d", hours)
		text = string.gsub(text, "%%0h", hours)
	end

	if string.match(text, "%%0m") then
		local minutes = math.floor(ms / 60000) % 60
		minutes = string.format("%02d", minutes)
		text = string.gsub(text, "%%0m", minutes)
	end

	if string.match(text, "%%0s") then
		local seconds = math.floor(ms / 1000) % 60
		seconds = string.format("%02d", seconds)
		text = string.gsub(text, "%%0s", seconds)
	end

	if string.match(text, "%%H") then
		local hours_infinite = math.floor(ms / 3600000)
		text = string.gsub(text, "%%HH", hours_infinite)
	end

	if string.match(text, "%%M") then
		local minutes_infinite = math.floor(ms / 60000)
		text = string.gsub(text, "%%MM", minutes_infinite)
	end

	if string.match(text, "%%S") then
		local seconds_infinite = math.floor(ms / 1000)
		text = string.gsub(text, "%%S", seconds_infinite)
	end

	if string.match(text, "%%h") then
		local hours = math.floor(ms / 3600000) % 24
		text = string.gsub(text, "%%h", hours)
	end

	if string.match(text, "%%m") then
		local minutes = math.floor(ms / 60000) % 60
		text = string.gsub(text, "%%m", minutes)
	end

	if string.match(text, "%%s") then
		local seconds = math.floor(ms / 1000) % 60
		text = string.gsub(text, "%%s", seconds)
	end

	if string.match(text, "%%d") then
		local days = math.floor(ms / 86400000)
		text = string.gsub(text, "%%d", tostring(days))
	end

	local decimal = string.format("%.3d", ms % 1000)

	if string.match(text, "%%3t") then
		decimal = string.sub(decimal, 1, 3)
		text = string.gsub(text, "%%3t", decimal)
	end

	if string.match(text, "%%2t") then
		decimal = string.sub(decimal, 1, 2)
		text = string.gsub(text, "%%2t", decimal)
	end

	if string.match(text, "%%t") then
		decimal = string.sub(decimal, 1, 1)
		text = string.gsub(text, "%%t", decimal)
	end


  -- remove leading 0s
  -- checking length > 4 leavves 0s for minutes
  --while (flagRemoveLeading0s and string.len(text)>2) do
  while (flagRemoveLeading0s and string.len(text)>5) do
  	if (string.sub(text,1,2)=="0:") then
  		text = string.sub(text,3)
  	elseif (string.sub(text,1,3)=="00:") then
  		text = string.sub(text,4)
  	else
  		break
  	end
  end



  -- ATTN: new attempt to save some cpu cycles
  if (text == lastText) then
    --print "In set_time_text skiping update"
    return
  end
  lastText = text



	local source = obs.obs_get_source_by_name(source_name)
	if source ~= nil then
		local settings = obs.obs_data_create()
		obs.obs_data_set_string(settings, "text", text)
		obs.obs_source_update(source, settings)
		obs.obs_data_release(settings)
		obs.obs_source_release(source)
	end
end


-- ATTN: called automatically EVERY FRAME -- big waste of cpu; better to set up timer
-- see https://obsproject.com/docs/scripting.html#scripting-timers
function script_tick_DISABLED(sec)
  script_tick_manual(sec)
end


-- see https://obsproject.com/docs/scripting.html#scripting-timers
function script_tick_periodicTimer()
  script_tick_manual(timerMsDelay)
end


function script_tick_manual(sec)
  --print (string.format("In script_tick_manual 1 mode = '%s'.",mode))
  
	if timer_active == false then
		return
	end
	
	  --print "In script_tick_manual 2"

	local delta = obs.os_gettime_ns() - orig_time
	
	
	if (mode == "Streaming timer" or mode == "Recording timer") then
		if (mode == "Streaming timer") then
			cur_ns = getStats_streamingTime();
		elseif (mode == "Recording timer") then
			cur_ns = getStats_recordingTime();
		end
	else

		--if mode == "Countup" or mode == "Streaming timer" or mode == "Recording timer" or up == true then
  	if mode == "Countup" or up == true then
			cur_ns = cur_time + delta
		else
			cur_ns = cur_time - delta
		end
	end
	
	

	if cur_ns < 1 and (mode == "Countdown" or mode == "Specific time" or mode == "Specific date and time") then
		if up_when_finished == false then
			set_time_text(cur_ns, stop_text)
			-- dont disable? new
			stop_timer()
			return
		else
			cur_time = 0
			up = true
			start_timer()
			return
		end
	end

	set_time_text(cur_ns, format)
end








-- from Obs-Stats-on-Stream.lua
function getStats_streamingTime()
	local streaming_output = obs.obs_frontend_get_streaming_output();
	-- output will be nil when not actually streaming
	local streaming_duration_total_seconds = 0;
	if streaming_output ~= nil then
		-- Get streaming duration
	  local fps = obs.obs_get_active_fps();
		local total_frames = obs.obs_output_get_total_frames(streaming_output);
		streaming_duration_total_seconds =  total_frames / fps;
		obs.obs_output_release(streaming_output);
	end
	return streaming_duration_total_seconds * 1000000000;
end



function getStats_recordingTime()
	local recording_output = obs.obs_frontend_get_recording_output();
	local recording_duration_total_seconds = 0;
	if recording_output ~= nil then
		-- Get recording duration
  	local fps = obs.obs_get_active_fps();
		local recording_total_frames = obs.obs_output_get_total_frames(recording_output);
		recording_duration_total_seconds = recording_total_frames / fps;
		obs.obs_output_release(recording_output);
	end
	return recording_duration_total_seconds * 1000000000;
end








-- uncomment these print statements to better track when timer is turned on and off

function start_timer()
  if (timer_active) then
    return
  end
	timer_active = true
	orig_time = obs.os_gettime_ns()
	-- instead of calling ontick every frame we will now use a callback timer
	obs.timer_add(script_tick_periodicTimer, timerMsDelay)
	--print "ATTN: IMP Timer started."
end

function stop_timer()
  if (not timer_active) then
    return
  end
	timer_active = false
	obs.timer_remove(script_tick_periodicTimer)
	--print "ATTN: IMP Timer removed."
end







function on_event(event)
    if event == obs.OBS_FRONTEND_EVENT_EXIT then
			globalAppIsExiting = true
		end
end



function activate(activating)
  if (activating) then
    --print "ATTN: activate 1 on"
  else
    --print "ATTN: activate 1 off"
  end
	if activated == activating then
		return
	end
  --print "ATTN: activate 2"


  --print "ATTN: activate 3"
	activated = activating

	if activating then
    --print "ATTN: activate 4"
		if global then
			return
		end
	end


  -- not global activating, then run this
  --print "ATTN: activate 5"
  script_update(settings_, activating)
end



function activate_signal(cd, activating)
  if (activating) then 
  	--print "ATTN: in activate_signal on"
  else
    --print "ATTN: in activate_signal off"
    -- turn it OFF even if we can't find it below
    -- activate(activating)
  end
  
	local source = obs.calldata_source(cd, "source")
	if source ~= nil then
		local name = obs.obs_source_get_name(source)
		if (name == source_name) then
			activate(activating)
		end
	end
end






function source_activated(cd)
	activate_signal(cd, true)
end

function source_deactivated(cd)
  --print "IN source_deactivated"
	activate_signal(cd, false)
end


function reset(pressed)
	if not pressed then
		return
	end
	script_update(settings_, timer_active)
	
	-- test
	startTimerIfSourceVisible()
end


function on_pause(pressed)
	if not pressed then
		return
	end


	if cur_ns < 1 then
		reset(true)
	end

	if timer_active then
		stop_timer()
		cur_time = cur_ns
		paused = true
	else
		stop_timer()
		start_timer()
		paused = false
	end
end

function pause_button_clicked(props, p)
	on_pause(true)
	return true
end

function reset_button_clicked(props, p)
	reset(true)
	return true
end

function settings_modified(props, prop, settings)
	local mode_setting = obs.obs_data_get_string(settings, "mode")
	local p_duration = obs.obs_properties_get(props, "duration")
	local p_year = obs.obs_properties_get(props, "year")
	local p_month = obs.obs_properties_get(props, "month")
	local p_day = obs.obs_properties_get(props, "day")
	local p_hour = obs.obs_properties_get(props, "hour")
	local p_minutes = obs.obs_properties_get(props, "minutes")
	local p_seconds = obs.obs_properties_get(props, "seconds")
	local p_stop_text = obs.obs_properties_get(props, "stop_text")
	local p_a_mode = obs.obs_properties_get(props, "a_mode")
	local button_pause = obs.obs_properties_get(props, "pause_button")
	local button_reset = obs.obs_properties_get(props, "reset_button")
	local up_finished = obs.obs_properties_get(props, "countup_countdown_finished")

	if (mode_setting == "Countdown") then
		obs.obs_property_set_visible(p_duration, true)
		obs.obs_property_set_visible(p_year, false)
		obs.obs_property_set_visible(p_month, false)
		obs.obs_property_set_visible(p_day, false)
		obs.obs_property_set_visible(p_hour, false)
		obs.obs_property_set_visible(p_minutes, false)
		obs.obs_property_set_visible(p_seconds, false)
		obs.obs_property_set_visible(p_stop_text, true)
		obs.obs_property_set_visible(button_pause, true)
		obs.obs_property_set_visible(button_reset, true)
		obs.obs_property_set_visible(p_a_mode, true)
		obs.obs_property_set_visible(up_finished, true)
	elseif (mode_setting == "Countup") then
		obs.obs_property_set_visible(p_duration, false)
		obs.obs_property_set_visible(p_year, false)
		obs.obs_property_set_visible(p_month, false)
		obs.obs_property_set_visible(p_day, false)
		obs.obs_property_set_visible(p_hour, false)
		obs.obs_property_set_visible(p_minutes, false)
		obs.obs_property_set_visible(p_seconds, false)
		obs.obs_property_set_visible(p_stop_text, false)
		obs.obs_property_set_visible(button_pause, true)
		obs.obs_property_set_visible(button_reset, true)
		obs.obs_property_set_visible(p_a_mode, true)
		obs.obs_property_set_visible(up_finished, false)
	elseif (mode_setting == "Specific time") then
		obs.obs_property_set_visible(p_duration, false)
		obs.obs_property_set_visible(p_year, false)
		obs.obs_property_set_visible(p_month, false)
		obs.obs_property_set_visible(p_day, false)
		obs.obs_property_set_visible(p_hour, true)
		obs.obs_property_set_visible(p_minutes, true)
		obs.obs_property_set_visible(p_seconds, true)
		obs.obs_property_set_visible(p_stop_text, true)
		obs.obs_property_set_visible(button_pause, true)
		obs.obs_property_set_visible(button_reset, true)
		obs.obs_property_set_visible(p_a_mode, true)
		obs.obs_property_set_visible(up_finished, true)
	elseif (mode_setting == "Specific date and time") then
		obs.obs_property_set_visible(p_duration, false)
		obs.obs_property_set_visible(p_year, true)
		obs.obs_property_set_visible(p_month, true)
		obs.obs_property_set_visible(p_day, true)
		obs.obs_property_set_visible(p_hour, true)
		obs.obs_property_set_visible(p_minutes, true)
		obs.obs_property_set_visible(p_seconds, true)
		obs.obs_property_set_visible(p_stop_text, true)
		obs.obs_property_set_visible(button_pause, true)
		obs.obs_property_set_visible(button_reset, true)
		obs.obs_property_set_visible(p_a_mode, true)
		obs.obs_property_set_visible(up_finished, true)
	elseif (mode_setting == "Streaming timer") then
		obs.obs_property_set_visible(p_duration, false)
		obs.obs_property_set_visible(p_year, false)
		obs.obs_property_set_visible(p_month, false)
		obs.obs_property_set_visible(p_day, false)
		obs.obs_property_set_visible(p_hour, false)
		obs.obs_property_set_visible(p_minutes, false)
		obs.obs_property_set_visible(p_seconds, false)
		obs.obs_property_set_visible(p_stop_text, false)
		obs.obs_property_set_visible(button_pause, false)
		obs.obs_property_set_visible(button_reset, false)
		obs.obs_property_set_visible(p_a_mode, false)
		obs.obs_property_set_visible(up_finished, false)
	elseif (mode_setting == "Recording timer") then
		obs.obs_property_set_visible(p_duration, false)
		obs.obs_property_set_visible(p_year, false)
		obs.obs_property_set_visible(p_month, false)
		obs.obs_property_set_visible(p_day, false)
		obs.obs_property_set_visible(p_hour, false)
		obs.obs_property_set_visible(p_minutes, false)
		obs.obs_property_set_visible(p_seconds, false)
		obs.obs_property_set_visible(p_stop_text, false)
		obs.obs_property_set_visible(button_pause, false)
		obs.obs_property_set_visible(button_reset, false)
		obs.obs_property_set_visible(p_a_mode, false)
		obs.obs_property_set_visible(up_finished, false)
	end

	return true
end

function script_properties()
	local props = obs.obs_properties_create()

	local p_mode = obs.obs_properties_add_list(props, "mode", "Mode", obs.OBS_COMBO_TYPE_EDITABLE, obs.OBS_COMBO_FORMAT_STRING)
	obs.obs_property_list_add_string(p_mode, "Countdown", "countdown")
	obs.obs_property_list_add_string(p_mode, "Countup", "countup")
	obs.obs_property_list_add_string(p_mode, "Specific time", "specific_time")
	obs.obs_property_list_add_string(p_mode, "Specific date and time", "specific_date_and_time")
	obs.obs_property_list_add_string(p_mode, "Streaming timer", "stream")
	obs.obs_property_list_add_string(p_mode, "Recording timer", "recording")
	obs.obs_property_set_modified_callback(p_mode, settings_modified)

	obs.obs_properties_add_int(props, "duration", "Countdown duration (seconds)", 1, 100000000, 1)
	obs.obs_properties_add_int(props, "year", "Year", 1971, 100000000, 1)
	obs.obs_properties_add_int(props, "month", "Month (1-12)", 1, 12, 1)
	obs.obs_properties_add_int(props, "day", "Day (1-31)", 1, 31, 1)
	obs.obs_properties_add_int(props, "hour", "Hour (0-24)", 0, 24, 1)
	obs.obs_properties_add_int(props, "minutes", "Minutes (0-59)", 0, 59, 1)
	obs.obs_properties_add_int(props, "seconds", "Seconds (0-59)", 0, 59, 1)
	local f_prop = obs.obs_properties_add_text(props, "format", "Format", obs.OBS_TEXT_DEFAULT)
	obs.obs_property_set_long_description(f_prop, "%d - days\n%0h - hours with leading zero (00..23)\n%h - hours (0..23)\n%0H - hours with leading zero (00..infinity)\n%H - hours (0..infinity)\n%0m - minutes with leading zero (00..59)\n%m - minutes (0..59)\n%0M - minutes with leading zero (00..infinity)\n%M - minutes (0..infinity)\n%0s - seconds with leading zero (00..59)\n%s - seconds (0..59)\n%0S - seconds with leading zero (00..infinity)\n%S - seconds (0..infinity)\n%t - tenths\n%2t - hundredths\n%3t - thousandths")

	local p = obs.obs_properties_add_list(props, "source", "Text source", obs.OBS_COMBO_TYPE_EDITABLE, obs.OBS_COMBO_FORMAT_STRING)
	local sources = obs.obs_enum_sources()
	if sources ~= nil then
		for _, source in ipairs(sources) do
			source_id = obs.obs_source_get_unversioned_id(source)
			if source_id == "text_gdiplus" or source_id == "text_ft2_source" then
				local name = obs.obs_source_get_name(source)
				obs.obs_property_list_add_string(p, name, name)
			end
		end
	end
	obs.source_list_release(sources)

	obs.obs_properties_add_text(props, "stop_text", "Countdown final text", obs.OBS_TEXT_DEFAULT)

	local p_a_mode = obs.obs_properties_add_list(props, "a_mode", "Activation mode", obs.OBS_COMBO_TYPE_EDITABLE, obs.OBS_COMBO_FORMAT_STRING)
	obs.obs_property_list_add_string(p_a_mode, "Global (timer always active)", "global")
	obs.obs_property_list_add_string(p_a_mode, "Start timer on activation", "start_reset")
	obs.obs_properties_add_bool(props, "countup_countdown_finished", "Countup when done")

	obs.obs_properties_add_button(props, "pause_button", "Start/Stop", pause_button_clicked)
	obs.obs_properties_add_button(props, "reset_button", "Reset", reset_button_clicked)

	settings_modified(props, nil, settings_)

	return props
end

function script_description()
	return "Sets a text source to act as a timer with advanced options. Hotkeys can be set for starting/stopping and to the reset timer."
end

function script_update(settings, activating)
	stop_timer()
	up = false

	mode = obs.obs_data_get_string(settings, "mode")
	a_mode = obs.obs_data_get_string(settings, "a_mode")
	source_name = obs.obs_data_get_string(settings, "source")
	stop_text = obs.obs_data_get_string(settings, "stop_text")
	local year = obs.obs_data_get_int(settings, "year")
	local month = obs.obs_data_get_int(settings, "month")
	local day = obs.obs_data_get_int(settings, "day")
	local hour = obs.obs_data_get_int(settings, "hour")
	local minute = obs.obs_data_get_int(settings, "minutes")
	local second = obs.obs_data_get_int(settings, "seconds")
	format = obs.obs_data_get_string(settings, "format")
	up_when_finished = obs.obs_data_get_bool(settings, "countup_countdown_finished")

	if mode == "Countdown" then
		cur_time = obs.obs_data_get_int(settings, "duration") * 1000000000
	elseif mode == "Specific time" then
		cur_time = delta_time(-1, -1, -1, hour, minute, second)
	elseif mode == "Specific date and time" then
		cur_time = delta_time(year, month, day, hour, minute, second)
	else
		cur_time = 0
	end

	global = a_mode == "Global (timer always active)"

	if mode == "Streaming timer" or mode == "Recording timer" then
		global = true
	end
	
	
	-- ATTN: we do NOT support global timers with this new mod
	global = false
	

	set_time_text(cur_time, format)

	if global == false and paused == false then
    if (activating) then
	    -- ATTN: can we let this trigger on activate instead??
		  start_timer()
		else
			-- called automatically?
			startTimerIfSourceVisible()
		end
	end



end

function script_defaults(settings)
	obs.obs_data_set_default_int(settings, "duration", 5)
	obs.obs_data_set_default_int(settings, "year", os.date("%Y", os.time()))
	obs.obs_data_set_default_int(settings, "month", os.date("%m", os.time()))
	obs.obs_data_set_default_int(settings, "day", os.date("%d", os.time()))
	obs.obs_data_set_default_string(settings, "stop_text", "Starting soon (tm)")
	obs.obs_data_set_default_string(settings, "mode", "Countdown")
	obs.obs_data_set_default_string(settings, "a_mode", "Global (timer always active)")
	obs.obs_data_set_default_string(settings, "format", "%0H:%0m:%0s")
end

function script_save(settings)
	local hotkey_save_array_reset = obs.obs_hotkey_save(hotkey_id_reset)
	local hotkey_save_array_pause = obs.obs_hotkey_save(hotkey_id_pause)
	obs.obs_data_set_array(settings, "reset_hotkey", hotkey_save_array_reset)
	obs.obs_data_set_array(settings, "pause_hotkey", hotkey_save_array_pause)
	obs.obs_data_array_release(hotkey_save_array_pause)
	obs.obs_data_array_release(hotkey_save_array_reset)
end

function script_load(settings)
	local sh = obs.obs_get_signal_handler()
	--obs.signal_handler_connect(sh, "source_show", source_activated)
	--obs.signal_handler_connect(sh, "source_hide", source_deactivated)
	obs.signal_handler_connect(sh, "source_activate", source_activated)	
	obs.signal_handler_connect(sh, "source_deactivate", source_deactivated)	
	 

	hotkey_id_reset = obs.obs_hotkey_register_frontend("reset_timer_thingy", "Reset Timer", reset)
	hotkey_id_pause = obs.obs_hotkey_register_frontend("pause_timer", "Start/Stop Timer", on_pause)
	local hotkey_save_array_reset = obs.obs_data_get_array(settings, "reset_hotkey")
	local hotkey_save_array_pause = obs.obs_data_get_array(settings, "pause_hotkey")
	obs.obs_hotkey_load(hotkey_id_reset, hotkey_save_array_reset)
	obs.obs_hotkey_load(hotkey_id_pause, hotkey_save_array_pause)
	obs.obs_data_array_release(hotkey_save_array_reset)
	obs.obs_data_array_release(hotkey_save_array_pause)

	obs.obs_frontend_add_event_callback(on_event)

	settings_ = settings

	script_update(settings, false)

  -- test
	--startTimerIfSourceVisible()
end




-- Added by mouser@donationcoder.com 12/12/21
function script_unload()
  globalAppIsExiting = true
	stop_timer()
end




-- idea is to call this after script modifications, etc so we wont have to
-- but we havent yet figured out where to call this from
-- we could try calling it on every scene change
function startTimerIfSourceVisible()
  -- this function causes crashes on exit
  if (false) then
  	return
  end
  
  if (globalAppIsExiting) then
  	return
  end
  
	-- find source by name
	local source = obs.obs_get_source_by_name(source_name)
	if source ~= nil then
		-- is it visible?
		if (obs.obs_source_active(source)) then
			start_timer()
		end
		obs.obs_source_release(source)
	end
end
