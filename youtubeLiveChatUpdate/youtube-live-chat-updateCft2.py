﻿"""
youtube-live-chat-update.py
OBS script that updates the YouTube livechat URL on a browser source on stream 
start to the livechat associated with the (first) current livestream on a 
given channel.
To use this you will need a YouTube API key, see 
  https://developers.google.com/youtube/v3/getting-started 
and 
  https://developers.google.com/youtube/registering_an_application
for more information on how to go about that.
Additionally you will require the channel ID of the channel for which you want
to query the current live stream. You can retrieve that by inspecting the
web site source of the channel in question and looking for `externalId`.
---
Copyright (c) 2021 Gina Häußge
Permission is hereby granted, free of charge, to any person obtaining a 
copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included 
in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
IN THE SOFTWARE.
"""


# ATTN: MODIFIED 12/9/2 by jesse reichler to better get stream
# my first try required the scene with browser source have "chat" or "youtube" in the name
# now instead we use settings
# see https://gist.github.com/foosel/1e39096ab679e14c36727cd6f72d5aeb

# NOTE: This cannot work for UNLISTED videos

import obspython as obs
import urllib.request
import urllib.error
import json

__author__ = "Gina Häußge"
__license__ = "MIT"

channelid   = "UCVhDxIFwwaaeb7nZ4cWA3tw"
apikey      = "YOUR_YOUTUBE_APIKEY"
source_name = "BrowserTest"
delay       = 5

globalAppIsExiting = False
globalFoundGoodUrl = False
globalTrustFoundGoodUrl = True
livechat = ""

YOUTUBE_QUERY_URL = "https://www.googleapis.com/youtube/v3/search?part=snippet&channelId={channelid}&eventType=live&type=video&key={apikey}"
YOUTUBE_CHAT_URL = "https://gaming.youtube.com/live_chat?v={vid}"
NO_STREAM = "https://gaming.youtube.com/live_chat?v=VIDEOIDHERE"
# ------------------------------------------------------------

def update_url():
    global channelid
    global apikey
    global source_name
    global globalFoundGoodUrl
    global globalTrustFoundGoodUrl
    global livechat
    
    obs.timer_remove(update_url)
    
    if (globalTrustFoundGoodUrl and globalFoundGoodUrl):
        # we can try skipping refetching if we haven't reset the streaming so that switching back to scene doesnt require a refetch
        print("Skipping refetch of chat url since we already have a good one: " + livechat)
        return

    source = obs.obs_get_source_by_name(source_name)
    # NOTE source must be released or crash on exit
    if apikey and source is not None:
        url = YOUTUBE_QUERY_URL.format(channelid=channelid, apikey=apikey)
        try:
            with urllib.request.urlopen(url) as response:
                data = response.read()
                body = data.decode('utf-8')
                data = json.loads(body)

                items = data.get("items")
                if items:
                    vid = items[0]["id"]["videoId"]
                    print("Found live stream, vid: {}".format(vid))

                    livechat = YOUTUBE_CHAT_URL.format(vid=vid)
                    print("Setting livechat URL to {}".format(livechat))
                else:
                    livechat = NO_STREAM
                    # obs.script_log(obs.LOG_DEBUG, "No active stream, skipping update.")
                    print("ATTN: No active stream data found at '" + url + "', skipping update.")
                    # test pretend there is a url
                    #livechat = "https://gaming.youtube.com/live_chat?v=VIDEOIDHERETEST"

                if livechat != NO_STREAM:
                	settings = obs.obs_data_create()
                	#
                	oldSourceSettings = obs.obs_source_get_settings(source);
                	livechatOld = obs.obs_data_get_string(oldSourceSettings, "url")
                	obs.obs_data_release(oldSourceSettings)
                	#
                	if livechatOld != livechat:
                		obs.obs_data_set_string(settings, "url", livechat)
                		obs.obs_source_update(source, settings)
                		print("ATTN: " + "Set livechat url to " + livechat)
                		globalFoundGoodUrl = True
                	else:
                		print("ATTN:Livechat url remains unchanged, doing noting: " + livechat + " vs " + livechatOld)
                	obs.obs_data_release(settings)

        except urllib.error.URLError as err:
            obs.script_log(obs.LOG_WARNING, "Error retrieving livechat URL for channel '" + channelid + "': " + err.reason)
            obs.remove_current_callback()

        obs.obs_source_release(source)

def refresh_pressed(props, prop):
    print("Refresh pressed")
    update_url()

def on_frontend_event(event):
    #print ("ATTN: on_frontend_event 1")
    global globalAppIsExiting
    if event == obs.OBS_FRONTEND_EVENT_STREAMING_STARTED:
        global delay

        #print("ATTN: Streaming started")
        global globalFoundGoodUrl
        globalFoundGoodUrl = False
        obs.timer_remove(update_url)
        obs.timer_add(update_url, delay * 1000)
    elif event == obs.OBS_FRONTEND_EVENT_STREAMING_STOPPED:
        #print("ATTN: Streaming stopped")
        obs.timer_remove(update_url)
    elif event == obs.OBS_FRONTEND_EVENT_EXIT:
        #print ("ATTN: on_frontend_event exit")
        globalAppIsExiting = True
        # TEST
        # obs.timer_remove(update_url)
    #elif event == obs.OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
    #    #print ("ATTN: on_frontend_event cleanup")
    #    globalAppIsExiting = True
    #    # TEST
    #    # obs.timer_remove(update_url)
    elif (not globalAppIsExiting) and (event == obs.OBS_FRONTEND_EVENT_SCENE_CHANGED):
        #print ("ATTN: on_frontend_event OBS_FRONTEND_EVENT_SCENE_CHANGED")
        #obs.script_log(obs.LOG_DEBUG, "ATTN:In on_frontend_event scene changed.")
        # is it possible this event is causing an OBS crash on exit?
        # THIS FUNCTION IS CRASHING OUR OBS
        #return
        # let's try to do this in a timer since its causing an obs creash on exit and i dont know how to stop that because we live in hell      
        obs.timer_remove(checkCurrentSceneOnChange)
        obs.timer_add(checkCurrentSceneOnChange, 100)
# ------------------------------------------------------------


# new code
def checkCurrentSceneOnChange():
    #print ("ATTN: checkCurrentSceneOnChange1")
    global globalAppIsExiting
    obs.timer_remove(checkCurrentSceneOnChange)
    #print ("ATTN: checkCurrentSceneOnChange2")
    #obs.script_log(obs.LOG_DEBUG, "In checkCurrentSceneOnChange.")
    if (globalAppIsExiting):
    	  #print ("ATTN: checkCurrentSceneOnChange - globalAppIsExiting")
    	  #obs.script_log(obs.LOG_DEBUG, "In checkCurrentSceneOnChange leaving early.")
    	  return
    # we are getting here on program exit and crashing
    #print ("ATTN: checkCurrentSceneOnChange3")
    current_scene = obs.obs_frontend_get_current_scene()
    #print ("ATTN: checkCurrentSceneOnChange4")
    if (current_scene != None):
        scene = obs.obs_scene_from_source(current_scene)
        #sceneName = obs.obs_source_get_name(current_scene).lower()
        #obs.script_log(obs.LOG_DEBUG, "sceneName = " + sceneName)
        #if ("chat" in sceneName) or ("youtube" in sceneName):
        if (scene != None):
            scene_item = obs.obs_scene_find_source(scene, source_name)
            if (scene_item != None):
                obs.script_log(obs.LOG_DEBUG, "ATTN: Changed to script designated chat scene, trigerring update..")
                # ask to reload url
                obs.timer_remove(update_url)
                obs.timer_add(update_url, 200)
            else:
            	  #obs.script_log(obs.LOG_DEBUG, "Changed to NON-browser chat scene.")
    	          pass
    	  # without this line we get CRASH ON EXIT (sometimes!!!)
        obs.obs_source_release(current_scene)





def script_description():
    return "Updates the YouTube livechat URL on a browser source on stream start.\n\nby @foosel w/ mouser mods"

def script_update(settings):
    global channelid
    global apikey
    global source_name
    global delay

    channelid    = obs.obs_data_get_string(settings, "channelid")
    apikey       = obs.obs_data_get_string(settings, "apikey")
    source_name  = obs.obs_data_get_string(settings, "source")
    delay        = obs.obs_data_get_int(settings, "delay")

def script_defaults(settings):
    obs.obs_data_set_default_int(settings, "delay", 5)

def script_properties():
    props = obs.obs_properties_create()

    obs.obs_properties_add_text(props, "channelid", "YouTube Channel ID", obs.OBS_TEXT_DEFAULT)
    obs.obs_properties_add_text(props, "apikey", "YouTube API key", obs.OBS_TEXT_DEFAULT)

    p = obs.obs_properties_add_list(props, "source", "Browser Source", obs.OBS_COMBO_TYPE_EDITABLE, obs.OBS_COMBO_FORMAT_STRING)
    sources = obs.obs_enum_sources()
    if sources is not None:
        for source in sources:
            source_id = obs.obs_source_get_unversioned_id(source)
            if source_id == "browser_source":
                name = obs.obs_source_get_name(source)
                obs.obs_property_list_add_string(p, name, name)

        obs.source_list_release(sources)

    obs.obs_properties_add_int(props, "delay", "Execution delay (seconds)", 5, 3600, 1)

    obs.obs_properties_add_button(props, "button", "Refresh", refresh_pressed)
    return props

def script_load(settings):
    # TEST
    obs.obs_frontend_add_event_callback(on_frontend_event)
    pass



def script_unload():
    #print ("ATTN: script_unload")
    global globalAppIsExiting
    globalAppIsExiting = True
    obs.obs_frontend_remove_event_callback(on_frontend_event)
    # obs.timer_remove(update_url)

