#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
"""
v2.3 - 7/6/22 - mouser@donationcoder.com
this tool scans infoWriter text output and writes all events that happen after a break or on a hotkey press
it uses the FIRST hotkey it finds before a break to set the starting offset of the stream
OR you can manually set the starting stream offset time by simply adding a HOTKEY:HOTKEY GOLIVE entry with the real Stream Time Marker time
ALSO you can modify whatever the calculated timestamps are by adding a line: "OFFSET 00:00:01" or "OFFSET -00:00:01" to add or subtract a second

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Usage: iwscan <src>
"""
import sys
import os
import re
import datetime

def main():
    # Check and retrieve command-line arguments
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)   # Return a non-zero value to indicate abnormal termination
    fileIn  = sys.argv[1]
    fileOut = fileIn + ".ts.txt"

    # Verify source file
    if not os.path.isfile(fileIn):
        print("error: {} does not exist".format(fileIn))
        sys.exit(1)
        
    linesWritten = doProcessFile(fileIn, fileOut, "Stream", True)
    if (linesWritten == 0):
        linesWritten = doProcessFile(fileIn, fileOut, "Record", False)

    print("Output timestamps:{}\n".format(linesWritten))


def doProcessFile(fileIn, fileOut, timeStampKeyword, heuristicResetAtFirstHotkey):

    # Verify destination file
    if False and os.path.isfile(fileOut):
        print("{} exists. Override (y/n)?".format(fileOut))
        reply = input().strip().lower()
        if reply[0] != 'y':
           sys.exit(1)

    # init
    currentEventLine = ""
    timeOffsetSeconds = 0
    withNextTimestamp = ""
    inBreakFlag = False
    breakCount = 0
    writeLines = 0
    # regex compiles
#    regexEventStartingSoon = re.compile("^EVENT\:(.*Starting Soon.*)")
    regexEventStartingSoon = re.compile(".*Starting.*")
    regexEventBreak = re.compile(".*EVENT\:(.*Title.*)")
    regexEventBreak2 = re.compile("^EVENT\:(.*Intro Video.*)")
    regexEventNonBreak = re.compile("^EVENT\:(.*)")
    regexGoLive = re.compile("(.*GOLIVE.*)")
    regexHotkey = re.compile("^HOTKEY\:(.*)")
    regexStreamTime = re.compile("^(.*) " + timeStampKeyword + " Time Marker$")
    regexEventStartStreaming = re.compile("^EVENT\:START STREAM \@ [0-9\-]+ (.*)$")
    regexEventStartBroadcasting = re.compile("^EVENT\:START BROADCASTING \@ [0-9\-]+ (.*)$")
    regexOffset = re.compile("^OFFSET (.*)$")
    # this will be assumed as the time that the youtube stream went live
    assumedStreamingStartTimestamp = "23:00:00"
    startingClockTimeStamp = ""
    assumedStartRecordingOffset = 0
    adjustOffsetTimestampSecs = 0
    guessingStarTime = True
    #
    #DefHotkeyStartStreamTimeCutoff = 0
    DefHotkeyStartStreamTimeCutoff = 60 * 20

    # Process the file line-by-line
    with open(fileIn, 'r') as fpIn, open(fileOut, 'w') as fpOut:
        lineNumber = 0
        for line in fpIn:
            lineNumber += 1
            line = line.rstrip()   # Strip trailing spaces and newline
            #print(line)
            # see what kind of line it is
            if (regexOffset.match(line)):
                # start of stream, get real world time
                result = regexOffset.search(line)
                offsetTimestamp = result.group(1)
                # manual adjustment
                adjustOffsetTimestampSecs = convertTimeStampToSeconds(offsetTimestamp)

            elif (regexEventStartBroadcasting.match(line)):
            	  # NEW EVENT THAT REALLY MARKS START OF GOING LIVE
                # is this all we need
                withNextTimestamp = "streamStartTimeReset"
                # tweak
                adjustOffsetTimestampSecs = -1
                # no more guessing
                guessingStarTime = False


            elif (regexEventStartStreaming.match(line) and (assumedStreamingStartTimestamp != "")):
                # start of stream, get real world time
                result = regexEventStartStreaming.search(line)
                startingClockTimeStamp = result.group(1)
                # the offset is the assumed stream broadcast wall clock start time - stream wall clock record start time
                assumedStartRecordingOffset = convertTimeStampToSeconds(assumedStreamingStartTimestamp) - convertTimeStampToSeconds(startingClockTimeStamp)
            elif ((regexEventBreak.match(line)) or (regexEventBreak2.match(line))):
                # event break
                inBreakFlag = True
                currentEventLine = line
                withNextTimestamp = ""
                breakCount = breakCount + 1
                # if this break is a starting soon, it MAY indicate that youtube stream has started
                if (regexEventStartingSoon.match(line)):
                    # starting soon display (the LAST ONE) may indicate right before we go live, if user switched to it right before pressing GO LIVE on youtube
                    if guessingStarTime:
                        withNextTimestamp = "streamStartTimeReset"
            elif (regexGoLive.match(line)):
                # go live marker
                withNextTimestamp = "streamStartTimeReset"
                guessingStarTime = False
            elif (regexHotkey.match(line)):
                # go hotkey
                currentEventLine = line
                withNextTimestamp = "write"
                writeRawLineToFile(fpOut, "// Found hotkey 1")
                if (writeLines == 0) and (timeOffsetSeconds <= DefHotkeyStartStreamTimeCutoff) and (heuristicResetAtFirstHotkey):
                    # kludge, if first hotkey comes before first break, we use it to reset stream time
                    withNextTimestamp = "writeAndStreamStartTimeReset"
                    guessingStarTime = False
                    writeRawLineToFile(fpOut, "// Found hotkey 2")
            elif (regexStreamTime.match(line)):
                # should we write it or do something with it
                result = regexStreamTime.search(line)
                timeStampText = result.group(1)
                if (withNextTimestamp=="streamStartTimeReset") or (withNextTimestamp=="writeAndStreamStartTimeReset"):
                    # set offset
                    timeOffsetSeconds = convertTimeStampToSeconds(timeStampText)
                elif (withNextTimestamp=="write") or (withNextTimestamp=="writeAndStreamStartTimeReset"):
                    # write it out
                    if (writeLines == 0):
                        # initial start time
                        writeRawLineToFile(fpOut, "// TIMESTAMP OUTPUTS")
                        writeRawLineToFile(fpOut, "// To manually add offset use: OFFSET 0:00:00")
                        writeTimestampLineToFile(fpOut, "0:00:00", "Start of stream")
                    # if no time offset is found, we can try to guess it
                    if (timeOffsetSeconds==0):
                        # no time offset so can we guess it using assumedStreamingStartTimestamp
                        timeOffsetSeconds = assumedStartRecordingOffset
                    # write it
                    timeStampTextAdjusted = adjustTimeStamp(timeStampText,timeOffsetSeconds - adjustOffsetTimestampSecs)
                    writeTimestampLineToFile(fpOut, timeStampTextAdjusted, currentEventLine)
                    writeLines = writeLines + 1                
                # clear
                withNextTimestamp = ""
                currentEventLine = ""
            elif (regexEventNonBreak.match(line)):
                # event break
                if (inBreakFlag):
                    inBreakFlag = False
                    currentEventLine = line
                    withNextTimestamp = "write"
    # return # of lines written
    return writeLines



def adjustTimeStamp(timeStampText, timeOffsetSeconds):
    timeStampSecs = convertTimeStampToSeconds(timeStampText)
    timeStampSecs -= timeOffsetSeconds   
    return converSecondsToTimeStamp(timeStampSecs)


def convertTimeStampToSeconds(timeStampText):
    # see https://stackoverflow.com/questions/6402812/how-to-convert-an-hmmss-time-string-to-seconds-in-python
    if (timeStampText==""):
    	return 0
    sgn = 1
    if (timeStampText[0]=='-'):
    	sgn = -1
    	timeStampText = timeStampText[1:]
    h, m, s = timeStampText.split(':')
    retv = sgn * (int(h) * 3600 + int(m) * 60 + int(s))
    return retv


def converSecondsToTimeStamp(secs):
    # see https://stackoverflow.com/questions/1384406/convert-seconds-to-hhmmss-in-python
    dt = datetime.timedelta(seconds=secs)
    return str(dt)


def writeTimestampLineToFile(fpOut, timeStampText, lineLabel):
    timeStampTextClean = cleanTimeStampForOutput(timeStampText)
    fpOut.write("{} - {}\n".format(timeStampTextClean, lineLabel))
    # Need \n, which will be translated to platform-dependent newline


def writeRawLineToFile(fpOut, str):
    fpOut.write("{}\n".format(str))


def cleanTimeStampForOutput(timeStampText):
    # remove leading 0s and :
    timeStampTextClean = re.sub("^[0\:]{1,3}" , "", timeStampText)
    if (timeStampTextClean==""):
        return timeStampText
    return timeStampTextClean

if __name__ == '__main__':
    main()