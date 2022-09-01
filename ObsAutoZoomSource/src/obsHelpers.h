#pragma once

//---------------------------------------------------------------------------
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

