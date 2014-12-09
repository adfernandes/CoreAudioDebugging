#!/bin/bash

FILE="CoreAudioDebugging.mm"
OBJF="CoreAudioDebugging.o"

IOS_SDK="8.1"
MAC9_SDK="10.9"
MAC10_SDK="10.10"

IOS_DEVICE="-arch armv7 -arch armv7s -arch arm64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS${IOS_SDK}.sdk"
IOS_SIMULATOR="-arch i386 -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator${IOS_SDK}.sdk"
MAC9_DEVICE="-arch i386 -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX${MAC9_SDK}.sdk"
MAC10_DEVICE="-arch i386 -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX${MAC10_SDK}.sdk"

/bin/rm -f "${OBJF}"

set -ex

for OBJC in "-ObjC++" ""; do
    for STD in "c++03" "c++11"; do
        for LIB in "libstdc++" "libc++"; do
            for CFG in "${IOS_DEVICE}" "${IOS_SIMULATOR}" "${MAC9_DEVICE}" "${MAC10_DEVICE}"; do
                /usr/bin/clang "${OBJC}" "-std=${STD}" "-stdlib=${LIB}" ${CFG} -Os -Wall -c "${FILE}" -o "${OBJF}"
                /bin/rm -f "${OBJF}"
            done
        done
    done
done
