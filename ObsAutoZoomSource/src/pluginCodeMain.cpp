//---------------------------------------------------------------------------
#include <cstdio>
//
#include "myPlugin.h"
//---------------------------------------------------------------------------







//---------------------------------------------------------------------------
void JrPlugin::doTick() {
	// ATTN: there is code in render() that could be placed here..
	// ATTN: rebuild sources from multisource effect plugin code -- needed every tick?
	checkAndUpdateAllTrackingSources();
}
//---------------------------------------------------------------------------










//---------------------------------------------------------------------------
void JrPlugin::checkAndUpdateAllTrackingSources() {

	bool sourcesDidChange = stracker.checkAndUpdateAllTrackingSources();
	if (sourcesDidChange) {
		needsRebuildStaging = true;
	}
}

//---------------------------------------------------------------------------










































//---------------------------------------------------------------------------
void JrPlugin::multiSourceTargetLost() {
	// when we lose a target, we consider checking the wider camera (source with lower values index)
	// if we can't re-aquire targets on the lowest source then we switch to option markerless source index and markerless view (by default full view)
	// note that this new view may NOT be the widest source(!)
	// but note that regardless of our default markerless source view, when we are looking to reaquire markers, we will always be scanning the widest (lowest index) source
}


void JrPlugin::multiSourceTargetChanged() {
	// when we find a new target in the CURRENT source, before we will switch and lock on to it, we try to find it in the most zoomed in source, and switch to it there.
}
//---------------------------------------------------------------------------













