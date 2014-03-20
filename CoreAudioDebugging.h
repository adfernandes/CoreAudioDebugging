#ifndef CORE_AUDIO_DEBUGGING_H
#define CORE_AUDIO_DEBUGGING_H

#include <AudioUnit/AUComponent.h>

#ifdef __cplusplus
extern "C" {
#endif

/*

    When built as non-Objective-C/C++, the pointers returned by the functions below
    can be simply 'free'ed when no longer needed as they are allocated with 'calloc'.

    Created by Andrew Fernandes (andrew@fernandes.org) on 2014/03/14.

    Copyright (c) 2014 Andrew Fernandes and Pharynks Corporation.

    Licensed under the MIT license (http://opensource.org/licenses/MIT).

*/

char * StringFromAudioStreamBasicDescription(const AudioStreamBasicDescription asbd);

#ifdef __cplusplus
    char * StringFromAudioComponentDescription(const AudioComponentDescription acd, const bool includeFlags = false );
#else
    char * StringFromAudioComponentDescription(const AudioComponentDescription acd, const bool includeFlags );
#endif

#if __OBJC__

    #import <Foundation/NSString.h>

    NSString * NSStringFromAudioStreamBasicDescription(const AudioStreamBasicDescription asbd);

    #ifdef __cplusplus
        NSString * NSStringFromAudioComponentDescription(const AudioComponentDescription acd, const bool includeFlags = false );
    #else
        NSString * NSStringFromAudioComponentDescription(const AudioComponentDescription acd, const bool includeFlags );
    #endif

#endif

#ifdef __cplusplus
}
#endif
#endif /* ! CORE_AUDIO_DEBUGGING_H */
