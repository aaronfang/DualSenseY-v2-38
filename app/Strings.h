#include "MyUtils.h"

using json = nlohmann::json;

std::vector<std::string> languages = {"en", "pl", "it", "es", "pt", "fr", "ja", "zh", "ko", "ru", "tr", "vi"};

// Define all regular string members with their default values
#define STRING_MEMBERS \
    X(Position, "Position") \
    X(Strength, "Strength") \
    X(StartPosition, "Start position") \
    X(EndPosition, "End position") \
    X(Amplitude, "Amplitude") \
    X(StartStrength, "Start strength") \
    X(EndStrength, "End strength") \
    X(SelectedTrigger, "Currently selected trigger") \
    X(TriggerFormat, "Trigger format") \
    X(LeftTrigger, "Left") \
    X(RightTrigger, "Right") \
    X(HeadsetVolume, "Headset volume") \
    X(MaxRightMotor, "Max right motor") \
    X(MaxLeftMotor, "Max left motor") \
    X(CommunicationPaused, "COMMUNICATION PAUSED") \
    X(Resume, "Resume") \
    X(R2ToMouseClick, "R2 to mouse click") \
    X(L2Deadzone, "L2 deadzone") \
    X(R2Deadzone, "R2 deadzone") \
    X(MinimumStickValue, "Minimum stick value") \
    X(GyroDeadzone, "Gyro deadzone") \
    X(StopWriting, "Stop writing") \
    X(ControllerEmulationRequired, "Controller emulation required!") \
    X(L2Threshold, "Left trigger threshold") \
    X(GyroToMouse, "Gyro to mouse") \
    X(GyroToRightAnalogStick, "Gyro to right analog stick") \
    X(Frequency, "Frequency") \
    X(TouchpadToHaptics, "Touchpad to haptics") \
    X(Motion, "Motion") \
    X(Gyroscope, "Gyroscope") \
    X(Accelerometer, "Accelerometer") \
    X(OverrideDS4Lightbar, "Override DS4 lightbar") \
    X(FullyRetractWhenNoData, "Fully retract the triggers when there is no incoming data") \
    X(TouchpadAsRightStick, "Use touchpad as right analog stick") \
    X(UseControllerSpeakerInsteadOfSystemAudio, "Use speaker audio peaks instead of system audio") \
    X(UseControllerActuatorsInsteadOfSystemAudio, "Use actuator audio peaks instead of system audio") \
    X(RunAudioToHapticsOnStart, "Run audio to haptics on start") \
    X(Style, "Style") \
    X(SaveStyleToFile, "Save style to file") \
    X(LoadStyleFromFile, "Load style from file") \
    X(SetDefaultStyleFile, "Set default style file") \
    X(ResetStyle, "Reset style") \
    X(RemoveDefaultStyle, "Remove default style config") \
    X(DisablePlayerLED, "Disable player LED") \
    X(Speed, "Speed") \
    X(AudioEngineNotInitializedError, "Audio engine wasn't initialized, haptic feedback not available") \
    X(ScreenshotSoundVolume, "Screenshot sound volume") \
    X(QuietColor, "Quiet color") \
    X(MediumColor, "Medium color") \
    X(LoudColor, "Loud Color") \
    X(ShowConsole, "Show debug console") \
    X(UseHeadset, "Use headset") \
    X(TouchpadShortcut, "Touchpad + Mic button = Touchpad as mouse") \
    X(TouchpadAsSelectStart, "Touchpad as select/start") \
    X(TouchpadAsSelect, "Touchpad as select") \
    X(TouchpadAsStart, "Touchpad as start") \
    X(DualShock4V2emu, "DualShock4 V2 (CUH-ZCT2x)") \
    X(LeftAnalogStickDeadZone, "Left analog stick deadzone") \
    X(RightAnalogStickDeadZone, "Right analog stick deadzone") \
    X(TriggersAsButtons, "Triggers as buttons") \
    X(SpeakerVolume, "Speaker volume") \
    X(HapticsUnavailableNoAudioDevice, "Couldn't find the DualSense Wireless Controller speaker associated with this controller. Haptic feedback not available") \
    X(HapticsToTriggers, "Haptics To Adaptive Triggers") \
    X(RumbleToAT_RigidMode, "Rigid trigger mode") \
    X(ControllerNumberText, "Controller No.") \
    X(USBorBTconnectionType, "Connection type") \
    X(BatteryLevel, "Battery level") \
    X(InstallLatestUpdate, "Install latest update") \
    X(LeftTriggerMode, "Left Trigger Mode") \
    X(RightTriggerMode, "Right Trigger Mode") \
    X(LEDandATunavailableUDP, "LED and Adaptive Trigger settings are unavailable while UDP is active") \
    X(HapticsUnavailable, "Haptic Feedback features are unavailable in Bluetooth mode.") \
    X(X360emu, "Start X360 emulation") \
    X(DS4emu, "Start DS4 emulation") \
    X(STOPemu, "Stop emulation") \
    X(ControllerEmuUserMode, "Application is not running as administrator, real controller will not be hidden from other apps durning controller emulation") \
    X(ControllerEmuAppAsAdmin, "Controller will be hidden only if HidHide Driver is installed") \
    X(Language, "Language") \
    X(Sensitivity, "Sensitivity") \
    X(GeneralData, "General data") \
    X(TouchPacketNum, "Touch packet number") \
    X(Touch, "Touch") \
    X(StandardRumble, "Standard rumble") \
    X(MicButton, "Microphone button") \
    X(LED_DS4_Unavailable, "LED settings are unavailable while emulating DualShock 4") \
    X(UDPStatus, "UDP Status") \
    X(TouchpadToMouse, "Touchpad as mouse") \
    X(Active, "Active") \
    X(Inactive, "Inactive") \
    X(RumbleToAT, "Rumble To Adaptive Triggers") \
    X(SwapTriggersRumbleToAT, "Swap triggers") \
    X(MaxLeftIntensity, "Max left trigger intensity") \
    X(MaxLeftFrequency, "Max left trigger frequency") \
    X(MaxRightIntensity, "Max right trigger intensity") \
    X(MaxRightFrequency, "Max right trigger frequency") \
    X(LedSection, "LED") \
    X(AudioToLED, "Audio to LED") \
    X(DiscoMode, "Disco Mode") \
    X(BatteryLightbar, "Lightbar battery status") \
    X(LED_Red, "Red") \
    X(LED_Green, "Green") \
    X(LED_Blue, "Blue") \
    X(AdaptiveTriggers, "Adaptive Triggers") \
    X(HapticFeedback, "Haptic Feedback") \
    X(LeftMotor, "Left \"Motor\"") \
    X(RightMotor, "Right \"Motor\"") \
    X(StartAudioToHaptics, "Start [Audio To Haptics]") \
    X(TakeScreenshot, "Take screenshot") \
    X(RealMicFunctionality, "Real functionality") \
    X(SwapTriggersShortcut, "Up D-pad + Mic button = Swap triggers in \"Rumble To AT\" option") \
    X(X360Shortcut, "Left D-pad + Mic button = X360 Controller Emulation") \
    X(DS4Shortcut, "Right D-pad + Mic button = DS4 Emulation") \
    X(StopWritingShortcut, "Triangle + Mic button = Stop writing") \
    X(StopEmuShortcut, "Down D-pad + Mic button = Stop emulation") \
    X(GyroToMouseShortcut, "L1 + Mic button = Gyro to mouse") \
    X(GyroToRightAnalogStickShortcut, "L2 + Mic button = Gyro to right analog stick") \
    X(EmulationHeader, "Controller emulation (DS4/X360)") \
    X(ConfigHeader, "Config") \
    X(SaveConfig, "Save current configuration") \
    X(LoadConfig, "Load configuration") \
    X(SetDefaultConfig, "Set default config for this port") \
    X(RemoveDefaultConfig, "Remove default config") \
    X(RunAsAdmin, "Run as administrator") \
    X(HideToTray, "Hide to tray") \
    X(HideWindowOnStartup, "Hide window on startup") \
    X(RunWithWindows, "Run with Windows") \
    X(Scale, "Scale") \
    X(Tooltip_MaxMotor, "Sets maximum vibration for emulated controller") \
    X(Tooltip_GyroToRightAnalogStick, "When holding L2, the right analog stick will move according to your controller's motions") \
    X(Tooltip_GyroToMouse, "When holding L2, the mouse will move according to your controller's motions") \
    X(Tooltip_TouchpadToHaptics, "Makes the controller vibrate based on finger position") \
    X(Tooltip_Dualshock4V2, "Emulates the second revision of DualShock 4, older games will not recognise it") \
    X(Tooltip_TriggersAsButtons, "Sets trigger resistance to very hard and L2/R2 to maximum on slightest trigger pull") \
    X(Tooltip_SpeakerVolume, "Sets speaker volume at hardware level, affects everything played on this controller") \
    X(Tooltip_HapticsToTriggers, "Turns haptic feedback signal to Adaptive Trigger effects, works only for games that don't write to the controller (Ex. Zenless Zone Zero) or Audio To Haptics feature") \
    X(Tooltip_RumbleToAT_RigidMode, "Sets rigid trigger effect") \
    X(Tooltip_RumbleToAT, "Translates rumble vibrations to adaptive triggers, really good in games that don't support them natively") \
    X(Tooltip_SwapTriggersRumbleToAT, "Sets left motor to right trigger and right motor to left trigger") \
    X(Tooltip_MaxIntensity, "Sets maximum trigger vibration sensitivity for Rumble To Adaptive Triggers option") \
    X(Tooltip_MaxFrequency, "Sets maximum trigger vibration frequency for Rumble To Adaptive Triggers option") \
    X(Tooltip_AudioToLED, "Sync the lightbar with audio levels") \
    X(Tooltip_DiscoMode, "Animated color transition, gamers' favorite.") \
    X(Tooltip_BatteryLightbar, "Display Battery Status with color-coded LED indicators.") \
    X(Tooltip_HapticFeedback, "Controls standard rumble values of your DualSense. This is what your controller does to emulate DualShock 4 rumble motors.") \
    X(Tooltip_StartAudioToHaptics, "Creates haptic feedback from your system audio.") \
    X(Tooltip_TakeScreenshot, "Takes screenshot on mic button click. It's saved to your clipboard and your Pictures directory") \
    X(Tooltip_RealMicFunctionality, "Mimics microphone button functionality from the PS5, only works on this controller's microphone.") \
    X(Tooltip_SwapTriggersShortcut, "Shortcut to toggle Swap Triggers in \"Rumble To AT\"") \
    X(Tooltip_X360Shortcut, "Shortcut to start X360 Controller Emulation") \
    X(Tooltip_DS4Shortcut, "Shortcut to start DS4 Controller Emulation") \
    X(Tooltip_StopEmuShortcut, "Shortcut to stop controller emulation") \
    X(Tooltip_SaveConfig, "Saves current values to a file") \
    X(Tooltip_LoadConfig, "Loads a selected configuration file") \
    X(Tooltip_SetDefaultConfig, "Sets default config for this port. Auto-loads when the controller is connected.") \
    X(Tooltip_RemoveDefaultConfig, "Removes default configuration for this port") \
    X(Tooltip_RunAsAdmin, "Runs the app as administrator on startup (requires restart)") \
    X(Tooltip_HideToTray, "Hides window after minimizing; click tray icon to restore") \
    X(Tooltip_HideWindowOnStartup, "Hides window at startup") \
    X(Tooltip_RunWithWindows, "Runs application on startup") \
    X(Tooltip_TouchpadToMouse, "Use dualsense touchpad like a laptop touchpad") \
    X(Tooltip_StopWriting, "This option will halt the communication between DualSenseY and your DualSense controller. Useful when you want to play a native DualSense game without closing this app.")

class Strings {
public:
    #define X(name, default) std::string name = default;
    STRING_MEMBERS
    #undef X

    std::string _Touchpad = "Touchpad";

    json to_json() const {
        json j;

        #define X(name, default) j[#name] = name;
        STRING_MEMBERS
        #undef X

        j["Touchpad"] = _Touchpad;
        return j;
    }

    static Strings from_json(const json& j) {
        Strings s;
        #define X(name, default) if (j.contains(#name)) j.at(#name).get_to(s.name);
        STRING_MEMBERS
        #undef X

        if (j.contains("Touchpad")) j.at("Touchpad").get_to(s._Touchpad);
        return s;
    }
};

static Strings ReadStringsFromFile(const std::string& language) {
    Strings strings;
    std::string fileLocation = MyUtils::GetExecutableFolderPath() + "\\localizations\\" + language + ".json";
    
    std::ifstream file(fileLocation);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << fileLocation << std::endl;
        return strings; 
    }

    try {
        json j;
        file >> j;
        strings = Strings::from_json(j);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
    }
    file.close();
    return strings;
}
