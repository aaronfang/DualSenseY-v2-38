#include <Propvarutil.h>
#include <chrono>
#include <hidapi.h>
#include <iostream>
#include <mmdeviceapi.h>
#include <thread>
#include <vector>
#pragma comment(lib, "Propsys.lib")
#include <atlbase.h>
#include <functiondiscoverykeys_devpkey.h>
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <algorithm>
#include <array>
#include <cfgmgr32.h>
#include <cstdint>
#include <regex>
#include <setupapi.h>
#include <sstream>
#include <string>
#include <usbiodef.h>
#include <nlohmann/json.hpp>
#include <codecvt>

using namespace std;

// get-parent-device -> https://github.com/mkielar/get-parent-device/blob/master/get-parent-device/main.cpp
constexpr int ERR_BAD_ARGUMENTS = 1;
constexpr int ERR_NO_DEVICES_FOUND = 2;
constexpr int ERR_NO_DEVICE_INFO = 3;
constexpr wchar_t GUID_DISK_DRIVE_STRING[] =
    L"{21EC2020-3AEA-1069-A2DD-08002B30309D}";
BOOL GetParentDeviceInstanceId(_Out_ std::wstring &parentDeviceInstanceId,
                               _Out_ DEVINST &parentDeviceInstanceIdHandle,
                               _In_ DEVINST currentDeviceInstanceId)
{
    CONFIGRET result = CM_Get_Parent(&parentDeviceInstanceIdHandle,
                                     currentDeviceInstanceId,
                                     0);
    if (result == CR_SUCCESS)
    {
        parentDeviceInstanceId.clear();
        parentDeviceInstanceId.resize(MAX_DEVICE_ID_LEN);
        result = CM_Get_Device_IDW(parentDeviceInstanceIdHandle,
                                   (PWSTR)parentDeviceInstanceId.data(),
                                   MAX_DEVICE_ID_LEN,
                                   0);
        if (result == CR_SUCCESS)
        {
            return TRUE;
        }
    }
    return FALSE;
}

// Tests whether given Device Instance ID matches the pattern.
BOOL DeviceIdMatchesPattern(const std::wstring &deviceInstanceId,
                            const std::wstring &pattern)
{
    std::wregex regexPattern(pattern);
    return std::regex_match(deviceInstanceId, regexPattern);
}

// Searches for a parent device with a matching Device Instance ID pattern
int FindParentDeviceInstanceId(
    const std::wstring &searchedDeviceInstanceId,
    const std::wstring &parentDeviceInstanceIdPattern,
    std::wstring &foundDeviceInstanceId)
{
    GUID diskDriveGuid;
    CLSIDFromString(GUID_DISK_DRIVE_STRING, &diskDriveGuid);

    HDEVINFO devInfo = SetupDiGetClassDevs(&diskDriveGuid,
                                           NULL,
                                           NULL,
                                           DIGCF_PRESENT | DIGCF_ALLCLASSES);
    std::wstring deviceInstanceId(MAX_DEVICE_ID_LEN, L'\0');

    if (devInfo != INVALID_HANDLE_VALUE)
    {
        DWORD devIndex = 0;
        SP_DEVINFO_DATA devInfoData = {};
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        while (SetupDiEnumDeviceInfo(devInfo, devIndex, &devInfoData))
        {
            std::fill(deviceInstanceId.begin(), deviceInstanceId.end(), L'\0');
            SetupDiGetDeviceInstanceIdW(devInfo,
                                        &devInfoData,
                                        (PWSTR)deviceInstanceId.data(),
                                        MAX_PATH,
                                        0);

            if (_wcsicmp(searchedDeviceInstanceId.c_str(),
                         deviceInstanceId.c_str()) == 0)
            {
                DEVINST currentDeviceInstanceId = devInfoData.DevInst;
                DEVINST parentDeviceInstanceId = 0;
                std::wstring parentDeviceInstanceIdStr;

                while (true)
                {
                    std::fill(deviceInstanceId.begin(),
                              deviceInstanceId.end(),
                              L'\0');
                    parentDeviceInstanceId = 0;
                    if (GetParentDeviceInstanceId(parentDeviceInstanceIdStr,
                                                  parentDeviceInstanceId,
                                                  currentDeviceInstanceId))
                    {
                        if (DeviceIdMatchesPattern(
                                parentDeviceInstanceIdStr,
                                parentDeviceInstanceIdPattern))
                        {
                            foundDeviceInstanceId =
                                parentDeviceInstanceIdStr; // Found ID
                            return 0;
                        }
                        currentDeviceInstanceId = parentDeviceInstanceId;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            devIndex++;
        }

        if (devIndex == 0)
        {
            return ERR_NO_DEVICES_FOUND;
        }
    }
    else
    {
        return ERR_NO_DEVICE_INFO;
    }
    return ERR_NO_DEVICES_FOUND;
}

#define MAX_READ_LENGTH 64
#define MAX_USB_WRITE_LENGTH 48
#define MAX_BT_WRITE_LENGTH 78

const UINT32 hashTable[256] =
    { // Hash table from -> https://github.com/Ohjurot/DualSense-Windows/blob/main/VS19_Solution/DualSenseWindows/src/DualSenseWindows/DS_CRC32.cpp
        0xd202ef8d, 0xa505df1b, 0x3c0c8ea1, 0x4b0bbe37, 0xd56f2b94, 0xa2681b02,
        0x3b614ab8, 0x4c667a2e, 0xdcd967bf, 0xabde5729, 0x32d70693, 0x45d03605,
        0xdbb4a3a6, 0xacb39330, 0x35bac28a, 0x42bdf21c, 0xcfb5ffe9, 0xb8b2cf7f,
        0x21bb9ec5, 0x56bcae53, 0xc8d83bf0, 0xbfdf0b66, 0x26d65adc, 0x51d16a4a,
        0xc16e77db, 0xb669474d, 0x2f6016f7, 0x58672661, 0xc603b3c2, 0xb1048354,
        0x280dd2ee, 0x5f0ae278, 0xe96ccf45, 0x9e6bffd3, 0x762ae69,  0x70659eff,
        0xee010b5c, 0x99063bca, 0xf6a70,    0x77085ae6, 0xe7b74777, 0x90b077e1,
        0x9b9265b,  0x7ebe16cd, 0xe0da836e, 0x97ddb3f8, 0xed4e242,  0x79d3d2d4,
        0xf4dbdf21, 0x83dcefb7, 0x1ad5be0d, 0x6dd28e9b, 0xf3b61b38, 0x84b12bae,
        0x1db87a14, 0x6abf4a82, 0xfa005713, 0x8d076785, 0x140e363f, 0x630906a9,
        0xfd6d930a, 0x8a6aa39c, 0x1363f226, 0x6464c2b0, 0xa4deae1d, 0xd3d99e8b,
        0x4ad0cf31, 0x3dd7ffa7, 0xa3b36a04, 0xd4b45a92, 0x4dbd0b28, 0x3aba3bbe,
        0xaa05262f, 0xdd0216b9, 0x440b4703, 0x330c7795, 0xad68e236, 0xda6fd2a0,
        0x4366831a, 0x3461b38c, 0xb969be79, 0xce6e8eef, 0x5767df55, 0x2060efc3,
        0xbe047a60, 0xc9034af6, 0x500a1b4c, 0x270d2bda, 0xb7b2364b, 0xc0b506dd,
        0x59bc5767, 0x2ebb67f1, 0xb0dff252, 0xc7d8c2c4, 0x5ed1937e, 0x29d6a3e8,
        0x9fb08ed5, 0xe8b7be43, 0x71beeff9, 0x6b9df6f,  0x98dd4acc, 0xefda7a5a,
        0x76d32be0, 0x1d41b76,  0x916b06e7, 0xe66c3671, 0x7f6567cb, 0x862575d,
        0x9606c2fe, 0xe101f268, 0x7808a3d2, 0xf0f9344,  0x82079eb1, 0xf500ae27,
        0x6c09ff9d, 0x1b0ecf0b, 0x856a5aa8, 0xf26d6a3e, 0x6b643b84, 0x1c630b12,
        0x8cdc1683, 0xfbdb2615, 0x62d277af, 0x15d54739, 0x8bb1d29a, 0xfcb6e20c,
        0x65bfb3b6, 0x12b88320, 0x3fba6cad, 0x48bd5c3b, 0xd1b40d81, 0xa6b33d17,
        0x38d7a8b4, 0x4fd09822, 0xd6d9c998, 0xa1def90e, 0x3161e49f, 0x4666d409,
        0xdf6f85b3, 0xa868b525, 0x360c2086, 0x410b1010, 0xd80241aa, 0xaf05713c,
        0x220d7cc9, 0x550a4c5f, 0xcc031de5, 0xbb042d73, 0x2560b8d0, 0x52678846,
        0xcb6ed9fc, 0xbc69e96a, 0x2cd6f4fb, 0x5bd1c46d, 0xc2d895d7, 0xb5dfa541,
        0x2bbb30e2, 0x5cbc0074, 0xc5b551ce, 0xb2b26158, 0x4d44c65,  0x73d37cf3,
        0xeada2d49, 0x9ddd1ddf, 0x3b9887c,  0x74beb8ea, 0xedb7e950, 0x9ab0d9c6,
        0xa0fc457,  0x7d08f4c1, 0xe401a57b, 0x930695ed, 0xd62004e,  0x7a6530d8,
        0xe36c6162, 0x946b51f4, 0x19635c01, 0x6e646c97, 0xf76d3d2d, 0x806a0dbb,
        0x1e0e9818, 0x6909a88e, 0xf000f934, 0x8707c9a2, 0x17b8d433, 0x60bfe4a5,
        0xf9b6b51f, 0x8eb18589, 0x10d5102a, 0x67d220bc, 0xfedb7106, 0x89dc4190,
        0x49662d3d, 0x3e611dab, 0xa7684c11, 0xd06f7c87, 0x4e0be924, 0x390cd9b2,
        0xa0058808, 0xd702b89e, 0x47bda50f, 0x30ba9599, 0xa9b3c423, 0xdeb4f4b5,
        0x40d06116, 0x37d75180, 0xaede003a, 0xd9d930ac, 0x54d13d59, 0x23d60dcf,
        0xbadf5c75, 0xcdd86ce3, 0x53bcf940, 0x24bbc9d6, 0xbdb2986c, 0xcab5a8fa,
        0x5a0ab56b, 0x2d0d85fd, 0xb404d447, 0xc303e4d1, 0x5d677172, 0x2a6041e4,
        0xb369105e, 0xc46e20c8, 0x72080df5, 0x50f3d63,  0x9c066cd9, 0xeb015c4f,
        0x7565c9ec, 0x262f97a,  0x9b6ba8c0, 0xec6c9856, 0x7cd385c7, 0xbd4b551,
        0x92dde4eb, 0xe5dad47d, 0x7bbe41de, 0xcb97148,  0x95b020f2, 0xe2b71064,
        0x6fbf1d91, 0x18b82d07, 0x81b17cbd, 0xf6b64c2b, 0x68d2d988, 0x1fd5e91e,
        0x86dcb8a4, 0xf1db8832, 0x616495a3, 0x1663a535, 0x8f6af48f, 0xf86dc419,
        0x660951ba, 0x110e612c, 0x88073096, 0xff000000};

const UINT32 crcSeed = 0xeada2d49;

UINT32 compute(unsigned char *buffer, size_t len)
{
    UINT32 result = crcSeed;

    for (size_t i = 0; i < len; i++)
    {
        result =
            hashTable[((unsigned char)result) ^ ((unsigned char)buffer[i])] ^
            (result >> 8);
    }

    return result;
}

class Touchpad
{
public:
    int RawTrackingNum;
    bool IsActive;
    int ID;
    int X;
    int Y;

    Touchpad()
    {
        RawTrackingNum = 0;
        IsActive = false;
        ID = 0;
        X = 0;
        Y = 0;
    }
};

class Gyro
{
public:
    int X;
    int Y;
    int Z;

    Gyro()
    {
        X = 0;
        Y = 0;
        Z = 0;
    }
};

class Accelerometer
{
public:
    int X;
    int Y;
    int Z;
    uint32_t SensorTimestamp;

    Accelerometer()
    {
        X = 0;
        Y = 0;
        Z = 0;
        SensorTimestamp = 0;
    }
};

enum class BatteryState : uint8_t
{
    POWER_SUPPLY_STATUS_DISCHARGING = 0x0,
    POWER_SUPPLY_STATUS_CHARGING = 0x2,
    POWER_SUPPLY_STATUS_FULL = 0x1,
    POWER_SUPPLY_STATUS_NOT_CHARGING = 0xb,
    POWER_SUPPLY_STATUS_ERROR = 0xf,
    POWER_SUPPLY_TEMP_OR_VOLTAGE_OUT_OF_RANGE = 0xa,
    POWER_SUPPLY_STATUS_UNKNOWN = 0x0
};

struct Battery
{
    BatteryState State;
    int Level;

    Battery() : State(BatteryState::POWER_SUPPLY_STATUS_UNKNOWN), Level(0)
    {
    }
};

class ButtonState
{
public:
    bool square = false, triangle = false, circle = false, cross = false, 
     DpadUp = false, DpadDown = false, DpadLeft = false, DpadRight = false,
     L1 = false, L3 = false, R1 = false, R3 = false, R2Btn = false, 
     L2Btn = false, share = false, options = false, ps = false, 
     touchBtn = false, micBtn = false;
    uint8_t L2 = 0, R2 = 0;
    int RX = 0, RY = 0, LX = 0, LY = 0, TouchPacketNum = 0;
    std::string PathIdentifier = "";
    Touchpad trackPadTouch0;
    Touchpad trackPadTouch1;
    Gyro gyro;
    Accelerometer accelerometer;
    Battery battery;

    void SetDPadState(int dpad_state)
    {
        switch (dpad_state)
        {
        case 0:
            DpadUp = true;
            DpadDown = false;
            DpadLeft = false;
            DpadRight = false;
            break;

        case 1:
            DpadUp = true;
            DpadDown = false;
            DpadLeft = false;
            DpadRight = true;
            break;

        case 2:
            DpadUp = false;
            DpadDown = false;
            DpadLeft = false;
            DpadRight = true;
            break;

        case 3:
            DpadUp = false;
            DpadDown = true;
            DpadLeft = false;
            DpadRight = true;
            break;

        case 4:
            DpadUp = false;
            DpadDown = true;
            DpadLeft = false;
            DpadRight = false;
            break;

        case 5:
            DpadUp = false;
            DpadDown = true;
            DpadLeft = true;
            DpadRight = false;
            break;

        case 6:
            DpadUp = false;
            DpadDown = false;
            DpadLeft = true;
            DpadRight = false;
            break;

        case 7:
            DpadUp = true;
            DpadDown = false;
            DpadLeft = true;
            DpadRight = false;
            break;

        default:
            DpadUp = false;
            DpadDown = false;
            DpadLeft = false;
            DpadRight = false;
            break;
        }
    }
};

namespace Feature
{
enum MuteLight : uint8_t
{
    Off = 0,
    On,
    Breathing,
    DoNothing, // literally nothing, this input is ignored,
               // though it might be a faster blink in other versions
    NoAction4,
    NoAction5,
    NoAction6,
    NoAction7 = 7
};

enum class LightBrightness : uint8_t
{
    Bright = 0,
    Mid,
    Dim,
    BNoAction3,
    BNoAction4,
    BNoAction5,
    BNoAction6,
    BNoAction7 = 7
};

enum LightFadeAnimation : uint8_t
{
    Nothing = 0,
    FadeIn, // from black to blue
    FadeOut // from blue to black
};

enum RumbleMode : uint8_t
{
    StandardRumble = 0xFF,
    Haptic_Feedback = 0xFC
};

enum FeatureType : int8_t {
    Full = 0x57,
    LetGo = 0x08,
    BT_Init = 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x40,
};

enum AudioOutput : uint8_t
{
    Headset = 0x05,
    Speaker = 0x31
};

enum MicrophoneLED : uint8_t
{
    Pulse = 0x2,
    MLED_On = 0x1,
    MLED_Off = 0x0
};

enum MicrophoneStatus : uint8_t
{
    MSTATUS_On = 0,
    MSTATUS_Off = 0x10
};

enum Brightness : uint8_t
{
    High = 0x0,
    Medium = 0x1,
    Low = 0x2
};

enum PlayerLED : uint8_t
{
    PLAYER_OFF = 0x0,
    PLAYER_1 = 0x01,
    PLAYER_2 = 0x02,
    PLAYER_3 = 0x03,
    PLAYER_4 = 0x04,
    PLAYER_5 = 0x05,
};

enum DeviceType : uint8_t
{
    DualSense = 0,
    DualSense_Edge = 1,
    //DualShock4 = 2,
};

enum ConnectionType : uint8_t
{
    BT = 0x0,
    USB = 0x1
};
}; // namespace Feature

namespace Trigger
{
enum TriggerMode : uint8_t
{
    Off = 0x0,
    Rigid = 0x1,
    Pulse = 0x2,
    Rigid_A = 0x1 | 0x20,
    Rigid_B = 0x1 | 0x04,
    Rigid_AB = 0x1 | 0x20 | 0x04,
    Pulse_A = 0x2 | 0x20,
    Pulse_A2 = 35,
    Pulse_B = 0x2 | 0x04,
    Pulse_B2 = 38,
    Pulse_AB = 39,
    Calibration = 0xFC,
    // Official
    Feedback  = 0x21,
    Weapon    = 0x25, 
    Vibration = 0x26
};
}

namespace DualsenseUtils
{
    enum HapticFeedbackStatus {
        Working = 0,
        BluetoothNotUSB = 1,
        AudioDeviceNotFound = 2,
        AudioEngineNotInitialized = 3,
        Unknown = 4,
    };

class InputFeatures
{
public:
    Feature::RumbleMode VibrationType = Feature::Haptic_Feedback;
    Feature::FeatureType Features = Feature::FeatureType::Full;
    uint8_t _RightMotor = 0;
    uint8_t _LeftMotor = 0;
    int SpeakerVolume = 100;
    int MicrophoneVolume = 80;
    int HeadsetVolume = 0x7C;
    Feature::AudioOutput audioOutput = Feature::Speaker;
    Feature::MicrophoneLED micLED = Feature::MicrophoneLED::MLED_Off;
    Feature::MicrophoneStatus micStatus = Feature::MicrophoneStatus::MSTATUS_On;
    Trigger::TriggerMode RightTriggerMode = Trigger::TriggerMode::Off;
    Trigger::TriggerMode LeftTriggerMode = Trigger::TriggerMode::Off;
    int RightTriggerForces[11] = {0};
    int LeftTriggerForces[11] = {0};
    Feature::Brightness _Brightness = Feature::Brightness::High;
    Feature::PlayerLED playerLED = Feature::PlayerLED::PLAYER_OFF;
    unsigned char PlayerLED_Bitmask = 0x0;
    int Red = 0;
    int Green = 0;
    int Blue = 0;
    string ID = ""; // Optional ID

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["VibrationType"] = VibrationType;
        j["Features"] = Features;
        j["_RightMotor"] = _RightMotor;
        j["_LeftMotor"] = _LeftMotor;
        j["SpeakerVolume"] = SpeakerVolume;
        j["MicrophoneVolume"] = MicrophoneVolume;
        j["HeadsetVolume"] = HeadsetVolume;
        j["audioOutput"] = audioOutput;
        j["micLED"] = micLED;
        j["micStatus"] = micStatus;
        j["RightTriggerMode"] = RightTriggerMode;
        j["LeftTriggerMode"] = LeftTriggerMode;
        j["RightTriggerForces"] = std::vector<int>(std::begin(RightTriggerForces), std::end(RightTriggerForces));
        j["LeftTriggerForces"] = std::vector<int>(std::begin(LeftTriggerForces), std::end(LeftTriggerForces));
        j["_Brightness"] = _Brightness;
        j["playerLED"] = playerLED;
        j["PlayerLED_Bitmask"] = PlayerLED_Bitmask;
        j["Red"] = Red;
        j["Green"] = Green;
        j["Blue"] = Blue;
        j["ID"] = ID;
        return j;
    }

    static InputFeatures from_json(const nlohmann::json& j) {
        InputFeatures features;
        if (j.contains("VibrationType")) j.at("VibrationType").get_to(features.VibrationType);
        if (j.contains("Features")) j.at("Features").get_to(features.Features);
        if (j.contains("_RightMotor")) j.at("_RightMotor").get_to(features._RightMotor);
        if (j.contains("_LeftMotor")) j.at("_LeftMotor").get_to(features._LeftMotor);
        if (j.contains("SpeakerVolume")) j.at("SpeakerVolume").get_to(features.SpeakerVolume);
        if (j.contains("MicrophoneVolume")) j.at("MicrophoneVolume").get_to(features.MicrophoneVolume);
        if (j.contains("HeadsetVolume")) j.at("HeadsetVolume").get_to(features.HeadsetVolume);
        if (j.contains("audioOutput")) j.at("audioOutput").get_to(features.audioOutput);
        if (j.contains("micLED")) j.at("micLED").get_to(features.micLED);
        if (j.contains("micStatus")) j.at("micStatus").get_to(features.micStatus);
        if (j.contains("RightTriggerMode")) j.at("RightTriggerMode").get_to(features.RightTriggerMode);
        if (j.contains("LeftTriggerMode")) j.at("LeftTriggerMode").get_to(features.LeftTriggerMode);
        for (int i = 0; i < 7; ++i) {
            features.RightTriggerForces[i] = j.at("RightTriggerForces")[i];
            features.LeftTriggerForces[i] = j.at("LeftTriggerForces")[i];
        }
        if (j.contains("_Brightness")) j.at("_Brightness").get_to(features._Brightness);
        if (j.contains("playerLED")) j.at("playerLED").get_to(features.playerLED);
        if (j.contains("PlayerLED_Bitmask")) j.at("PlayerLED_Bitmask").get_to(features.PlayerLED_Bitmask);
        if (j.contains("Red")) j.at("Red").get_to(features.Red);
        if (j.contains("Green")) j.at("Green").get_to(features.Green);
        if (j.contains("Blue")) j.at("Blue").get_to(features.Blue);
        if (j.contains("ID")) j.at("ID").get_to(features.ID);
        return features;
    }

    bool operator==(const InputFeatures& other) const {
    return VibrationType == other.VibrationType &&
           Features == other.Features &&
           _RightMotor == other._RightMotor &&
           _LeftMotor == other._LeftMotor &&
           SpeakerVolume == other.SpeakerVolume &&
           MicrophoneVolume == other.MicrophoneVolume &&
           HeadsetVolume == other.HeadsetVolume &&
           audioOutput == other.audioOutput &&
           micLED == other.micLED &&
           micStatus == other.micStatus &&
           RightTriggerMode == other.RightTriggerMode &&
           LeftTriggerMode == other.LeftTriggerMode &&
           std::equal(std::begin(RightTriggerForces), std::end(RightTriggerForces), std::begin(other.RightTriggerForces)) &&
           std::equal(std::begin(LeftTriggerForces), std::end(LeftTriggerForces), std::begin(other.LeftTriggerForces)) &&
           _Brightness == other._Brightness &&
           playerLED == other.playerLED &&
           PlayerLED_Bitmask == other.PlayerLED_Bitmask &&
           Red == other.Red &&
           Green == other.Green &&
           Blue == other.Blue &&
           ID == other.ID;
    }
};

int GetControllerCount()
{
    int i = 0;
    // DualSense
    hid_device_info *info = hid_enumerate(0x054c, 0x0ce6);
    hid_device_info *cur = info;

    while (cur)
    {
        cur = cur->next;
        i++;
    }

    // DualSense Edge
    hid_device_info *info2 = hid_enumerate(0x054c, 0x0df2);
    hid_device_info *cur2 = info2;

    while (cur2)
    {
        cur2 = cur2->next;
        i++;
    }

    hid_free_enumeration(cur);
    hid_free_enumeration(info);
    hid_free_enumeration(cur2);
    hid_free_enumeration(info2);

    return i;
}

std::vector<std::string> EnumerateControllerIDs()
{
    std::vector<std::string> v; // Use std::string to manage memory automatically
    hid_device_info *initial = hid_enumerate(0x054c, 0x0ce6);
    hid_device_info *initialEdge = hid_enumerate(0x054c, 0x0df2);

    // DualSense
    if (initial != NULL)
    {
        hid_device_info *current = initial; // Use a temporary pointer to iterate
        while (current)
        {
            // Copying path into std::string to ensure proper memory management
            v.push_back(current->path); // emplace_back constructs the string in place
            current = current->next;
        }
        hid_free_enumeration(initial); // Free the original head
        hid_free_enumeration(current);
    }

    // DualSense Edge
    if (initialEdge != NULL)
    {
        hid_device_info *currentEdge = initialEdge; // Use a temporary pointer for iteration
        while (currentEdge)
        {
            // Copying path into std::string to ensure proper memory management
            v.push_back(currentEdge->path); // emplace_back constructs the string in place
            currentEdge = currentEdge->next;
        }
        hid_free_enumeration(initialEdge); // Free the original head
        hid_free_enumeration(currentEdge);
    }

    return v;
}

enum Reconnect_Result
{
    Reconnected = 0,
    Fail = 1,
    No_Change = 2
};
} // namespace DualsenseUtils

std::wstring ctow(const char *src)
{
    return std::wstring(src, src + strlen(src));
}

std::string convertDeviceId(const std::string &originalId)
{
    // Check for the expected prefix
    const std::string prefix = "\\\\?\\HID#";
    if (originalId.find(prefix) != 0)
    {
        std::cerr << "Invalid format: " << originalId << std::endl;
        return "";
    }

    // Remove the prefix
    std::string trimmedId = originalId.substr(prefix.length());

    // Split the ID at '#' to separate the main components
    size_t hashPos = trimmedId.find('#');
    if (hashPos == std::string::npos)
    {
        std::cerr << "Invalid format: " << originalId << std::endl;
        return "";
    }

    // Extract the first part (e.g., VID and PID)
    std::string mainPart = trimmedId.substr(0, hashPos);

    // Split to get the VID and PID
    std::string vidPid = mainPart; // Keep the VID and PID part

    // Find the next '#' for the instance part
    size_t nextHashPos = trimmedId.find('#', hashPos + 1);
    std::string instancePart =
        trimmedId.substr(hashPos + 1, nextHashPos - hashPos - 1); // Instance ID

    // Extract the last part (e.g., the GUID)
    std::string guidPart;
    if (nextHashPos != std::string::npos)
    {
        guidPart = trimmedId.substr(nextHashPos);
    }

    // Reconstruct the new device ID
    std::string newDeviceId = "USB\\" + vidPid + "\\" + instancePart;

    return newDeviceId;
}

// Function to replace "USB" with "HID"
std::wstring replaceUSBwithHID(const std::wstring &input)
{
    std::wstring result;
    size_t length = input.length();
    for (size_t i = 0; i < length; ++i)
    {
        // Check for the substring "USB"
        if (input.substr(i, 3) == L"USB")
        {
            result += L"HID"; // Replace "USB" with "HID"
            i += 2;           // Skip the next two characters of "USB"
        }
        else
        {
            result += input[i]; // Add the character as it is
        }
    }
    return result;
}

void trimTrailingWhitespace(std::wstring& str) {
    str.erase(std::find_if(str.rbegin(), str.rend(), [](wchar_t ch) {
        return ch != L'\0' && ch > 0x20; // Keep only non-null, printable characters
    }).base(), str.end());
}

void debugString(const std::wstring& str) {
    std::wcout << L"String content (size: " << str.size() << L"):" << std::endl;
    for (size_t i = 0; i < str.size(); ++i) {
        std::wcout << L"[" << i << L"]: '" << str[i] << L"' (0x" 
                   << std::hex << (int)str[i] << std::dec << L")" << std::endl;
    }
}

bool compareDeviceIDs(const std::wstring &id1, const std::wstring &id2)
{
    std::wregex pattern(LR"(MI_\d{2}|&\d{3}|&\d{2}(&|$))");

    // Remove the unwanted parts from both strings
    std::wstring strippedId1 = std::regex_replace(id1, pattern, L"");
    std::wstring strippedId2 = std::regex_replace(id2, pattern, L"");

    trimTrailingWhitespace(strippedId1);
    trimTrailingWhitespace(strippedId2);

    // Remove last four numbers from both string
    if (strippedId1.size() >= 4) {
        strippedId1.erase(strippedId1.size() - 4, 4);
    }

    if (strippedId2.size() >= 4) {
        strippedId2.erase(strippedId2.size() - 4, 4);
    }

    // Compare the stripped IDs
    wcout << "Comparing -> " << strippedId1 << "size " << strippedId1.size() << " size " << strippedId2.size() << " to " << strippedId2 << endl;
    return strippedId1 == strippedId2;
}

class Peaks {
public:
    float channelZero = 0; // used for left headset headphone
    float Speaker = 0;
    float LeftActuator = 0;
    float RightActuator = 0;
};

class Dualsense
{
private:
    std::chrono::high_resolution_clock::time_point LastTimeReconnected = std::chrono::high_resolution_clock::now();
    std::chrono::steady_clock::time_point lastPlayTime;
    std::chrono::milliseconds cooldownDuration{300}; // Cooldown duration (1 second)

    static void data_callback(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
        float* pInputSamples = (float*)input;
        Dualsense* self = (Dualsense*)device->pUserData;

        // Reset peaks for this callback
        self->peaks.channelZero = 0.0f;
        self->peaks.Speaker = 0.0f;
        self->peaks.LeftActuator = 0.0f;
        self->peaks.RightActuator = 0.0f;

        for (ma_uint32 i = 0; i < frameCount; ++i) {
            for (int channel = 0; channel < 4; ++channel) {
                // Correct access pattern for 4 channels
                float sample = pInputSamples[i * 4 + channel];

                // Apply noise gate
                if (fabs(sample) > 0.01f) {
                    // Update peak values based on channel
                    if (channel == 0) {
                        if (sample > self->peaks.channelZero) {
                            self->peaks.channelZero = sample; // Update peak for channel zero
                        }
                    } else if (channel == 1) {
                        if (sample > self->peaks.Speaker) {
                            self->peaks.Speaker = sample; // Update peak for Speaker
                        }
                    } else if (channel == 2) {
                        if (sample > self->peaks.LeftActuator) {
                            self->peaks.LeftActuator = sample; // Update peak for Left Actuator
                        }
                    } else if (channel == 3) {
                        if (sample > self->peaks.RightActuator) {
                            self->peaks.RightActuator = sample; // Update peak for Right Actuator
                        }
                    }
                }
            } 
        }
    }

    static void data_callback_playback(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
        float* pOutputSamples = (float*)output;
        Dualsense* self = (Dualsense*)device->pUserData;

        if (self->TouchpadToHaptics && self->State.trackPadTouch0.IsActive)         
        {
            static float phase = 0.0f; 
            float normalizedX = self->State.trackPadTouch0.X / 1979.0f;
            const float phaseIncrement = (2.0f * 3.14 * self->TouchpadToHapticsFrequency) / 48000.0f;
            float channel3Amplitude = 1.0f - normalizedX;
            float channel4Amplitude = normalizedX;

            for (ma_uint32 frame = 0; frame < frameCount; ++frame) {
                float sineValue = sinf(phase);

                phase += phaseIncrement;
                if (phase >= 2.0f * 3.14) {
                    phase -= 2.0f * 3.14;
                }

                pOutputSamples[frame * 4 + 2] = channel3Amplitude * sineValue;
                pOutputSamples[frame * 4 + 3] = channel4Amplitude * sineValue;

                pOutputSamples[frame * 4 + 0] = 0.0f; 
                pOutputSamples[frame * 4 + 1] = 0.0f;
            }
        }
    }

    int offset = 0;
    uint8_t connectionType;
    bool Running = false;
    const wchar_t *DefaultError = L"Success";
    volatile const wchar_t *error = L"";
    int res = 0;
    std::string path = "";
    std::string instanceid = "";
    wstring lastKnownParent;
    hid_device *handle;
    bool bt_initialized = false;
    bool AudioInitialized = false;
    bool AudioDeviceNotFound = false;
    bool WasConnectedAtLeastOnce = false;
    bool DisconnectedByError = false;
    bool FirstTimeWrite = true;
    wstring AudioDeviceInstanceID;
    const char* AudioDeviceName;

    DualsenseUtils::InputFeatures CurSettings;
    DualsenseUtils::InputFeatures LastSettings;

    // Audio variables
    Peaks peaks;
    ma_context context;
    ma_result result;
    ma_engine engine;
    ma_device device;
    ma_decoder decoder;
    ma_device deviceLoopback;
    std::map<std::string, shared_ptr<ma_sound>> soundMap;
    std::map<std::string, shared_ptr<ma_sound>> CurrentlyPlayedSounds;

public:
    bool Connected = false;
    bool TouchpadToHaptics = false;
    float TouchpadToHapticsFrequency = 50.0f;
    ButtonState State;
    Dualsense(std::string Path = "")
    {
        if (Path != "")
        {
            handle = hid_open_path(Path.c_str());

            if (!handle)
            {
                path = "";
            }
            else
            {
                DefaultError = hid_error(handle);
                hid_device_info *info = hid_get_device_info(handle);

                if (info->bus_type == 1) // USB
                {
                    connectionType = Feature::USB;
                    offset = 0;
                }
                else if (info->bus_type == 2) // Bluetooth
                {
                    connectionType = Feature::BT;
                    offset = 1;
                }

                wstring parent;
                string stringPath(Path);
                string convertedPath = convertDeviceId(stringPath);
                instanceid = convertedPath;

                if (connectionType == Feature::USB)
                {
                    wstring finalS = ctow(convertedPath.c_str());

                    int statusf =
                        FindParentDeviceInstanceId(replaceUSBwithHID(finalS),
                                                   L".*",
                                                   parent);
                    wcout << "Device parent: " << parent << endl;
                    lastKnownParent = parent;
                }
                else
                {
                    cout << "Bluetooth connection detected." << endl;
                }

                std::wcout << L"Vendor ID: " << info->manufacturer_string
                           << "\nPath: " << info->path << std::endl;

                Running = true;
                Connected = true;
                hid_free_enumeration(info);
            }

            path = Path;
            FirstTimeWrite = true;
        }
    };

    bool LoadSound(const std::string& soundName, const std::string& filePath) {
        if (connectionType == Feature::USB && AudioInitialized && !AudioDeviceNotFound) {
            if (soundMap.find(soundName) != soundMap.end()) {
                return false; // Return if already loaded
            }

            // Allocate memory for a new sound
            shared_ptr<ma_sound> sound = make_shared<ma_sound>();
            ma_result result = ma_sound_init_from_file(&engine, filePath.c_str(), 0, NULL, NULL, sound.get());

            if (result == MA_SUCCESS) {
                soundMap[soundName] = sound; // Store the sound in the map
                std::cout << "Loaded -> " << filePath << std::endl;
                return true;
            } else {
                std::cerr << "Failed to load " << filePath << ". Reason: " << result << std::endl;

                // Ensure proper cleanup
                ma_sound_uninit(sound.get());

                return false;
            }
        }
        return false;
    }

    Peaks GetAudioPeaks() {
        return peaks;
    }

    wstring GetKnownAudioParentInstanceID()
    {
        return lastKnownParent;
    }

    wstring GetAudioDeviceInstanceID() {
        return AudioDeviceInstanceID;
    }

    std::string GetInstanceID() {
        return instanceid;
    }

    const char* GetAudioDeviceName() {
        return AudioDeviceName;
    }

    void Read()
    {
        if (handle && Connected)
        {
            unsigned char ButtonStates[MAX_READ_LENGTH];
            ButtonState buttonState;

            res = hid_read(handle, ButtonStates, MAX_READ_LENGTH);
            if (res < 0) {
                Connected = false;
            }
            else {
                Connected = true;
            }

            buttonState.LX = ButtonStates[1 + offset];
            buttonState.LY = ButtonStates[2 + offset];
            buttonState.RX = ButtonStates[3 + offset];
            buttonState.RY = ButtonStates[4 + offset];
            buttonState.L2 = ButtonStates[5 + offset];
            buttonState.R2 = ButtonStates[6 + offset];

            uint8_t buttonButtonState = ButtonStates[8 + offset];
            buttonState.triangle = (buttonButtonState & (1 << 7)) != 0;
            buttonState.circle = (buttonButtonState & (1 << 6)) != 0;
            buttonState.cross = (buttonButtonState & (1 << 5)) != 0;
            buttonState.square = (buttonButtonState & (1 << 4)) != 0;

            uint8_t dpad_ButtonState = (uint8_t)(buttonButtonState & 0x0F);
            buttonState.SetDPadState(dpad_ButtonState);

            uint8_t misc = ButtonStates[9 + offset];
            buttonState.R3 = (misc & (1 << 7)) != 0;
            buttonState.L3 = (misc & (1 << 6)) != 0;
            buttonState.options = (misc & (1 << 5)) != 0;
            buttonState.share = (misc & (1 << 4)) != 0;
            buttonState.R2Btn = (misc & (1 << 3)) != 0;
            buttonState.L2Btn = (misc & (1 << 2)) != 0;
            buttonState.R1 = (misc & (1 << 1)) != 0;
            buttonState.L1 = (misc & (1 << 0)) != 0;

            uint8_t misc2 = ButtonStates[10 + offset];
            buttonState.ps = (misc2 & (1 << 0)) != 0;
            buttonState.touchBtn = (misc2 & 0x02) != 0;
            buttonState.micBtn = (misc2 & 0x04) != 0;

            buttonState.trackPadTouch0.RawTrackingNum =
                (uint8_t)(ButtonStates[33 + offset]);
            buttonState.trackPadTouch0.ID =
                (uint8_t)(ButtonStates[33 + offset] & 0x7F);
            buttonState.trackPadTouch0.IsActive =
                (ButtonStates[33 + offset] & 0x80) == 0;
            buttonState.trackPadTouch0.X =
                ((ButtonStates[35 + offset] & 0x0F) << 8) |
                ButtonStates[34 + offset];
            buttonState.trackPadTouch0.Y =
                ((ButtonStates[36 + offset]) << 4) |
                ((ButtonStates[35 + offset] & 0xF0) >> 4);

            buttonState.trackPadTouch1.RawTrackingNum =
                (uint8_t)(ButtonStates[37 + offset]);
            buttonState.trackPadTouch1.ID =
                (uint8_t)(ButtonStates[37 + offset] & 0x7F);
            buttonState.trackPadTouch1.IsActive =
                (ButtonStates[37 + offset] & 0x80) == 0;
            buttonState.trackPadTouch1.X =
                ((ButtonStates[39 + offset] & 0x0F) << 8) |
                ButtonStates[38 + offset];
            buttonState.trackPadTouch1.Y =
                ((ButtonStates[40 + offset]) << 4) |
                ((ButtonStates[39 + offset] & 0xF0) >> 4);
            buttonState.TouchPacketNum = (uint8_t)(ButtonStates[41 + offset]);

            buttonState.accelerometer.X =
                bit_cast<int16_t>(array<uint8_t, 2>{ButtonStates[16 + offset],
                                                    ButtonStates[17 + offset]});
            buttonState.accelerometer.Y =
                bit_cast<int16_t>(array<uint8_t, 2>{ButtonStates[18 + offset],
                                                    ButtonStates[19 + offset]});
            buttonState.accelerometer.Z =
                bit_cast<int16_t>(array<uint8_t, 2>{ButtonStates[20 + offset],
                                                    ButtonStates[21 + offset]});

            buttonState.gyro.X =
                bit_cast<int16_t>(array<uint8_t, 2>{ButtonStates[22 + offset],
                                                    ButtonStates[23 + offset]});
            buttonState.gyro.Y =
                bit_cast<int16_t>(array<uint8_t, 2>{ButtonStates[24 + offset],
                                                    ButtonStates[25 + offset]});
            buttonState.gyro.Z =
                bit_cast<int16_t>(array<uint8_t, 2>{ButtonStates[26 + offset],
                                                    ButtonStates[27 + offset]});

            buttonState.accelerometer.SensorTimestamp =
                (static_cast<uint32_t>(ButtonStates[28 + offset])) |
                (static_cast<uint32_t>(ButtonStates[29 + offset]) << 8) |
                static_cast<uint32_t>(ButtonStates[30 + offset] << 16);

            buttonState.battery.State =
                (BatteryState)((uint8_t)(ButtonStates[53 + offset] & 0xF0) >>
                               4);
            buttonState.battery.Level =
                min((int)((ButtonStates[53 + offset] & 0x0F) * 10 + 5), 100);

            buttonState.PathIdentifier = path;

            State = buttonState;
        }
        else
        {
            State.LX = 128;
            State.LY = 128;
            State.RX = 128;
            State.RY = 128;
            State.square = false;
            State.triangle = false;
            State.circle = false;
            State.cross = false;
            State.DpadUp = false;
            State.DpadDown = false;
            State.DpadLeft = false;
            State.DpadRight = false;
            State.L1 = false;
            State.L3 = false;
            State.R1 = false;
            State.R3 = false;
            State.R2Btn = false;
            State.L2Btn = false;
            State.share = false;
            State.options = false;
            State.ps = false;
            State.touchBtn = false;
            State.micBtn = false;
            State.L2 = 0;
            State.R2 = 0;
            State.trackPadTouch0.IsActive = false;
            State.trackPadTouch1.IsActive = false;
            State.battery.State = BatteryState::POWER_SUPPLY_STATUS_UNKNOWN;
            State.battery.Level = 0;
        }
    }

    bool Write()
    {       
        if (CurSettings == LastSettings && !FirstTimeWrite)
        {          
            if (error != DefaultError && error != L"")
            {
                Connected = false;
                bt_initialized = false;
                FirstTimeWrite = true;
            }
            return false;
        }

        if (handle != nullptr && Connected)
        {
            // Update the previous settings with the current ones
            LastSettings = CurSettings;

            if (connectionType == Feature::USB)
            {
                uint8_t outReport[MAX_USB_WRITE_LENGTH];
                outReport[0] = 2;
                outReport[1] = CurSettings.VibrationType;
                outReport[2] = CurSettings.Features;
                outReport[3] = CurSettings._RightMotor;
                outReport[4] = CurSettings._LeftMotor;
                outReport[5] = CurSettings.HeadsetVolume;
                outReport[6] = (unsigned char)CurSettings.SpeakerVolume;
                outReport[7] = CurSettings.MicrophoneVolume;
                outReport[8] = CurSettings.audioOutput;
                outReport[9] = CurSettings.micLED;
                outReport[10] = CurSettings.micStatus;

                outReport[11] = CurSettings.RightTriggerMode;
                outReport[12] = CurSettings.RightTriggerForces[0];
                outReport[13] = CurSettings.RightTriggerForces[1];
                outReport[14] = CurSettings.RightTriggerForces[2];
                outReport[15] = CurSettings.RightTriggerForces[3];
                outReport[16] = CurSettings.RightTriggerForces[4];
                outReport[17] = CurSettings.RightTriggerForces[5];
                outReport[18] = CurSettings.RightTriggerForces[6];
                outReport[19] = CurSettings.RightTriggerForces[7];
                outReport[20] = CurSettings.RightTriggerForces[8];
                outReport[21] = CurSettings.RightTriggerForces[9];

                outReport[22] = CurSettings.LeftTriggerMode;
                outReport[23] = CurSettings.LeftTriggerForces[0];
                outReport[24] = CurSettings.LeftTriggerForces[1];
                outReport[25] = CurSettings.LeftTriggerForces[2];
                outReport[26] = CurSettings.LeftTriggerForces[3];
                outReport[27] = CurSettings.LeftTriggerForces[4];
                outReport[28] = CurSettings.LeftTriggerForces[5];
                outReport[29] = CurSettings.LeftTriggerForces[6];
                outReport[30] = CurSettings.LeftTriggerForces[7];
                outReport[31] = CurSettings.LeftTriggerForces[8];
                outReport[32] = CurSettings.LeftTriggerForces[9];

                outReport[33] = 0;
                outReport[34] = 0;
                outReport[35] = 0;
                outReport[36] = 0;

                uint8_t triggerReduction = 0x0;
                uint8_t rumbleReduction = 0x0;

                outReport[37] = (rumbleReduction << 4) | (triggerReduction & 0x0F);

                outReport[38] = 0;
                outReport[39] = 0;
                outReport[40] = 0;
                outReport[41] = 0;
                outReport[42] = CurSettings.micLED;
                outReport[43] = CurSettings._Brightness;
                outReport[44] = CurSettings.PlayerLED_Bitmask;
                outReport[45] = CurSettings.Red;
                outReport[46] = CurSettings.Green;
                outReport[47] = CurSettings.Blue;

                res = hid_write(handle, outReport, MAX_USB_WRITE_LENGTH);
                if (res < 0) {
                    Connected = false;
                    FirstTimeWrite = true;
                }
                else {
                    Connected = true;
                    FirstTimeWrite = false;
                    return true;
                }
            }
            else if (connectionType == Feature::BT)
            {
                uint8_t outReport[MAX_BT_WRITE_LENGTH];
                outReport[0] = 0x31;
                outReport[1] = 2;
                outReport[2] = CurSettings.VibrationType;
                if (bt_initialized == false)
                {                 
                        outReport[3] = Feature::FeatureType::BT_Init;
                        bt_initialized = true;
                        const UINT32 crcChecksum = compute(outReport, 74);

                        outReport[0x4A] = (unsigned char)((crcChecksum & 0x000000FF) >> 0UL);
                        outReport[0x4B] = (unsigned char)((crcChecksum & 0x0000FF00) >> 8UL);
                        outReport[0x4C] = (unsigned char)((crcChecksum & 0x00FF0000) >> 16UL);
                        outReport[0x4D] = (unsigned char)((crcChecksum & 0xFF000000) >> 24UL);

                        error = hid_error(handle);
                        if(error == DefaultError)
                        {
                            res = hid_write(handle, outReport, MAX_BT_WRITE_LENGTH);
                            if (res < 0) {
                                Connected = false;
                                FirstTimeWrite = true;
                            }
                            else {
                                FirstTimeWrite = false;
                            }                         
                        }
                }

                outReport[3] = CurSettings.Features;              
                outReport[4] = CurSettings._RightMotor;
                outReport[5] = CurSettings._LeftMotor;
                outReport[6] = CurSettings.HeadsetVolume;
                outReport[7] = (unsigned char)CurSettings.SpeakerVolume;
                outReport[8] = CurSettings.MicrophoneVolume;
                outReport[9] = CurSettings.audioOutput;
                outReport[10] = CurSettings.micLED;
                outReport[11] = CurSettings.micStatus;

                outReport[12] = CurSettings.RightTriggerMode;
                outReport[13] = CurSettings.RightTriggerForces[0];
                outReport[14] = CurSettings.RightTriggerForces[1];
                outReport[15] = CurSettings.RightTriggerForces[2];
                outReport[16] = CurSettings.RightTriggerForces[3];
                outReport[17] = CurSettings.RightTriggerForces[4];
                outReport[18] = CurSettings.RightTriggerForces[5];
                outReport[19] = CurSettings.RightTriggerForces[6];
                outReport[20] = CurSettings.RightTriggerForces[7];
                outReport[21] = CurSettings.RightTriggerForces[8];
                outReport[22] = CurSettings.RightTriggerForces[9];

                outReport[23] = CurSettings.LeftTriggerMode;
                outReport[24] = CurSettings.LeftTriggerForces[0];
                outReport[25] = CurSettings.LeftTriggerForces[1];
                outReport[26] = CurSettings.LeftTriggerForces[2];
                outReport[27] = CurSettings.LeftTriggerForces[3];
                outReport[28] = CurSettings.LeftTriggerForces[4];
                outReport[29] = CurSettings.LeftTriggerForces[5];
                outReport[30] = CurSettings.LeftTriggerForces[6];
                outReport[31] = CurSettings.LeftTriggerForces[7];
                outReport[32] = CurSettings.LeftTriggerForces[8];
                outReport[33] = CurSettings.LeftTriggerForces[9];

                outReport[34] = 0;
                outReport[35] = 0;
                outReport[36] = 0;
                outReport[37] = 0;
                outReport[38] = 0;
                outReport[39] = 0;
                outReport[40] = CurSettings._Brightness;
                outReport[43] = CurSettings._Brightness;
                outReport[44] = CurSettings.micLED;
                outReport[45] = CurSettings.PlayerLED_Bitmask;
                outReport[46] = CurSettings.Red;
                outReport[47] = CurSettings.Green;
                outReport[48] = CurSettings.Blue;

                const UINT32 crcChecksum = compute(outReport, 74);

                outReport[0x4A] =
                    (unsigned char)((crcChecksum & 0x000000FF) >> 0UL);
                outReport[0x4B] =
                    (unsigned char)((crcChecksum & 0x0000FF00) >> 8UL);
                outReport[0x4C] =
                    (unsigned char)((crcChecksum & 0x00FF0000) >> 16UL);
                outReport[0x4D] =
                    (unsigned char)((crcChecksum & 0xFF000000) >> 24UL);

                try
                {
                    error = hid_error(handle);
                    if(error == DefaultError)
                    {
                        res = hid_write(handle, outReport, MAX_BT_WRITE_LENGTH);
                        if (res < 0) {
                            Connected = false;
                        }
                        else {
                            return true;
                        }
                    }
                }
                catch (...)
                {
                    Connected = false;
                    FirstTimeWrite = true;
                }
            }
        }
        else
        {
            Connected = false;
        }

        return false;
    }

    std::string GetPath()
    {
        return path;
    }

    Feature::ConnectionType GetConnectionType()
    {
        return (Feature::ConnectionType)connectionType;
    }

    void InitializeAudioEngine()
    {
        std::chrono::high_resolution_clock::time_point Now = std::chrono::high_resolution_clock::now();
        if ((Now - LastTimeReconnected) < 2s){
            return;
        }

        if (!AudioInitialized && !AudioDeviceNotFound && connectionType == Feature::USB && Connected)
        {
            CComPtr<IMMDeviceEnumerator> pEnumerator;
            CComPtr<IMMDeviceCollection> pDevices;
            CComPtr<IMMDevice> pDevice;
            CComPtr<IPropertyStore> pProps;
            PROPVARIANT varName;
            HRESULT hr = CoInitialize(nullptr); // Initialize COM library

            if (FAILED(hr))
            {
                std::wcerr << L"Failed to initialize COM library." << std::endl;
            }

            hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                  nullptr,
                                  CLSCTX_ALL,
                                  IID_PPV_ARGS(&pEnumerator));
            if (FAILED(hr))
            {
                std::wcerr << L"Failed to create IMMDeviceEnumerator."
                           << std::endl;
            }

            hr = pEnumerator->EnumAudioEndpoints(eRender,
                                                 DEVICE_STATE_ACTIVE,
                                                 &pDevices);
            if (FAILED(hr))
            {
                std::wcerr << L"Failed to enumerate audio endpoints: "
                           << std::hex << hr << std::endl;
            }

            UINT count;
            hr = pDevices->GetCount(&count);
            if (FAILED(hr))
            {
                std::wcerr << L"Failed to get device count: " << std::hex << hr
                           << std::endl;
            }

            std::wcout << L"Number of devices: " << count << std::endl;

            LPWSTR ID;
            bool found = false;
            for (UINT i = 0; i < count; ++i)
            {
                pDevice
                    .Release(); // Make sure to release any previous reference
                hr = pDevices->Item(i, &pDevice);
                if (FAILED(hr) || pDevice == nullptr)
                {
                    std::wcerr << L"Failed to get device at index " << i
                               << L": " << std::hex << hr << std::endl;
                    continue;
                }
                else
                {
                    // Open the property store of the device
                    CComPtr<IPropertyStore> pProps;
                    hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
                    hr = pDevice->GetId(&ID);

                    if (FAILED(hr))
                    {
                        std::wcerr << L"Failed to open property store for "
                                      L"device at index "
                                   << i << L": " << std::hex << hr << std::endl;
                        continue;
                    }
                    else
                    {
                        wstring correctInstanceID =
                            L"SWD\\MMDEVAPI\\" + wstring(ID);
                        wstring foundID;
                        FindParentDeviceInstanceId(correctInstanceID,
                                                   L".*",
                                                   foundID);

                        if (compareDeviceIDs(foundID, lastKnownParent))
                        {
                            wcout << "Found matching parents for dualsense "
                                     "speaker: "
                                  << foundID << endl;
                            found = true;;
                            AudioDeviceInstanceID = foundID;
                            break;
                        }
                        else {
                            wcout << "Wrong device: " << foundID << endl;
                        }
                    }
                }
            }

            if (!found)
            {
                AudioInitialized = false;
                AudioDeviceNotFound = true;
                return;
            }
            else {
                AudioDeviceNotFound = false;
            }

            if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS)
            {
                // Error.
            }

            ma_device_info *pPlaybackInfos;
            ma_uint32 playbackCount;
            ma_device_info *pCaptureInfos;
            ma_uint32 captureCount;
            if (ma_context_get_devices(&context,
                                       &pPlaybackInfos,
                                       &playbackCount,
                                       &pCaptureInfos,
                                       &captureCount) != MA_SUCCESS)
            {
                // Error.
            }

            int deviceIndex = 0;
            found = false;
            for (ma_uint32 iDevice = 0; iDevice < playbackCount; iDevice += 1)
            {
                std::string name = pPlaybackInfos[iDevice].name;

                ma_device_info info;
                ma_result r = ma_context_get_device_info__wasapi(
                    &context,
                    ma_device_type_playback,
                    &pPlaybackInfos[iDevice].id,
                    &info);

                if (wcscmp(info.id.wasapi, ID) == 0)
                {
                    wcout << "Selected audio device: " << info.id.wasapi
                          << " Index:" << deviceIndex << endl;

                    found = true;
                    break;
                }
                else
                {
                    wcout << "Invalid wasapi id: " << info.id.wasapi << endl;
                    deviceIndex++;
                }
            }

            if (!found)
            {
                AudioInitialized = false;
                AudioDeviceNotFound = true;
                return;
            }
            else {
                AudioDeviceNotFound = false;
            }

                ma_channel channel;

                ma_standard_channel_map channelInMap;

                ma_channel_converter_config channelConverterConfig =
                    ma_channel_converter_config_init(
                        ma_format_f32,
                        2,
                        NULL,
                        4,
                        NULL,
                        ma_channel_mix_mode_default);
                ma_channel_converter channelConverter;
                channelConverter.channelsIn = 2;
                channelConverter.channelsOut = 4;
                ma_channel_conversion_path cpath;

                ma_result channelConverterResult =
                    ma_channel_converter_init(&channelConverterConfig,
                                              NULL,
                                              &channelConverter);

                if (channelConverterResult != MA_SUCCESS)
                {
                    cout << "Creating channel converter failed!" << endl;
                }

                ma_device_config deviceconfigLoopback = ma_device_config_init(ma_device_type_loopback);
                deviceconfigLoopback.capture.pDeviceID = &pPlaybackInfos[deviceIndex].id;
                deviceconfigLoopback.capture.format = ma_format_f32;
                deviceconfigLoopback.capture.channels = 0;
                deviceconfigLoopback.capture.channelMixMode = ma_channel_mix_mode_default;
                deviceconfigLoopback.capture.shareMode = ma_share_mode_shared;
                deviceconfigLoopback.playback.pDeviceID = &pPlaybackInfos[deviceIndex].id;
                deviceconfigLoopback.playback.format = ma_format_f32;
                deviceconfigLoopback.playback.channels = 0;
                deviceconfigLoopback.playback.channelMixMode = ma_channel_mix_mode_default;
                deviceconfigLoopback.sampleRate = 48000;
                deviceconfigLoopback.playback.shareMode = ma_share_mode_shared;
                deviceconfigLoopback.pUserData = this;
                deviceconfigLoopback.dataCallback = data_callback;

                ma_device_config deviceconfig = ma_device_config_init(ma_device_type_playback);
                deviceconfig.capture.pDeviceID = &pPlaybackInfos[deviceIndex].id;
                deviceconfig.capture.format = ma_format_f32;
                deviceconfig.capture.channels = 0;
                deviceconfig.capture.channelMixMode = ma_channel_mix_mode_default;
                deviceconfig.capture.shareMode = ma_share_mode_shared;
                deviceconfig.playback.pDeviceID = &pPlaybackInfos[deviceIndex].id;
                deviceconfig.playback.format = ma_format_f32;
                deviceconfig.playback.channels = 0;
                deviceconfig.playback.channelMixMode = ma_channel_mix_mode_default;
                deviceconfig.sampleRate = 48000;
                deviceconfig.playback.shareMode = ma_share_mode_shared;
                deviceconfig.pUserData = this;
                deviceconfig.dataCallback = data_callback_playback;
                

                if (ma_device_init(NULL, &deviceconfig, &device) != MA_SUCCESS)
                {
                    cout << "Failed to initialize audio device!" << endl;
                }
                else
                {
                    AudioDeviceName = pPlaybackInfos[deviceIndex].name;
                    cout << "Audio device intialized" << endl;
                }

                if (ma_device_init(NULL, &deviceconfigLoopback, &deviceLoopback) != MA_SUCCESS)
                {
                    cout << "Failed to initialize loopback device!" << endl;
                }
                else
                {
                    cout << "Audio loopback intialized" << endl;
                }

                ma_device_start(&device);
                ma_device_start(&deviceLoopback);

                ma_engine_config config = ma_engine_config_init();
                //config.pContext = &context;
                config.pPlaybackDeviceID = &pPlaybackInfos[deviceIndex].id;
                config.channels = 0;
                config.sampleRate = 48000;
                config.monoExpansionMode = ma_mono_expansion_mode_duplicate;

                result = ma_engine_init(&config, &engine);
                if (result != MA_SUCCESS)
                {
                    return;
                }
                else
                {
                    cout << "Sound engine intialized" << endl;
                }
          
                AudioInitialized = true;
                CoUninitialize();           
        }
    }

    DualsenseUtils::HapticFeedbackStatus GetHapticFeedbackStatus() {
        if (connectionType == Feature::ConnectionType::BT) {
            return DualsenseUtils::HapticFeedbackStatus::BluetoothNotUSB;
        }

        if (AudioDeviceNotFound) {
            return DualsenseUtils::HapticFeedbackStatus::AudioDeviceNotFound;
        }

        if (!AudioInitialized) {
            return DualsenseUtils::HapticFeedbackStatus::AudioEngineNotInitialized;
        }

        return DualsenseUtils::HapticFeedbackStatus::Working;
    }

    
    static void ma_sound_end_proc(void* pUserData, ma_sound* pSound) {
        auto Loop = static_cast<bool>(pUserData);
    }
    

    bool SetSoundVolume(const std::string& soundName, float Volume) {
        if (connectionType == Feature::USB && AudioInitialized && !AudioDeviceNotFound)
        {
            auto it = CurrentlyPlayedSounds.find(soundName);
            if (it == CurrentlyPlayedSounds.end()) {
                return false;
            }

            ma_sound_set_volume(it->second.get(), Volume);
            return true;
        }

        return false;
    }

    bool SetSoundPitch(const std::string& soundName, float Pitch) {
        if (connectionType == Feature::USB && AudioInitialized && !AudioDeviceNotFound)
        {
            auto it = CurrentlyPlayedSounds.find(soundName);
            if (it == CurrentlyPlayedSounds.end()) {
                return false;
            }

            ma_sound_set_pitch(it->second.get(), Pitch);
            return true;
        }

        return false;
    }

    void StopAllHaptics() {
        for (const auto& [key, value] : CurrentlyPlayedSounds) {
            ma_sound_stop(value.get());
            ma_sound_set_looping(value.get(), false);
            CurrentlyPlayedSounds.erase(key);
        }      
    }

    void StopSoundsThatStartWith(const std::string& String) {
        for (const auto& [key, value] : CurrentlyPlayedSounds) {
            if (key.rfind(String, 0) == 0) { // rfind with pos=0 checks the prefix
                ma_sound_stop(value.get());
                ma_sound_set_looping(value.get(), false);
                CurrentlyPlayedSounds.erase(key);
            }
        }
    }

    bool StopHaptics(const std::string& soundName) {
        if (connectionType == Feature::USB && AudioInitialized && !AudioDeviceNotFound)
        {
            auto it = CurrentlyPlayedSounds.find(soundName);
            if (it == CurrentlyPlayedSounds.end()) {
                return false;
            }

            ma_sound_stop(it->second.get());
            return true;
        }

        return false;
    }


    bool PlayHaptics(const std::string& soundName, bool DontPlayIfAlreadyPlaying = false, bool LoopUntilStoppedManually = false) {
        if (connectionType == Feature::USB && AudioInitialized && !AudioDeviceNotFound) {
            auto it = soundMap.find(soundName);
            if (it == soundMap.end()) {
                cout << "sound not found" << endl;
                return false;
            }

            std::shared_ptr<ma_sound> originalSound = it->second;

            // Return if the sound is already playing and DontPlayIfAlreadyPlaying is set to true
            auto it2 = CurrentlyPlayedSounds.find(soundName);
            if (it2 != CurrentlyPlayedSounds.end()) {
                if (ma_sound_is_playing(it2->second.get()) || ma_sound_is_looping(it2->second.get())) {
                    if (DontPlayIfAlreadyPlaying) {
                        return false;
                    }
                }
            }

            // Create a new instance of the sound with a custom deleter
            auto deleter = [](ma_sound* sound) {
                if (sound) {
                    ma_sound_stop(sound);
                    ma_sound_uninit(sound);
                    delete sound;
                }
            };

            std::shared_ptr<ma_sound> newSound(new ma_sound, deleter);
            ma_result result = ma_sound_init_copy(&engine, originalSound.get(), 0, NULL, newSound.get());
            if (result != MA_SUCCESS) {
                soundMap.erase(it);
                std::cerr << "Failed to create a new sound instance. Error: " << result << std::endl;
                return false;
            }


            // Add to the currently played sounds map
            //newSound.get()->endCallback = ma_sound_end_proc;

            if (LoopUntilStoppedManually) {
                ma_sound_set_looping(newSound.get(), true);
            }

            CurrentlyPlayedSounds[soundName] = newSound;

            // Start the sound
            ma_result soundplay = ma_sound_start(newSound.get());
            if (soundplay == MA_SUCCESS) {
                return true;
            } else {
                std::cerr << "Error playing sound: " << static_cast<int>(soundplay) << std::endl;

                // Remove from the map (if it was already added)
                CurrentlyPlayedSounds.erase(soundName);

                return false;
            }
        }

        return false;
    }


    DualsenseUtils::Reconnect_Result Reconnect(bool SamePort = true)
    {
        if (!Connected)
        {
            FirstTimeWrite = true;
            bt_initialized = false;
            if (SamePort && path != "")
            {
                handle = hid_open_path(path.c_str());
            }
            else
            {
                handle = hid_open(0x054c, 0x0ce6, NULL);
            }

            if (!handle)
            {
                hid_close(handle);
                
                Connected = false;
                return DualsenseUtils::Reconnect_Result::Fail;
            }
            else
            {
                Connected = true;
                if (AudioInitialized) {
                    ma_device_uninit(&device);
                    ma_device_uninit(&deviceLoopback);
                    ma_engine_uninit(&engine);
                    ma_context_uninit(&context);
                    AudioInitialized = false;
                }

                hid_device_info *info = hid_get_device_info(handle);

                if (info->bus_type == 1) // USB
                {
                    connectionType = Feature::USB;
                    offset = 0;
                }
                else if (info->bus_type == 2) // Bluetooth
                {
                    connectionType = Feature::BT;
                    offset = 1;
                }

                if (connectionType == Feature::USB)
                {
                    wstring parent;
                    string stringPath(path);
                    string convertedPath = convertDeviceId(stringPath);
                    wstring finalS = ctow(convertedPath.c_str());

                    int statusf =
                        FindParentDeviceInstanceId(replaceUSBwithHID(finalS),
                                                   L".*",
                                                   parent);
                    wcout << "Device parent: " << parent << endl;
                    lastKnownParent = parent;
                }
                else
                {
                    cout << "Bluetooth connection detected." << endl;
                }

                LastTimeReconnected = std::chrono::high_resolution_clock::now();
                soundMap.clear();
                CurrentlyPlayedSounds.clear();
                return DualsenseUtils::Reconnect_Result::Reconnected;
            }
        }
        else
        {
            return DualsenseUtils::Reconnect_Result::No_Change;
        }
    }

    void Reinitialize()
    {
        Dualsense(path);
    }

    ~Dualsense()
    {
        SetLeftTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0,0,0,0,0);
        SetRightTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0,0,0,0,0);
        SetSpeakerVolume(100);
        UseHeadsetNotSpeaker(false);

        if (Connected)
        {
            if (connectionType == Feature::USB)
                CurSettings.Features = Feature::FeatureType::LetGo;
            else
            {
                // Return to blue lightbar if bluetooth
                SetPlayerLED(0);
                SetLightbar(0, 0, 255);
                SetMicrophoneLED(false, false);
            }

            UseRumbleNotHaptics(false);
            Write();
        }

        if (AudioInitialized) {
            for (auto it = soundMap.begin(); it != soundMap.end();) {
                if (it->second) {
                    ma_sound_uninit(it->second.get());  // Clean up the ma_sound object
                }
                it = soundMap.erase(it);  // Erase and safely update the iterator
            }

            ma_engine_uninit(&engine);
            ma_device_uninit(&device);
            ma_device_uninit(&deviceLoopback);           
            ma_context_uninit(&context);
           
            AudioInitialized = false;
        }
    };

    void SetLightbar(uint8_t R, uint8_t G, uint8_t B)
    {
        CurSettings.Red = R;
        CurSettings.Green = G;
        CurSettings.Blue = B;
    }

    void SetMicrophoneLED(bool LED, bool Pulse = false)
    {
        if (LED && Pulse)
        {
            CurSettings.micLED = Feature::MicrophoneLED::Pulse;
        }
        else if (LED)
        {
            CurSettings.micLED = Feature::MicrophoneLED::MLED_On;
        }
        else
        {
            CurSettings.micLED = Feature::MicrophoneLED::MLED_Off;
        }
    }

    void SetMicrophoneVolume(int Volume) {
        CurSettings.MicrophoneVolume = Volume;
    }

    void SetPlayerLED(uint8_t Player)
    {
        switch (Player)
        {
        case 0:
            CurSettings.PlayerLED_Bitmask = 0x00;
            break;
        case 1:
            CurSettings.PlayerLED_Bitmask = 0x04;
            break;
        case 2:
            CurSettings.PlayerLED_Bitmask = 0x02;
            break;
        case 3:
            CurSettings.PlayerLED_Bitmask = 0x01 | 0x04;
            break;
        case 4:
            CurSettings.PlayerLED_Bitmask = 0x02 | 0x10;
            break;
        case 5:
            CurSettings.PlayerLED_Bitmask = 0x02 | 0x04 | 0x10;
            break;
        default:
            CurSettings.PlayerLED_Bitmask = 0x00;
            break;
        }
    }

    void SetRightTrigger(Trigger::TriggerMode triggerMode,
                         uint8_t Force1,
                         uint8_t Force2,
                         uint8_t Force3,
                         uint8_t Force4,
                         uint8_t Force5,
                         uint8_t Force6,
                         uint8_t Force7,
                         uint8_t Force8,
                         uint8_t Force9,
                         uint8_t Force10,
                         uint8_t Force11)
    {
        CurSettings.RightTriggerMode = triggerMode;
        CurSettings.RightTriggerForces[0] = Force1;
        CurSettings.RightTriggerForces[1] = Force2;
        CurSettings.RightTriggerForces[2] = Force3;
        CurSettings.RightTriggerForces[3] = Force4;
        CurSettings.RightTriggerForces[4] = Force5;
        CurSettings.RightTriggerForces[5] = Force6;
        CurSettings.RightTriggerForces[6] = Force7;
        CurSettings.RightTriggerForces[7] = Force8;
        CurSettings.RightTriggerForces[8] = Force9;
        CurSettings.RightTriggerForces[9] = Force10;
        CurSettings.RightTriggerForces[10] = Force11;
    }

    void SetLeftTrigger(Trigger::TriggerMode triggerMode,
                        uint8_t Force1,
                        uint8_t Force2,
                        uint8_t Force3,
                        uint8_t Force4,
                        uint8_t Force5,
                        uint8_t Force6,
                        uint8_t Force7,
                        uint8_t Force8,
                        uint8_t Force9,
                        uint8_t Force10,
                        uint8_t Force11)
    {
        CurSettings.LeftTriggerMode = triggerMode;
        CurSettings.LeftTriggerForces[0] = Force1;
        CurSettings.LeftTriggerForces[1] = Force2;
        CurSettings.LeftTriggerForces[2] = Force3;
        CurSettings.LeftTriggerForces[3] = Force4;
        CurSettings.LeftTriggerForces[4] = Force5;
        CurSettings.LeftTriggerForces[5] = Force6;
        CurSettings.LeftTriggerForces[6] = Force7;
        CurSettings.LeftTriggerForces[7] = Force8;
        CurSettings.LeftTriggerForces[8] = Force9;
        CurSettings.LeftTriggerForces[9] = Force10;
        CurSettings.LeftTriggerForces[10] = Force11;
    }

    void UseRumbleNotHaptics(bool flag)
    {
        CurSettings.VibrationType = flag ? Feature::StandardRumble : Feature::Haptic_Feedback;
    }

    void SetSpeakerVolume(int Volume) {
        CurSettings.SpeakerVolume = Volume;
    }

    void SetHeadsetVolume(int Volume) {
        CurSettings.HeadsetVolume = Volume;
    }

    void SetRumble(uint8_t LeftMotor, uint8_t RightMotor)
    {
        if (LeftMotor < 0 || LeftMotor > 255)
        {
            return;
        }

        if (RightMotor < 0 || RightMotor > 255)
        {
            return;
        }

        CurSettings._LeftMotor = LeftMotor;
        CurSettings._RightMotor = RightMotor;
    }

    void UseHeadsetNotSpeaker(bool flag) {
        CurSettings.audioOutput = flag ? CurSettings.audioOutput = Feature::AudioOutput::Headset : CurSettings.audioOutput = Feature::AudioOutput::Speaker;
    }
};
