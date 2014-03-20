#include "CoreAudioDebugging.h"

#include <cstring>
#include <sstream>
#include <iomanip>
#include <vector>
#include <map>
#include <set>

/*

    The exported functions have been tested under iOS 6 and 7, and
    MacOS 10.8 and 10.9. They will probably work on both earlier
    and later versions, though, perhaps with mild tweaking.

    Although a C++ compiler and runtime are required, and both C++03
    and C++11 are supported, the exported functions are C-compatible.
    As per CoreAudio, the Objective-C runtime is not used, although
    compilation via Objective-C++ is supported.

    Created by Andrew Fernandes (andrew@fernandes.org) on 2014/03/14.

    Copyright (c) 2014 Andrew Fernandes and Pharynks Corporation.

    Licensed under the MIT license (http://opensource.org/licenses/MIT).

*/

using namespace std;

namespace {

    typedef map< OSType, char const * > OSTypeToStringMap;

    OSTypeToStringMap kAudioUnitManufacturer;
    OSTypeToStringMap kAudioUnitType;
    OSTypeToStringMap kAudioUnitSubType;
    OSTypeToStringMap kAudioFormat;

    bool SetupOSTypeMaps(void);

    const bool OSTypeMapsAreSetup = SetupOSTypeMaps();

    template< typename T >
    string ToHex(T value) {
        stringstream stream;
        stream << "0x" << setfill('0') << setw(2 * sizeof(T));
        stream << hex << uppercase << value;
        return stream.str();
    }

    struct FourCC {

        UInt32 code;

        union {
            UInt32 code;
            char prnt[4];
        } bige;

        string repr;

        FourCC(UInt32 _code)
        : code(_code) {
            bige.code = CFSwapInt32HostToBig(code);
            for (unsigned i = 0; i < 4; i++)
                if (!isprint(bige.prnt[i])) {
                    repr = ToHex(code);
                    return;
                }
            stringstream stream;
            stream << '\'';
            for (unsigned i = 0; i < 4; i++) stream << bige.prnt[i];
            stream << '\'';
            repr = stream.str();
            return;
        }
    };

    string OSTypeToString(const OSType &type, const OSTypeToStringMap &typemap) {
        OSTypeToStringMap::const_iterator iter = typemap.find(type);
        if (iter != typemap.end()) return (iter->second);
        return FourCC(type).repr;
    }

    struct StreamDescription : AudioStreamBasicDescription {

        // Much of this class was cobbled together by examining the
        // source of Apple's "Core Audio Utility Classes, v1.04".

        StreamDescription(const AudioStreamBasicDescription &asbd) {
            mSampleRate = asbd.mSampleRate;
            mFormatID = asbd.mFormatID;
            mFormatFlags = asbd.mFormatFlags;
            mBytesPerPacket = asbd.mBytesPerPacket;
            mFramesPerPacket = asbd.mFramesPerPacket;
            mBytesPerFrame = asbd.mBytesPerFrame;
            mChannelsPerFrame = asbd.mChannelsPerFrame;
            mBitsPerChannel = asbd.mBitsPerChannel;
            mReserved = asbd.mReserved;
        }

        UInt32 NumberInterleavedChannels() const {
            return IsInterleaved() ? mChannelsPerFrame : 1;
        }

        UInt32 NumberChannelStreams() const {
            return IsInterleaved() ? 1 : mChannelsPerFrame;
        }

        UInt32 NumberChannels() const {
            return mChannelsPerFrame;
        }

        UInt32 SampleWordSize() const {
            return (mBytesPerFrame > 0 && NumberInterleavedChannels()) ? (mBytesPerFrame / NumberInterleavedChannels()) : 0;
        }

        bool IsPCM() const {
            return mFormatID == kAudioFormatLinearPCM;
        }

        bool PackednessIsSignificant() const {
            return IsPCM() ? ((SampleWordSize() << 3) != mBitsPerChannel) : false;
        }

        bool AlignmentIsSignificant() const {
            return PackednessIsSignificant() || (mBitsPerChannel & 7) != 0;
        }

        bool IsInterleaved() const {
            return !IsPCM() || !(mFormatFlags & kAudioFormatFlagIsNonInterleaved);
        }

        bool IsSignedInteger() const {
            return IsPCM() && (mFormatFlags & kAudioFormatFlagIsSignedInteger);
        }

        bool IsFloat() const {
            return IsPCM() && (mFormatFlags & kAudioFormatFlagIsFloat);
        }

        bool IsNativeEndian() const {
            return (mFormatFlags & kAudioFormatFlagIsBigEndian) == kAudioFormatFlagsNativeEndian;
        }

        string ToString() const {
            stringstream out;

            char const *const eol = ", ";

            out << mChannelsPerFrame << " Ch @ " << mSampleRate << " Hz" << eol;
            out << "Format: " << OSTypeToString(mFormatID, kAudioFormat) << eol;
            switch (mFormatID) {

                case kAudioFormatAppleLossless:
                    switch (mFormatFlags) {
                        case kAppleLosslessFormatFlag_16BitSourceData:
                            out << "16";
                            break;
                        case kAppleLosslessFormatFlag_20BitSourceData:
                            out << "20";
                            break;
                        case kAppleLosslessFormatFlag_24BitSourceData:
                            out << "24";
                            break;
                        case kAppleLosslessFormatFlag_32BitSourceData:
                            out << "32";
                            break;
                        default:
                            out << "??";
                            break;
                    }
                    out << "-bit source data" << eol;
                    out << mFramesPerPacket << "frames/packet";
                    break;

                case kAudioFormatLinearPCM: {

                    int fracbits = (mFormatFlags & kLinearPCMFormatFlagsSampleFractionMask) >> kLinearPCMFormatFlagsSampleFractionShift;
                    if (fracbits > 0)
                        out << (mBitsPerChannel - fracbits) << '.' << fracbits;
                    else
                        out << mBitsPerChannel;
                    out << "-bit";

                    const int wordSize = SampleWordSize();
                    const bool isInt = !(mFormatFlags & kLinearPCMFormatFlagIsFloat);

                    if (wordSize > 1)
                        out << ((mFormatFlags & kLinearPCMFormatFlagIsBigEndian) ? " big-endian" : " little-endian") << eol;

                    if (isInt) {
                        out << ((mFormatFlags & kLinearPCMFormatFlagIsSignedInteger) ? "signed" : "unsigned");
                        out << " integer";
                    } else {
                        out << "floating-point";
                    }
                    out << eol;

                    if (wordSize > 0 && PackednessIsSignificant()) {
                        if (!(mFormatFlags & kLinearPCMFormatFlagIsPacked)) out << "un";
                        out << "packed in " << wordSize << " bytes";
                    }
                    if (wordSize > 0 && AlignmentIsSignificant())
                        out << ((mFormatFlags & kLinearPCMFormatFlagIsAlignedHigh) ? ", high-aligned" : ", low-aligned");

                    if (wordSize > 0 && (PackednessIsSignificant() || AlignmentIsSignificant()))
                        out << eol;

                    out << ((mFormatFlags & kAudioFormatFlagIsNonInterleaved) ? "non-interleaved" : "interleaved");

                }
                    break;

                default:
                    out << mBitsPerChannel << "bits/channel" << eol;
                    out << mBytesPerPacket << "bytes/packet" << eol;
                    out << mFramesPerPacket << "frames/packet" << eol;
                    out << mBytesPerFrame << "bytes/frame";
                    break;
            }

            return out.str();
        }
    };

    bool SetupOSTypeMaps(void) {

        kAudioUnitManufacturer['appl'] = "Apple";

        kAudioUnitType['auou'] = "Output";
        kAudioUnitType['aumu'] = "MusicDevice";
        kAudioUnitType['aumf'] = "MusicEffect";
        kAudioUnitType['aufc'] = "FormatConverter";
        kAudioUnitType['aufx'] = "Effect";
        kAudioUnitType['aumx'] = "Mixer";
        kAudioUnitType['aupn'] = "Panner";
        kAudioUnitType['auol'] = "OfflineEffect";
        kAudioUnitType['augn'] = "Generator";
        kAudioUnitType['auou'] = "Output";
        kAudioUnitType['aumu'] = "MusicDevice";
        kAudioUnitType['aumf'] = "MusicEffect";
        kAudioUnitType['aufc'] = "FormatConverter";
        kAudioUnitType['aufx'] = "Effect";
        kAudioUnitType['aumx'] = "Mixer";
        kAudioUnitType['aupn'] = "Panner";
        kAudioUnitType['augn'] = "Generator";
        kAudioUnitType['auol'] = "OfflineEffect";
        kAudioUnitType['aumi'] = "MIDIProcessor";
        kAudioUnitType['aurx'] = "RemoteEffect";
        kAudioUnitType['aurg'] = "RemoteGenerator";
        kAudioUnitType['auri'] = "RemoteInstrument";
        kAudioUnitType['aurm'] = "RemoteMusicEffect";

        kAudioUnitSubType['genr'] = "GenericOutput";
        kAudioUnitSubType['ahal'] = "HALOutput";
        kAudioUnitSubType['def '] = "DefaultOutput";
        kAudioUnitSubType['sys '] = "SystemOutput";
        kAudioUnitSubType['rioc'] = "RemoteIO";
        kAudioUnitSubType['vpio'] = "VoiceProcessingIO";
        kAudioUnitSubType['dls '] = "DLSSynth";
        kAudioUnitSubType['samp'] = "Sampler";
        kAudioUnitSubType['conv'] = "AUConverter";
        kAudioUnitSubType['vari'] = "Varispeed";
        kAudioUnitSubType['defr'] = "DeferredRenderer";
        kAudioUnitSubType['splt'] = "Splitter";
        kAudioUnitSubType['merg'] = "Merger";
        kAudioUnitSubType['nutp'] = "NewTimePitch";
        kAudioUnitSubType['ipto'] = "AUiPodTimeOther";
        kAudioUnitSubType['tmpt'] = "TimePitch";
        kAudioUnitSubType['raac'] = "RoundTripAAC";
        kAudioUnitSubType['iptm'] = "AUiPodTime";
        kAudioUnitSubType['lmtr'] = "PeakLimiter";
        kAudioUnitSubType['dcmp'] = "DynamicsProcessor";
        kAudioUnitSubType['lpas'] = "LowPassFilter";
        kAudioUnitSubType['hpas'] = "HighPassFilter";
        kAudioUnitSubType['bpas'] = "BandPassFilter";
        kAudioUnitSubType['hshf'] = "HighShelfFilter";
        kAudioUnitSubType['lshf'] = "LowShelfFilter";
        kAudioUnitSubType['pmeq'] = "ParametricEQ";
        kAudioUnitSubType['dist'] = "Distortion";
        kAudioUnitSubType['dely'] = "Delay";
        kAudioUnitSubType['greq'] = "GraphicEQ";
        kAudioUnitSubType['mcmp'] = "MultiBandCompressor";
        kAudioUnitSubType['mrev'] = "MatrixReverb";
        kAudioUnitSubType['tmpt'] = "Pitch";
        kAudioUnitSubType['filt'] = "AUFilter";
        kAudioUnitSubType['nsnd'] = "NetSend";
        kAudioUnitSubType['sdly'] = "SampleDelay";
        kAudioUnitSubType['rogr'] = "RogerBeep";
        kAudioUnitSubType['rvb2'] = "Reverb2";
        kAudioUnitSubType['ipeq'] = "AUiPodEQ";
        kAudioUnitSubType['nbeq'] = "NBandEQ";
        kAudioUnitSubType['mcmx'] = "MultiChannelMixer";
        kAudioUnitSubType['mxmx'] = "MatrixMixer";
        kAudioUnitSubType['smxr'] = "StereoMixer";
        kAudioUnitSubType['3dmx'] = "3DMixer";
        kAudioUnitSubType['3dem'] = "AU3DMixerEmbedded";
        kAudioUnitSubType['sphr'] = "SphericalHeadPanner";
        kAudioUnitSubType['vbas'] = "VectorPanner";
        kAudioUnitSubType['ambi'] = "SoundFieldPanner";
        kAudioUnitSubType['hrtf'] = "HRTFPanner";
        kAudioUnitSubType['nrcv'] = "NetReceive";
        kAudioUnitSubType['sspl'] = "ScheduledSoundPlayer";
        kAudioUnitSubType['afpl'] = "AudioFilePlayer";

        kAudioFormat['lpcm'] = "LinearPCM";
        kAudioFormat['ac-3'] = "AC3";
        kAudioFormat['cac3'] = "60958AC3";
        kAudioFormat['ima4'] = "AppleIMA4";
        kAudioFormat['aac '] = "MPEG4AAC";
        kAudioFormat['celp'] = "MPEG4CELP";
        kAudioFormat['hvxc'] = "MPEG4HVXC";
        kAudioFormat['twvq'] = "MPEG4TwinVQ";
        kAudioFormat['MAC3'] = "MACE3";
        kAudioFormat['MAC6'] = "MACE6";
        kAudioFormat['ulaw'] = "ULaw";
        kAudioFormat['alaw'] = "ALaw";
        kAudioFormat['QDMC'] = "QDesign";
        kAudioFormat['QDM2'] = "QDesign2";
        kAudioFormat['Qclp'] = "QUALCOMM";
        kAudioFormat['.mp1'] = "MPEGLayer1";
        kAudioFormat['.mp2'] = "MPEGLayer2";
        kAudioFormat['.mp3'] = "MPEGLayer3";
        kAudioFormat['time'] = "TimeCode";
        kAudioFormat['midi'] = "MIDIStream";
        kAudioFormat['apvs'] = "ParameterValueStream";
        kAudioFormat['alac'] = "AppleLossless";
        kAudioFormat['aach'] = "MPEG4AAC_HE";
        kAudioFormat['aacl'] = "MPEG4AAC_LD";
        kAudioFormat['aace'] = "MPEG4AAC_ELD";
        kAudioFormat['aacf'] = "MPEG4AAC_ELD_SBR";
        kAudioFormat['aacg'] = "MPEG4AAC_ELD_V2";
        kAudioFormat['aacp'] = "MPEG4AAC_HE_V2";
        kAudioFormat['aacs'] = "MPEG4AAC_Spatial";
        kAudioFormat['samr'] = "AMR";
        kAudioFormat['AUDB'] = "Audible";
        kAudioFormat['ilbc'] = "iLBC";
        kAudioFormat[0x6D730011] = "DVIIntelIMA";
        kAudioFormat[0x6D730031] = "MicrosoftGSM";
        kAudioFormat['aes3'] = "AES3";

        return true;
    }

}

extern "C" __attribute__((visibility("default")))
char * StringFromAudioStreamBasicDescription(const AudioStreamBasicDescription asbd) {
    StreamDescription format(asbd);
    string description = format.ToString();
    char *buffer = static_cast<char *>( calloc(description.length() + 1, sizeof(string::value_type)) );
    copy(description.begin(), description.end(), buffer);
    return buffer;
}

extern "C" __attribute__((visibility("default")))
char * StringFromAudioComponentDescription(const AudioComponentDescription acd, const bool includeFlags) {
    ostringstream format;
    format << "Manufacturer: " << OSTypeToString(acd.componentManufacturer, kAudioUnitManufacturer) << ", ";
    format << "Type: " << OSTypeToString(acd.componentType, kAudioUnitType) << ", ";
    format << "SubType: " << OSTypeToString(acd.componentSubType, kAudioUnitSubType);
    if (includeFlags) {
        format << ", ";
        format << "Flags: " << ToHex(acd.componentFlags) << ", ";
        format << "FlagsMask: " << ToHex(acd.componentFlagsMask);
    }
    string description = format.str();
    char *buffer = static_cast<char *>( calloc(description.length() + 1, sizeof(string::value_type)) );
    copy(description.begin(), description.end(), buffer);
    return buffer;
}

#if __OBJC__

extern "C" __attribute__((visibility("default")))
NSString * NSStringFromAudioStreamBasicDescription(const AudioStreamBasicDescription asbd) {
    char * buffer = StringFromAudioStreamBasicDescription(asbd);
    NSString * value = [NSString stringWithCString:buffer encoding:NSUTF8StringEncoding];
    free(buffer);
    return value;
}

extern "C" __attribute__((visibility("default")))
    NSString * NSStringFromAudioComponentDescription(const AudioComponentDescription acd, const bool includeFlags) {
    char * buffer = StringFromAudioComponentDescription(acd,includeFlags);
    NSString * value = [NSString stringWithCString:buffer encoding:NSUTF8StringEncoding];
    free(buffer);
    return value;
}

#endif
