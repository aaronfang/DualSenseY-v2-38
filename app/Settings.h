#pragma once
#include <nlohmann/json.hpp>

enum AudioEditType {
    Pitch,
    Volume,
    Stop,
    StopAll,
};

struct AudioEdit {
    std::string File = "";
    AudioEditType EditType;
    float Value = 0;
};

struct AudioPlay {
    std::string File = "";
    bool DontPlayIfAlreadyPlaying = false; 
    bool Loop = false;
};


class Settings
{
public:
    DualsenseUtils::InputFeatures ControllerInput;
    int L2Feedback[2]{0};
    int R2Feedback[2]{0};
    int L2WeaponFeedback[3]{0};
    int R2WeaponFeedback[3]{0};
    int L2VibrationFeedback[3]{0};
    int R2VibrationFeedback[3]{0};
    int L2SlopeFeedback[4]{0};
    int R2SlopeFeedback[4]{0};
    int L2MultiplePositionFeedback[10]{0};
    int R2MultiplePositionFeedback[10]{0};
    int L2MultipleVibrationFeedback[11]{0};
    int R2MultipleVibrationFeedback[11]{0};
    int L2DSXForces[11]{0};
    int R2DSXForces[11]{0};
    bool AudioToLED = false;
    bool DiscoMode = false;
    int lrumble = 0;
    int rrumble = 0;
    std::string lmodestr = "Off";
    std::string rmodestr = "Off";
    std::string lmodestrSony = "Off";
    std::string rmodestrSony = "Off";
    bool UseUDP = false;
    bool CurrentlyUsingUDP = false;
    bool MicScreenshot = false;
    bool MicFunc = false;
    bool RumbleToAT = false;
    bool RumbleToAT_RigidMode = false;
    bool BatteryLightbar = false;
    bool TouchpadToMouse = false;
    bool X360Shortcut = false;
    bool DS4Shortcut = false;
    bool TouchpadToMouseShortcut = false;
    bool StopEmuShortcut = false;
    bool SwapTriggersRumbleToAT = false;
    int MaxLeftRumbleToATIntensity = 255;
    int MaxLeftRumbleToATFrequency = 25;
    int MaxRightRumbleToATIntensity = 255;
    int MaxRightRumbleToATFrequency = 25;
    int MaxLeftTriggerRigid = 255;
    int MaxRightTriggerRigid = 255;
    bool DualShock4V2 = false;
    bool TouchpadAsSelectStart = false;
    bool TouchpadAsSelect = false;
    bool TouchpadAsStart = false;
    bool SwapTriggersShortcut = false;
    bool UseHeadset = false;
    float swipeThreshold = 0.50f;
    bool HapticsToTriggers = false;
    int LeftAnalogDeadzone = 0;
    int RightAnalogDeadzone = 0;
    bool TriggersAsButtons = false;
    float AudioToTriggersMaxFloatValue = 5.0f;
    int QUIET_THRESHOLD = 85;   // Below this is considered quiet
    int MEDIUM_THRESHOLD = 170; // Below this is medium, above is loud
    int QUIET_COLOR[3] = {10, 10, 10};
    int MEDIUM_COLOR[3] = { 125, 125, 125 };
    int LOUD_COLOR[3] = {255, 255, 255};
    float ScreenshotSoundLoudness = 1;
    int DiscoSpeed = 1;
    bool DisablePlayerLED = false;
    bool RunAudioToHapticsOnStart = false;
    bool WasAudioToHapticsRan = false;
    bool WasAudioToHapticsCheckboxChecked = false;
    bool HapticsToLED = false;
    bool SpeakerToLED = false;
    float HapticsAndSpeakerToLedScale = 0.5f;
    bool OverrideDS4Lightbar = false;
    bool TouchpadToRXRY = false;
    bool FullyRetractTriggers = true;
    bool TouchpadToHaptics = false;
    float TouchpadToHapticsFrequency = 20.0f;
    bool GyroToMouse = false;
    float GyroToMouseSensitivity = 0.010f;
    bool GyroToRightAnalogStick = false;
    int L2Threshold = 100;
    float GyroToRightAnalogStickSensitivity = 5.0f;
    float GyroToRightAnalogStickDeadzone = 0.005f;
    int GyroToRightAnalogStickMinimumStickValue = 64;
    bool StopWriting = false;
    bool StopWritingShortcut = false;
    int L2Deadzone = 0;
    int R2Deadzone = 0;
    int L2UDPDeadzone = 0;
    int R2UDPDeadzone = 0;
    bool GyroToMouseShortcut = false;
    bool GyroToRightAnalogStickShortcut = false;
    bool R2ToMouseClick = false;
    int MaxLeftMotor = 255;
    int MaxRightMotor = 255;
    const char* curTrigger = "L2";
    std::string triggerFormat = "Sony"; // OR DSX

    // UDP HAPTICS STUFF, DONT SAVE
    std::string CurrentHapticFile = "";
    bool DontPlayIfAlreadyPlaying = false;
    bool Loop = false;
    std::vector<AudioEdit> AudioEditQueue;
    std::vector<AudioPlay> AudioPlayQueue;

    EmuStatus emuStatus = None;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["ControllerInput"] = ControllerInput.to_json();

        j["R2Feedback"] = R2Feedback;
        j["R2WeaponFeedback"] = R2WeaponFeedback;
        j["R2VibrationFeedback"] = R2VibrationFeedback;
        j["R2SlopeFeedback"] = R2SlopeFeedback;
        j["R2MultiplePositionFeedback"] = R2MultiplePositionFeedback;
        j["R2MultipleVibrationFeedback"] = R2MultipleVibrationFeedback;
        j["R2DSXForces"] = L2DSXForces;
        j["L2Feedback"] = L2Feedback;
        j["L2WeaponFeedback"] = L2WeaponFeedback;
        j["L2VibrationFeedback"] = L2VibrationFeedback;
        j["L2SlopeFeedback"] = L2SlopeFeedback;
        j["L2MultiplePositionFeedback"] = L2MultiplePositionFeedback;
        j["L2MultipleVibrationFeedback"] = L2MultipleVibrationFeedback;
        j["L2DSXForces"] = L2DSXForces;
        j["MaxLeftMotor"] = MaxLeftMotor;
        j["triggerFormat"] = triggerFormat;
        j["MaxRightMotor"] = MaxRightMotor;
        j["R2ToMouseClick"] = R2ToMouseClick;
        j["GyroToRightAnalogStickShortcut"] = GyroToRightAnalogStickShortcut;
        j["GyroToMouseShortcut"] = GyroToMouseShortcut;
        j["L2Deadzone"] = L2Deadzone;
        j["R2Deadzone"] = R2Deadzone;
        j["StopWritingShortcut"] = StopWritingShortcut;
        j["StopWriting"] = StopWriting;
        j["GyroToRightAnalogStickSensitivity"] = GyroToRightAnalogStickSensitivity;
        j["GyroToRightAnalogStickDeadzone"] = GyroToRightAnalogStickDeadzone;
        j["GyroToRightAnalogStickMinimumStickValue"] = GyroToRightAnalogStickMinimumStickValue;
        j["GyroToMouseSensitivity"] = GyroToMouseSensitivity;
        j["GyroToMouse"] = GyroToMouse;
        j["GyroToRightAnalogStick"] = GyroToRightAnalogStick;
        j["L2Threshold"] = L2Threshold;
        j["TouchpadToHaptics"] = TouchpadToHaptics;
        j["TouchpadToHapticsFrequency"] = TouchpadToHapticsFrequency;
        j["FullyRetractTriggers"] = FullyRetractTriggers;
        j["OverrideDS4Lightbar"] = OverrideDS4Lightbar;
        j["TouchpadToRXRY"] = TouchpadToRXRY;
        j["HapticsAndSpeakerToLedScale"] = HapticsAndSpeakerToLedScale;
        j["SpeakerToLED"] = SpeakerToLED;
        j["HapticsToLED"] = HapticsToLED;
        j["RunAudioToHapticsOnStart"] = RunAudioToHapticsOnStart;
        j["DisablePlayerLED"] = DisablePlayerLED;
        j["DiscoSpeed"] = DiscoSpeed;
        j["ScreenshotSoundLoudness"] = ScreenshotSoundLoudness;
        j["QUIET_COLOR"] = QUIET_COLOR;
        j["MEDIUM_COLOR"] = MEDIUM_COLOR;
        j["LOUD_COLOR"] = LOUD_COLOR;
        j["TouchpadAsSelectStart"] = TouchpadAsSelectStart;
        j["TouchpadAsSelect"] = TouchpadAsSelect;
        j["TouchpadAsStart"] = TouchpadAsStart;
        j["UseHeadset"] = UseHeadset;
        j["TouchpadToMouseShortcut"] = TouchpadToMouseShortcut;
        j["DualShock4V2"] = DualShock4V2;
        j["LeftAnalogDeadzone"] = LeftAnalogDeadzone;
        j["RightAnalogDeadzone"] = RightAnalogDeadzone;
        j["TriggersAsButtons"] = TriggersAsButtons;
        j["AudioToLED"] = AudioToLED;
        j["DiscoMode"] = DiscoMode;
        j["lrumble"] = lrumble;
        j["rrumble"] = rrumble;
        j["lmodestr"] = lmodestr;
        j["rmodestr"] = rmodestr;
        j["lmodestrSony"] = lmodestrSony;
        j["rmodestrSony"] = rmodestrSony;
        j["UseUDP"] = UseUDP;
        j["MicScreenshot"] = MicScreenshot;
        j["MicFunc"] = MicFunc;
        j["RumbleToAT"] = RumbleToAT;
        j["RumbleToAT_RigidMode"] = RumbleToAT_RigidMode;
        j["BatteryLightbar"] = BatteryLightbar;
        j["swipeThreshold"] = swipeThreshold;
        j["TouchpadToMouse"] = TouchpadToMouse;
        j["X360Shortcut"] = X360Shortcut;
        j["DS4Shortcut"] = DS4Shortcut;
        j["StopEmuShortcut"] = StopEmuShortcut;
        j["SwapTriggersRumbleToAT"] = SwapTriggersRumbleToAT;
        j["MaxLeftRumbleToATIntensity"] = MaxLeftRumbleToATIntensity;
        j["MaxLeftRumbleToATFrequency"] = MaxLeftRumbleToATFrequency;
        j["MaxRightRumbleToATIntensity"] = MaxRightRumbleToATIntensity;
        j["MaxRightRumbleToATFrequency"] = MaxRightRumbleToATFrequency;
        j["MaxLeftTriggerRigid"] = MaxLeftTriggerRigid;
        j["MaxRightTriggerRigid"] = MaxRightTriggerRigid;
        j["SwapTriggersShortcut"] = SwapTriggersShortcut;
        j["AudioToTriggersMaxFloatValue"] = AudioToTriggersMaxFloatValue;
        j["HapticsToTriggers"] = HapticsToTriggers;
        j["emuStatus"] = static_cast<int>(emuStatus); // Assuming EmuStatus is an enum
        return j;
    }

    static Settings from_json(const nlohmann::json& j) {
        Settings settings;

        // Parse ControllerInput first
        if (j.contains("ControllerInput")) settings.ControllerInput = DualsenseUtils::InputFeatures::from_json(j.at("ControllerInput"));

        if (j.contains("R2Feedback"))       j.at("R2Feedback").get_to(settings.R2Feedback);
        if (j.contains("R2WeaponFeedback"))       j.at("R2WeaponFeedback").get_to(settings.R2WeaponFeedback);
        if (j.contains("R2VibrationFeedback"))       j.at("R2VibrationFeedback").get_to(settings.R2VibrationFeedback);
        if (j.contains("R2SlopeFeedback"))       j.at("R2SlopeFeedback").get_to(settings.R2SlopeFeedback);
        if (j.contains("R2MultiplePositionFeedback"))       j.at("R2MultiplePositionFeedback").get_to(settings.R2MultiplePositionFeedback);
        if (j.contains("R2MultipleVibrationFeedback"))       j.at("R2MultipleVibrationFeedback").get_to(settings.R2MultipleVibrationFeedback);
        if (j.contains("R2DSXForces"))       j.at("R2DSXForces").get_to(settings.R2DSXForces);

        if (j.contains("L2Feedback"))       j.at("L2Feedback").get_to(settings.L2Feedback);
        if (j.contains("L2WeaponFeedback"))       j.at("L2WeaponFeedback").get_to(settings.L2WeaponFeedback);
        if (j.contains("L2VibrationFeedback"))       j.at("L2VibrationFeedback").get_to(settings.L2VibrationFeedback);
        if (j.contains("L2SlopeFeedback"))       j.at("L2SlopeFeedback").get_to(settings.L2SlopeFeedback);
        if (j.contains("L2MultiplePositionFeedback"))       j.at("L2MultiplePositionFeedback").get_to(settings.L2MultiplePositionFeedback);
        if (j.contains("L2MultipleVibrationFeedback"))       j.at("L2MultipleVibrationFeedback").get_to(settings.L2MultipleVibrationFeedback);
        if (j.contains("L2DSXForces"))       j.at("L2DSXForces").get_to(settings.L2DSXForces);

        if (j.contains("triggerFormat"))       j.at("triggerFormat").get_to(settings.triggerFormat);
        if (j.contains("MaxLeftMotor"))       j.at("MaxLeftMotor").get_to(settings.MaxLeftMotor);
        if (j.contains("MaxRightMotor"))       j.at("MaxRightMotor").get_to(settings.MaxRightMotor);
        if (j.contains("R2ToMouseClick"))       j.at("R2ToMouseClick").get_to(settings.R2ToMouseClick);
        if (j.contains("GyroToRightAnalogStickShortcut"))       j.at("GyroToRightAnalogStickShortcut").get_to(settings.GyroToRightAnalogStickShortcut);
        if (j.contains("GyroToMouseShortcut"))       j.at("GyroToMouseShortcut").get_to(settings.GyroToMouseShortcut);
        if (j.contains("L2Deadzone"))       j.at("L2Deadzone").get_to(settings.L2Deadzone);
        if (j.contains("R2Deadzone"))       j.at("R2Deadzone").get_to(settings.R2Deadzone);
        if (j.contains("StopWritingShortcut"))       j.at("StopWritingShortcut").get_to(settings.StopWritingShortcut);
        if (j.contains("StopWriting"))       j.at("StopWriting").get_to(settings.StopWriting);
        if (j.contains("GyroToRightAnalogStickMinimumStickValue"))       j.at("GyroToRightAnalogStickMinimumStickValue").get_to(settings.GyroToRightAnalogStickMinimumStickValue);
        if (j.contains("GyroToRightAnalogStickDeadzone"))       j.at("GyroToRightAnalogStickDeadzone").get_to(settings.GyroToRightAnalogStickDeadzone);
        if (j.contains("GyroToRightAnalogStickSensitivity"))       j.at("GyroToRightAnalogStickSensitivity").get_to(settings.GyroToRightAnalogStickSensitivity);
        if (j.contains("GyroToRightAnalogStick"))       j.at("GyroToRightAnalogStick").get_to(settings.GyroToRightAnalogStick);
        if (j.contains("GyroToMouseSensitivity"))       j.at("GyroToMouseSensitivity").get_to(settings.GyroToMouseSensitivity);
        if (j.contains("GyroToMouse"))       j.at("GyroToMouse").get_to(settings.GyroToMouse);
        if (j.contains("L2Threshold"))       j.at("L2Threshold").get_to(settings.L2Threshold);
        if (j.contains("FullyRetractTriggers"))       j.at("FullyRetractTriggers").get_to(settings.FullyRetractTriggers);
        if (j.contains("OverrideDS4Lightbar"))       j.at("OverrideDS4Lightbar").get_to(settings.OverrideDS4Lightbar);
        if (j.contains("TouchpadToRXRY"))       j.at("TouchpadToRXRY").get_to(settings.TouchpadToRXRY);
        if (j.contains("HapticsAndSpeakerToLedScale"))       j.at("HapticsAndSpeakerToLedScale").get_to(settings.HapticsAndSpeakerToLedScale);
        if (j.contains("SpeakerToLED"))       j.at("SpeakerToLED").get_to(settings.SpeakerToLED);
        if (j.contains("HapticsToLED"))       j.at("HapticsToLED").get_to(settings.HapticsToLED);
        if (j.contains("RunAudioToHapticsOnStart"))       j.at("RunAudioToHapticsOnStart").get_to(settings.RunAudioToHapticsOnStart);
        if (j.contains("AudioToTriggersMaxFloatValue"))       j.at("AudioToTriggersMaxFloatValue").get_to(settings.AudioToTriggersMaxFloatValue);
        if (j.contains("MaxLeftTriggerRigid"))       j.at("MaxLeftTriggerRigid").get_to(settings.MaxLeftTriggerRigid);
        if (j.contains("MaxRightTriggerRigid"))       j.at("MaxRightTriggerRigid").get_to(settings.MaxRightTriggerRigid);
        if (j.contains("DisablePlayerLED"))       j.at("DisablePlayerLED").get_to(settings.DisablePlayerLED);
        if (j.contains("DiscoSpeed"))       j.at("DiscoSpeed").get_to(settings.DiscoSpeed);
        if (j.contains("ScreenshotSoundLoudness"))       j.at("ScreenshotSoundLoudness").get_to(settings.ScreenshotSoundLoudness);
        if (j.contains("QUIET_COLOR"))       j.at("QUIET_COLOR").get_to(settings.QUIET_COLOR);
        if (j.contains("MEDIUM_COLOR"))       j.at("MEDIUM_COLOR").get_to(settings.MEDIUM_COLOR);
        if (j.contains("LOUD_COLOR"))       j.at("LOUD_COLOR").get_to(settings.LOUD_COLOR);
        if (j.contains("TouchpadToMouseShortcut"))       j.at("TouchpadToMouseShortcut").get_to(settings.TouchpadToMouseShortcut);
        if (j.contains("UseHeadset"))       j.at("UseHeadset").get_to(settings.UseHeadset);
        if (j.contains("TouchpadAsSelectStart"))       j.at("TouchpadAsSelectStart").get_to(settings.TouchpadAsSelectStart);
        if (j.contains("TouchpadAsSelect"))       j.at("TouchpadAsSelect").get_to(settings.TouchpadAsSelect);
        if (j.contains("TouchpadAsStart"))       j.at("TouchpadAsStart").get_to(settings.TouchpadAsStart);
        if (j.contains("DualShock4V2"))       j.at("DualShock4V2").get_to(settings.DualShock4V2);
        if (j.contains("LeftAnalogDeadzone"))       j.at("LeftAnalogDeadzone").get_to(settings.LeftAnalogDeadzone);
        if (j.contains("RightAnalogDeadzone"))        j.at("RightAnalogDeadzone").get_to(settings.RightAnalogDeadzone);
        if (j.contains("TriggersAsButtons"))        j.at("TriggersAsButtons").get_to(settings.TriggersAsButtons);
        if (j.contains("AudioToLED"))       j.at("AudioToLED").get_to(settings.AudioToLED);
        if (j.contains("DiscoMode"))        j.at("DiscoMode").get_to(settings.DiscoMode);
        if (j.contains("lrumble"))          j.at("lrumble").get_to(settings.lrumble);
        if (j.contains("rrumble"))          j.at("rrumble").get_to(settings.rrumble);
        if (j.contains("lmodestr"))         j.at("lmodestr").get_to(settings.lmodestr);
        if (j.contains("rmodestr"))         j.at("rmodestr").get_to(settings.rmodestr);
        if (j.contains("lmodestrSony"))         j.at("lmodestrSony").get_to(settings.lmodestrSony);
        if (j.contains("rmodestrSony"))         j.at("rmodestrSony").get_to(settings.rmodestrSony);
        if (j.contains("UseUDP"))           j.at("UseUDP").get_to(settings.UseUDP);
        if (j.contains("MicScreenshot"))    j.at("MicScreenshot").get_to(settings.MicScreenshot);
        if (j.contains("MicFunc"))          j.at("MicFunc").get_to(settings.MicFunc);
        if (j.contains("RumbleToAT"))          j.at("RumbleToAT").get_to(settings.RumbleToAT);
        if (j.contains("RumbleToAT_RigidMode"))          j.at("RumbleToAT_RigidMode").get_to(settings.RumbleToAT_RigidMode);
        if (j.contains("BatteryLightbar"))          j.at("BatteryLightbar").get_to(settings.BatteryLightbar);
        if (j.contains("swipeThreshold"))          j.at("swipeThreshold").get_to(settings.swipeThreshold);
        if (j.contains("TouchpadToMouse"))          j.at("TouchpadToMouse").get_to(settings.TouchpadToMouse);
        if (j.contains("X360Shortcut"))          j.at("X360Shortcut").get_to(settings.X360Shortcut);
        if (j.contains("DS4Shortcut"))          j.at("DS4Shortcut").get_to(settings.DS4Shortcut);
        if (j.contains("StopEmuShortcut"))          j.at("StopEmuShortcut").get_to(settings.StopEmuShortcut);
        if (j.contains("SwapTriggersRumbleToAT"))          j.at("SwapTriggersRumbleToAT").get_to(settings.SwapTriggersRumbleToAT);
        if (j.contains("MaxRumbleToATIntensity"))          j.at("MaxRumbleToATIntensity").get_to(settings.MaxLeftRumbleToATIntensity);
        if (j.contains("MaxRumbleToATFrequency"))          j.at("MaxRumbleToATFrequency").get_to(settings.MaxLeftRumbleToATFrequency);
        if (j.contains("MaxRightRumbleToATIntensity"))          j.at("MaxRightRumbleToATIntensity").get_to(settings.MaxRightRumbleToATIntensity);
        if (j.contains("MaxRightRumbleToATFrequency"))          j.at("MaxRightRumbleToATFrequency").get_to(settings.MaxRightRumbleToATFrequency);
        if (j.contains("SwapTriggersShortcut"))          j.at("SwapTriggersShortcut").get_to(settings.SwapTriggersShortcut);
        if (j.contains("HapticsToTriggers"))          j.at("HapticsToTriggers").get_to(settings.HapticsToTriggers);
        if (j.contains("emuStatus"))        j.at("emuStatus").get_to(settings.emuStatus);

        return settings;
    }
};
