#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
"""
file_copy: Copy file line-by-line from source to destination
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
    regexEventBreak = re.compile("^EVENT\:(.*Title.*)")
    regexEventNonBreak = re.compile("^EVENT\:(.*)")
    regexGoLive = re.compile("(.*GOLIVE.*)")
    regexHotkey = re.compile("^HOTKEY\:(.*)")
    regexStreamTime = re.compile("^(.*) Stream Time Marker$")

    # Process the file line-by-line
    with open(fileIn, 'r') as fpIn, open(fileOut, 'w') as fpOut:
        writeTimestampLineToFile(fpOut, "0:00", "Start of stream")
        lineNumber = 0
        for line in fpIn:
            lineNumber += 1
            line = line.rstrip()   # Strip trailing spaces and newline
            # see what kind of line it is
            if (regexEventBreak.match(line)):
                # event break
                inBreakFlag = True
                currentEventLine = line
                withNextTimestamp = ""
                breakCount = breakCount + 1
            elif (regexGoLive.match(line)):
                # go live marker
                withNextTimestamp = "streamStartTimeReset"
            elif (regexHotkey.match(line)):
                # go hotkey
                currentEventLine = line
                withNextTimestamp = "write"
                if (writeLines == 0) and (timeOffsetSeconds == 0):
                    # kludge, if first hotkey comes before first break, we use it to reset stream time
                    withNextTimestamp = "writeAndStreamStartTimeReset"
            elif (regexStreamTime.match(line)):
                # should we write it or do something with it
                result = regexStreamTime.search(line)
                timeStampText = result.group(1)
                if (withNextTimestamp=="streamStartTimeReset") or (withNextTimestamp=="writeAndStreamStartTimeReset"):
                    # set offset
                    timeOffsetSeconds = convertTimeStampToSeconds(timeStampText)
                elif (withNextTimestamp=="write") or (withNextTimestamp=="writeAndStreamStartTimeReset"):
                    # write it out
                    timeStampTextAdjusted = adjustTimeStamp(timeStampText,timeOffsetSeconds)
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
        print("Processed lines: {}\n".format(lineNumber))



def adjustTimeStamp(timeStampText, timeOffsetSeconds):
    timeStampSecs = convertTimeStampToSeconds(timeStampText)
    timeStampSecs -= timeOffsetSeconds   
    return converSecondsToTimeStamp(timeStampSecs)


def convertTimeStampToSeconds(timeStampText):
    # see https://stackoverflow.com/questions/6402812/how-to-convert-an-hmmss-time-string-to-seconds-in-python
    h, m, s = timeStampText.split(':')
    return int(h) * 3600 + int(m) * 60 + int(s)


def converSecondsToTimeStamp(secs):
    # see https://stackoverflow.com/questions/1384406/convert-seconds-to-hhmmss-in-python
    dt = datetime.timedelta(seconds=secs)
    return str(dt)


def writeTimestampLineToFile(fpOut, timeStampText, lineLabel):
    timeStampTextClean = cleanTimeStampForOutput(timeStampText)
    fpOut.write("{} - {}\n".format(timeStampTextClean, lineLabel))
    # Need \n, which will be translated to platform-dependent newline


def cleanTimeStampForOutput(timeStampText):
    # remove leading 0s and :
    timeStampTextClean = re.sub("^[0\:]+" , "", timeStampText)
    if (timeStampTextClean==""):
        return timeStampText
    return timeStampTextClean

if __name__ == '__main__':
    main()