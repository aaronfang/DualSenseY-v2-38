const int VERSION = 38;

extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

#include "MyUtils.h"
#include "DualSense.h"
#include "ControllerEmulation.h"
#include "cycle.hpp"
#include "miniaudio.h"
#include "Config.h"
#include "UDP.h"
#include "Settings.h"
#include "Strings.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <algorithm>
#include <urlmon.h>

//#define _CRTDBG_MAP_ALLOC
//#include <cstdlib>
//#include <crtdbg.h>

#if defined(_MSC_VER) && (_MSC_VER >= 1900) &&                                 \
    !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")

#endif

static void glfw_error_callback(int error, const char *description);

deque<Dualsense> DualSense;
std::atomic<bool> stop_thread{false}; // Flag to signal the thread to stop

void readControllerState(Dualsense &controller, Settings &settings)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    while (!stop_thread)
    {
        controller.Read(); // Perform the read operation

        if(controller.Connected == false)
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void Tooltip(const char* text) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// Required structure definitions
struct TriggerSettings {
    Trigger::TriggerMode Mode;  // Assuming Trigger::ModeType is your trigger mode enum
    bool right = false;
    uint8_t Forces[11];
};

void setTriggerSettings(TriggerSettings& trigger, const char* modeStr, 
                       const int* feedbackArray, size_t feedbackSize) {
    if (strcmp(modeStr, "Off") == 0) {
        trigger.Mode = Trigger::Rigid_B;
        for (int i = 0; i < 11; i++) {
            trigger.Forces[i] = 0;
        }
    }
    else if (strcmp(modeStr, "Feedback") == 0) {
        uint8_t position = feedbackArray[0];
        uint8_t strength = feedbackArray[1];

        if (position > 9 || strength > 8) {
            return;
        }
        if (strength > 0) {
            uint8_t b = (strength - 1) & 7;
            uint32_t num = 0;
            uint16_t num2 = 0;

            for (int i = position; i < 10; i++) {
                num |= static_cast<uint32_t>(b) << (3 * i);
                num2 |= static_cast<uint16_t>(1 << i);
            }

            trigger.Mode = Trigger::Feedback;
            trigger.Forces[0] = static_cast<uint8_t>(num2 & 0xFF);
            trigger.Forces[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
            trigger.Forces[2] = static_cast<uint8_t>(num & 0xFF);
            trigger.Forces[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
            trigger.Forces[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
            trigger.Forces[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
            for (int i = 6; i < 11; i++) {
                trigger.Forces[i] = 0;
            }
        }
        else {
            trigger.Mode = Trigger::Rigid_B;
            for (int i = 0; i < 11; i++) {
                trigger.Forces[i] = 0;
            }
        }
    }
    else if (strcmp(modeStr, "Weapon") == 0) {
        uint8_t startPosition = feedbackArray[0];
        uint8_t endPosition = feedbackArray[1];
        uint8_t strength = feedbackArray[2];

        if (startPosition > 7 || startPosition < 2 || endPosition > 8 || 
            endPosition <= startPosition || strength > 8) {
            return;
        }
        if (strength > 0) {
            uint16_t num = static_cast<uint16_t>(
                1 << startPosition | 1 << endPosition);

            trigger.Mode = Trigger::Weapon;
            trigger.Forces[0] = static_cast<uint8_t>(num & 0xFF);
            trigger.Forces[1] = static_cast<uint8_t>((num >> 8) & 0xFF);
            trigger.Forces[2] = strength - 1;
            for (int i = 3; i < 11; i++) {
                trigger.Forces[i] = 0;
            }
        }
        else {
            trigger.Mode = Trigger::Rigid_B;
            for (int i = 0; i < 11; i++) {
                trigger.Forces[i] = 0;
            }
        }
    }
    else if (strcmp(modeStr, "Vibration") == 0) {
        uint8_t position = feedbackArray[0];
        uint8_t amplitude = feedbackArray[1];
        uint8_t frequency = feedbackArray[2];

        if (position > 9 || amplitude > 8) {
            return;
        }
        if (amplitude > 0 && frequency > 0) {
            uint8_t b = (uint8_t)((amplitude - 1) & 0x07);
            uint32_t amplitudeZones = 0;
            uint16_t activeZones = 0;

            for (int i = position; i < 10; i++) {
                amplitudeZones |= static_cast<uint32_t>(b << (3 * i));
                activeZones |= static_cast<uint16_t>(1 << i);
            }

            trigger.Mode = Trigger::Vibration;
            trigger.Forces[0] = static_cast<uint8_t>((activeZones >> 0) & 0xFF);
            trigger.Forces[1] = static_cast<uint8_t>((activeZones >> 8) & 0xFF);
            trigger.Forces[2] = static_cast<uint8_t>((amplitudeZones >> 0) & 0xFFF);
            trigger.Forces[3] = static_cast<uint8_t>((amplitudeZones >> 8) & 0xFF);
            trigger.Forces[4] = static_cast<uint8_t>((amplitudeZones >> 16) & 0xFF);
            trigger.Forces[5] = static_cast<uint8_t>((amplitudeZones >> 24) & 0xFF);
            trigger.Forces[6] = 0;
            trigger.Forces[7] = 0;
            trigger.Forces[8] = frequency;
            trigger.Forces[9] = 0;
        }
        else {
            trigger.Mode = Trigger::Rigid_B;
            for (int i = 0; i < 11; i++) {
                trigger.Forces[i] = 0;
            }
        }
    }
    else if (strcmp(modeStr, "Slope Feedback") == 0) {
        uint8_t startPosition = feedbackArray[0];
        uint8_t endPosition = feedbackArray[1];
        uint8_t startStrength = feedbackArray[2];
        uint8_t endStrength = feedbackArray[3];

        if (startPosition > 8 || startPosition < 0 || endPosition > 9 || 
            endPosition <= startPosition || startStrength > 8 || 
            startStrength < 1 || endStrength > 8 || endStrength < 1) {
            return;
        }

        uint8_t array[10] = {0};
        float slope = static_cast<float>(endStrength - startStrength) /
                     static_cast<float>(endPosition - startPosition);
        
        for (int i = startPosition; i < 10; i++) {
            if (i <= endPosition) {
                float strengthFloat = static_cast<float>(startStrength) +
                                   slope * static_cast<float>(i - startPosition);
                array[i] = static_cast<uint8_t>(std::round(strengthFloat));
            }
            else {
                array[i] = endStrength;
            }
        }

        bool anyStrength = false;
        for (int i = 0; i < 10; i++) {
            if (array[i] > 0) anyStrength = true;
        }

        if (anyStrength) {
            uint32_t num = 0;
            uint16_t num2 = 0;
            for (int i = 0; i < 10; i++) {
                if (array[i] > 0) {
                    uint8_t b = (array[i] - 1) & 7;
                    num |= static_cast<uint32_t>(b) << (3 * i);
                    num2 |= static_cast<uint16_t>(1 << i);
                }
            }

            trigger.Mode = Trigger::Feedback;
            trigger.Forces[0] = static_cast<uint8_t>(num2 & 0xFF);
            trigger.Forces[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
            trigger.Forces[2] = static_cast<uint8_t>(num & 0xFF);
            trigger.Forces[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
            trigger.Forces[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
            trigger.Forces[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
            for (int i = 6; i < 11; i++) {
                trigger.Forces[i] = 0;
            }
        }
        else {
            trigger.Mode = Trigger::Rigid_B;
            for (int i = 0; i < 11; i++) {
                trigger.Forces[i] = 0;
            }
        }
    }
    else if (strcmp(modeStr, "Multiple Position Feedback") == 0) {
        uint8_t strength[10];
        for (int i = 0; i < 10; i++) {
            strength[i] = feedbackArray[i];
        }

        bool anyStrength = false;
        for (int i = 0; i < 10; i++) {
            if (strength[i] > 0) anyStrength = true;
        }

        if (anyStrength) {
            uint32_t num = 0;
            uint16_t num2 = 0;
            for (int i = 0; i < 10; i++) {
                if (strength[i] > 0) {
                    uint8_t b = (strength[i] - 1) & 7;
                    num |= static_cast<uint32_t>(b) << (3 * i);
                    num2 |= static_cast<uint16_t>(1 << i);
                }
            }

            trigger.Mode = Trigger::Feedback;
            trigger.Forces[0] = static_cast<uint8_t>(num2 & 0xFF);
            trigger.Forces[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
            trigger.Forces[2] = static_cast<uint8_t>(num & 0xFF);
            trigger.Forces[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
            trigger.Forces[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
            trigger.Forces[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
            for (int i = 6; i < 11; i++) {
                trigger.Forces[i] = 0;
            }
        }
        else {
            trigger.Mode = Trigger::Rigid_B;
            for (int i = 0; i < 11; i++) {
                trigger.Forces[i] = 0;
            }
        }
    }
    else if (strcmp(modeStr, "Multiple Position Vibration") == 0) {
        uint8_t frequency = feedbackArray[0];
        uint8_t amplitude[10];
        for (int i = 0; i < 10; i++) {
            amplitude[i] = feedbackArray[i + 1];
        }

        if (frequency > 0) {
            bool anyAmplitude = false;
            for (int i = 0; i < 10; i++) {
                if (amplitude[i] > 0) anyAmplitude = true;
            }

            if (anyAmplitude) {
                uint32_t num = 0;
                uint16_t num2 = 0;
                for (int i = 0; i < 10; i++) {
                    if (amplitude[i] > 0) {
                        uint8_t b = (amplitude[i] - 1) & 7;
                        num |= static_cast<uint32_t>(b) << (3 * i);
                        num2 |= static_cast<uint16_t>(1 << i);
                    }
                }

                trigger.Mode = Trigger::Vibration;
                trigger.Forces[0] = static_cast<uint8_t>(num2 & 0xFF);
                trigger.Forces[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                trigger.Forces[2] = static_cast<uint8_t>(num & 0xFF);
                trigger.Forces[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
                trigger.Forces[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
                trigger.Forces[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
                trigger.Forces[6] = 0;
                trigger.Forces[7] = 0;
                trigger.Forces[8] = frequency;
                trigger.Forces[9] = 0;
                trigger.Forces[10] = 0;
            }
            else {
                trigger.Mode = Trigger::Rigid_B;
                for (int i = 0; i < 11; i++) {
                    trigger.Forces[i] = 0;
                }
            }
        }
        else {
            trigger.Mode = Trigger::Rigid_B;
            for (int i = 0; i < 11; i++) {
                trigger.Forces[i] = 0;
            }
        }
    }
}

void writeEmuController(Dualsense &controller, Settings &settings)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    ViGEm v;
    v.InitializeVigembus();
    ButtonState editedButtonState;
    constexpr auto POLL_INTERVAL = std::chrono::microseconds(500); // 0.5ms polling
    constexpr auto DISCONNECT_DELAY = std::chrono::seconds(5); // 5 seconds delay

    auto lastConnectedTime = std::chrono::high_resolution_clock::now();
    bool wasVirtualControllerStarted = false;
    EmuStatus lastEmu = None;

    while (!stop_thread)
    {
        if (controller.Connected)
        {
            lastConnectedTime = std::chrono::high_resolution_clock::now();

            if (!wasVirtualControllerStarted) {
                if (settings.emuStatus == X360) {
                    v.StartX360();
                    wasVirtualControllerStarted = true;
                    lastEmu = X360;
                }
                else if (settings.emuStatus == DS4) {
                    if (settings.DualShock4V2)
                        v.SetDS4V2();
                    else
                        v.SetDS4V1();

                    v.StartDS4();
                    lastEmu = DS4;
                    wasVirtualControllerStarted = true;
                }
            }
            else if (wasVirtualControllerStarted&& settings.emuStatus != lastEmu) {
                v.Remove360();
                v.RemoveDS4();
                lastEmu == None;
                wasVirtualControllerStarted = false;
            }

            auto start = std::chrono::high_resolution_clock::now();

            editedButtonState = controller.State;

            int deltaLX = editedButtonState.LX - 128;
            int deltaLY = editedButtonState.LY - 128;

            float magnitudeL = sqrt(deltaLX * deltaLX + deltaLY * deltaLY);

            if (magnitudeL <= settings.LeftAnalogDeadzone) {
                editedButtonState.LX = 128;
                editedButtonState.LY = 128;
            } else {
                float scale = (magnitudeL - settings.LeftAnalogDeadzone) / (127.0f - settings.LeftAnalogDeadzone);

                if (scale > 1.0f) scale = 1.0f;

                editedButtonState.LX = 128 + static_cast<int>(deltaLX * scale);
                editedButtonState.LY = 128 + static_cast<int>(deltaLY * scale);
            }

            int deltaRX = editedButtonState.RX - 128;
            int deltaRY = editedButtonState.RY - 128;

            float magnitudeR = sqrt(deltaRX * deltaRX + deltaRY * deltaRY);

            if (magnitudeR <= settings.RightAnalogDeadzone) {
                editedButtonState.RX = 128;
                editedButtonState.RY = 128;
            } else {
                float scale = (magnitudeR - settings.RightAnalogDeadzone) / (127.0f - settings.RightAnalogDeadzone);

                if (scale > 1.0f) scale = 1.0f;

                editedButtonState.RX = 128 + static_cast<int>(deltaRX * scale);
                editedButtonState.RY = 128 + static_cast<int>(deltaRY * scale);
            }

            if (settings.TriggersAsButtons) {
                if (editedButtonState.L2 >= 1) {
                    editedButtonState.L2 = 255;
                    editedButtonState.L2Btn = true;
                }

                if (editedButtonState.R2 >= 1) {
                    editedButtonState.R2 = 255;
                    editedButtonState.R2Btn = true;
                }
            }

            if (settings.TouchpadAsSelect && !settings.TouchpadAsStart) {
                if (controller.State.touchBtn) {
                    editedButtonState.share = true;
                }
            }
            else if (settings.TouchpadAsStart && !settings.TouchpadAsSelect) {
                if (controller.State.touchBtn) {
                    editedButtonState.options = true;
                }
            }
            else if (settings.TouchpadAsSelect && settings.TouchpadAsStart) {
                if(controller.State.trackPadTouch0.IsActive && !controller.State.trackPadTouch1.IsActive)
                {
                    if (controller.State.touchBtn && controller.State.trackPadTouch0.X <= 1000) {
                        editedButtonState.share = true;
                    }

                    if (controller.State.touchBtn && controller.State.trackPadTouch0.X >= 1001) {
                        editedButtonState.options = true;
                    }
                }
                else if (controller.State.trackPadTouch1.IsActive && !controller.State.trackPadTouch0.IsActive) {
                    if (controller.State.touchBtn && controller.State.trackPadTouch1.X <= 1000) {
                        editedButtonState.share = true;
                    }

                    if (controller.State.touchBtn && controller.State.trackPadTouch1.X >= 1001) {
                        editedButtonState.options = true;
                    }
                }
            }

            if (settings.TouchpadToRXRY && controller.State.trackPadTouch0.IsActive) {
                editedButtonState.RX = MyUtils::ConvertRange(controller.State.trackPadTouch0.X, 0, 1919, 0, 255);
                editedButtonState.RY = MyUtils::ConvertRange(controller.State.trackPadTouch0.Y, 0, 1079, 0, 255);
            }

            if (settings.GyroToRightAnalogStick && (controller.State.L2 >= settings.L2Threshold)) {
                if (fabs(controller.State.RX - 128) <= 80 && fabs(controller.State.RY - 128) <= 80) 
                {
                    float accelY = controller.State.accelerometer.X;
                    float accelX = controller.State.accelerometer.Y;

                    const float maxAccelValue = 10000.0f;

                    float normalizedAccelX = accelX / maxAccelValue;
                    float normalizedAccelY = accelY / maxAccelValue;

                    if (fabs(normalizedAccelX) < settings.GyroToRightAnalogStickDeadzone) {
                        normalizedAccelX = 0.0f;
                    }
                    else {
                        normalizedAccelX = (normalizedAccelX > 0 ? normalizedAccelX - settings.GyroToRightAnalogStickDeadzone : normalizedAccelX + settings.GyroToRightAnalogStickDeadzone) / (1.0f - settings.GyroToRightAnalogStickDeadzone);
                    }

                    if (fabs(normalizedAccelY) < settings.GyroToRightAnalogStickDeadzone) {
                        normalizedAccelY = 0.0f; 
                    }
                    else {
                        normalizedAccelY = (normalizedAccelY > 0 ? normalizedAccelY - settings.GyroToRightAnalogStickDeadzone : normalizedAccelY + settings.GyroToRightAnalogStickDeadzone) / (1.0f - settings.GyroToRightAnalogStickDeadzone);
                    }

                    const float sensitivity = settings.GyroToRightAnalogStickSensitivity * 5;

                    float adjustedX = -normalizedAccelX * sensitivity;
                    float adjustedY = -normalizedAccelY * sensitivity;

                    int accelAnalogStickX;
                    if (adjustedX > 0) {
                        accelAnalogStickX = 128 + max(static_cast<int>(settings.GyroToRightAnalogStickMinimumStickValue), static_cast<int>(adjustedX * 127.0f));
                    }
                    else if (adjustedX < 0) {
                        accelAnalogStickX = 128 - max(static_cast<int>(settings.GyroToRightAnalogStickMinimumStickValue), static_cast<int>(-adjustedX * 128.0f));
                    }
                    else {
                        accelAnalogStickX = 128;  // Center if no movement
                    }

                    int accelAnalogStickY;
                    if (adjustedY > 0) {
                        accelAnalogStickY = 128 + max(static_cast<int>(settings.GyroToRightAnalogStickMinimumStickValue), static_cast<int>(adjustedY * 127.0f));
                    }
                    else if (adjustedY < 0) {
                        accelAnalogStickY = 128 - max(static_cast<int>(settings.GyroToRightAnalogStickMinimumStickValue), static_cast<int>(-adjustedY * 128.0f));
                    }
                    else {
                        accelAnalogStickY = 128;  // Center if no movement
                    }

                    editedButtonState.RX = controller.State.RX + (accelAnalogStickX - 127);
                    editedButtonState.RY = controller.State.RY + (accelAnalogStickY - 127);
                }
            }

            if (controller.State.L2 < settings.L2Deadzone)
            {
                editedButtonState.L2 = 0;
                editedButtonState.L2Btn = false;
            }
            if (controller.State.R2 < settings.R2Deadzone)
            {
                editedButtonState.R2 = 0;
                editedButtonState.R2Btn = false;
            }

            if (settings.CurrentlyUsingUDP) {
                if (settings.L2UDPDeadzone > 0 && (controller.State.L2 < settings.L2UDPDeadzone)) {
                    editedButtonState.L2 = 0;
                    editedButtonState.L2Btn = false;
                }

                if (settings.R2UDPDeadzone > 0 && (controller.State.R2 < settings.R2UDPDeadzone)) {
                    editedButtonState.R2 = 0;
                    editedButtonState.R2Btn = false;
                }
            }

            if (settings.emuStatus != None && !v.isWorking())
            {
                MessageBox(0, "ViGEm connection failed! Are you sure you installed ViGEmBus driver?" ,"Error", 0);
                settings.emuStatus = None;
                continue;
            }

            if (settings.emuStatus == X360)
            {
                v.UpdateX360(editedButtonState);
                settings.lrumble = min(settings.MaxLeftMotor, v.rotors.lrotor);
                settings.rrumble = min(settings.MaxRightMotor, v.rotors.rrotor);
            }
            else if (settings.emuStatus == DS4)
            {
                v.UpdateDS4(editedButtonState);
                settings.lrumble = min(settings.MaxLeftMotor, v.rotors.lrotor);
                settings.rrumble = min(settings.MaxRightMotor, v.rotors.rrotor);

                if (!settings.CurrentlyUsingUDP) {
                    if (v.Red > 0 || v.Green > 0 || v.Blue > 0) {
                        controller.SetLightbar(v.Red, v.Green, v.Blue);                      
                    }
                }            
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed = end - start;
            if (elapsed < POLL_INTERVAL) {
                std::this_thread::sleep_for(POLL_INTERVAL - elapsed);
            }
        }
        else {
            auto now = std::chrono::high_resolution_clock::now();
            if (wasVirtualControllerStarted && now - lastConnectedTime > DISCONNECT_DELAY)
            {
                v.Remove360();
                v.RemoveDS4();
                lastEmu == None;
                wasVirtualControllerStarted = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

    }

    v.Free();
}


void MouseClick(DWORD flag, DWORD mouseData = 0)
{
    // Simulate mouse left button down
    INPUT input;
    input.type = INPUT_MOUSE;
    input.mi.mouseData = mouseData;
    input.mi.dwFlags = flag;  // Left button down
    input.mi.time = 0;
    input.mi.dwExtraInfo = 0;
    SendInput(1, &input, sizeof(INPUT));
}

void writeControllerState(Dualsense &controller, Settings &settings, UDP &udpServer)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    cout << "Write thread started: " << controller.GetPath() << " | "
         << settings.ControllerInput.ID << endl;

    // DISCO MODE VARIABLES
    int colorState = 0; // 0 = red to yellow, 1 = yellow to green, 2 = green to cyan, etc.
    int colorStateBattery = 0;
    int colorStateX360EMU = 0;
    int led[3];
    int step = 5; 
    led[0] = 255;
    led[1] = 0;
    led[2] = 0;
    int x360emuanimation = 0;
    bool lastMic = false;
    bool MicShortcutTriggered = false;
    bool mic = false;
    float touchpadLastX = 0;
    float touchpadLastY = 0;
    float touchpad1LastY = 0;
    bool wasR2Clicked = false;
    bool wasTouching = false;
    bool wasTouching1 = false;
    bool wasTouchpadClicked = false;
    bool X360animationplayed = false;
    bool X360readyfornew = false;
    int steps[] = {1, -1, 1, -1,1, -1, 1, -1,1, -1, 1, -1};
    int limits[] = {255, 0, 255, 0,255, 0, 255, 0,255, 0, 255, 0};
    int direction = 1; // 1 for increasing, -1 for decreasing
    DualsenseUtils::InputFeatures lastFeatures;
    bool firstTimeUDP = true;
    DualsenseUtils::InputFeatures preUDP;

    while (!stop_thread)
    {
        if (true)
        {
            if (!lastMic && MicShortcutTriggered) {
                MicShortcutTriggered = false;
            }

            if (settings.emuStatus == X360 && !X360animationplayed) {
                if (colorStateX360EMU < 20) {
                    x360emuanimation += direction * step;

                    // Reverse direction when reaching the upper or lower limits
                    if (x360emuanimation >= 255) {
                        x360emuanimation = 255; // Clamp to the max value
                        direction = -1;          // Change to decreasing
                        colorStateX360EMU++;
                    } else if (x360emuanimation <= 0) {
                        x360emuanimation = 0;    // Clamp to the min value
                        direction = 1;           // Change to increasing
                        colorStateX360EMU++;
                    }

                    controller.SetLightbar(0, x360emuanimation, 0);
                } else {
                    X360animationplayed = true;
                }          
            }
            else if (settings.emuStatus == X360 && X360readyfornew) {
                X360animationplayed = false;
            }

            if(settings.emuStatus == None && X360animationplayed) {
                X360readyfornew = true;             
                x360emuanimation = 0;
                colorStateX360EMU = 0;
            }
            else if (settings.emuStatus == DS4 && !X360animationplayed) {
                X360animationplayed = true;
            }
            else if (settings.emuStatus == None && !X360animationplayed) {
                X360animationplayed = true;
            }

            if (controller.State.micBtn && controller.State.DpadUp && settings.SwapTriggersShortcut && !MicShortcutTriggered)
            {
                settings.SwapTriggersRumbleToAT = settings.SwapTriggersRumbleToAT ? settings.SwapTriggersRumbleToAT = false : settings.SwapTriggersRumbleToAT = true;
                MicShortcutTriggered = true;
            }

            if (controller.State.micBtn && controller.State.L1 && settings.GyroToMouseShortcut && !MicShortcutTriggered) {
                settings.GyroToMouse = settings.GyroToMouse ? settings.GyroToMouse = false : settings.GyroToMouse = true;
                MicShortcutTriggered = true;
            }

            if (controller.State.micBtn && controller.State.L2 && settings.GyroToRightAnalogStickShortcut && !MicShortcutTriggered) {
                settings.GyroToRightAnalogStick = settings.GyroToRightAnalogStick ? settings.GyroToRightAnalogStick = false : settings.GyroToRightAnalogStick = true;
                MicShortcutTriggered = true;
            }

            if (controller.State.micBtn && controller.State.triangle && settings.StopWritingShortcut && !MicShortcutTriggered) {
                settings.StopWriting = settings.StopWriting ? settings.StopWriting = false : settings.StopWriting = true;
                MicShortcutTriggered = true;

                if(settings.StopWriting)
                    controller.PlayHaptics("halted");
                else
                    controller.PlayHaptics("resumed");
            }

            if (settings.emuStatus != X360 && settings.X360Shortcut && controller.State.micBtn && controller.State.DpadLeft) {
                X360animationplayed = false;
                X360readyfornew = true;
                MyUtils::RunAsyncHidHideRequest(controller.GetPath(), "hide");
                settings.emuStatus = X360;
            }

            if (settings.emuStatus != DS4 && settings.DS4Shortcut && controller.State.micBtn && controller.State.DpadRight) {
                MyUtils::RunAsyncHidHideRequest(controller.GetPath(), "hide");
                settings.emuStatus = DS4;
                controller.SetLightbar(0, 0, 64);
            }

            if (settings.emuStatus != None && settings.StopEmuShortcut && controller.State.micBtn && controller.State.DpadDown) {
                MyUtils::RunAsyncHidHideRequest(controller.GetPath(), "show");
                settings.emuStatus = None;
            }

            if (settings.TouchpadToMouseShortcut && controller.State.micBtn && controller.State.touchBtn && !MicShortcutTriggered) {
                settings.TouchpadToMouse = settings.TouchpadToMouse ? settings.TouchpadToMouse = false : settings.TouchpadToMouse = true;
                MicShortcutTriggered = true;
            }

            if (settings.R2ToMouseClick && controller.State.R2Btn) {
                if (!wasR2Clicked) {
                    MouseClick(MOUSEEVENTF_LEFTDOWN);
                    wasR2Clicked = true;
                }
            }
            else if (settings.R2ToMouseClick && !controller.State.R2Btn) {
                if (wasR2Clicked) {
                    MouseClick(MOUSEEVENTF_LEFTUP);
                    wasR2Clicked = false;
                }
            }

            if (settings.TouchpadToMouse && controller.State.trackPadTouch0.IsActive) {
                if (controller.State.touchBtn && !wasTouchpadClicked && controller.State.trackPadTouch0.X <= 1000) {
                    MouseClick(MOUSEEVENTF_LEFTDOWN);
                    wasTouchpadClicked = true;
                }
                else if (!controller.State.touchBtn && wasTouchpadClicked && controller.State.trackPadTouch0.X <= 1000) {
                    MouseClick(MOUSEEVENTF_LEFTUP);
                    wasTouchpadClicked = false;
                }

                if (controller.State.touchBtn && !wasTouchpadClicked && controller.State.trackPadTouch0.X >= 1001) {
                    MouseClick(MOUSEEVENTF_RIGHTDOWN);
                    wasTouchpadClicked = true;
                }
                else if (!controller.State.touchBtn && wasTouchpadClicked && controller.State.trackPadTouch0.X >= 1001) {
                    MouseClick(MOUSEEVENTF_RIGHTUP);
                    wasTouchpadClicked = false;
                }

                // If it wasn't previously touching, start tracking movement
                if (!wasTouching) {
                    touchpadLastX = controller.State.trackPadTouch0.X;
                    touchpadLastY = controller.State.trackPadTouch0.Y;
                }

                if (!wasTouching1) {
                    touchpad1LastY = controller.State.trackPadTouch1.Y;
                }

                float normalizedX = controller.State.trackPadTouch0.X;
                float normalizedY = controller.State.trackPadTouch0.Y;

                float deltaX = normalizedX - touchpadLastX;
                float deltaY = normalizedY - touchpadLastY;
                float delta1Y = touchpad1LastY - controller.State.trackPadTouch1.Y;

                if (controller.State.trackPadTouch1.IsActive) {
                    wasTouching1 = true;
                    if (fabs(delta1Y) > 5.0f) {
                        MouseClick(MOUSEEVENTF_WHEEL, static_cast<int>(delta1Y * 2));
                    }

                    touchpad1LastY = controller.State.trackPadTouch1.Y;
                }
                else {
                    wasTouching1 = false;
                }

                if (fabs(deltaX) > settings.swipeThreshold || fabs(deltaY) > settings.swipeThreshold) {

                    MyUtils::MoveCursor(deltaX * settings.swipeThreshold, deltaY * settings.swipeThreshold);
                }

                // Update the last touchpad position
                touchpadLastX = normalizedX;
                touchpadLastY = normalizedY;
                
                // Mark that the touchpad is active
                wasTouching = true;
            } else {
                // Reset the touch state when the touchpad is no longer active
                wasTouching = false;
            }

            if (settings.GyroToMouse) {
                if(controller.State.L2 >= settings.L2Threshold)
                {
                    int cursorDeltaX = static_cast<int>(-controller.State.accelerometer.Y * settings.GyroToMouseSensitivity);
                    int cursorDeltaY = static_cast<int>(-controller.State.accelerometer.X * settings.GyroToMouseSensitivity);

                    MyUtils::MoveCursor(cursorDeltaX, cursorDeltaY);
                }
            }

            if(!settings.CurrentlyUsingUDP && (settings.emuStatus != DS4 || settings.OverrideDS4Lightbar) && X360animationplayed)
            {
                controller.SetLightbar(settings.ControllerInput.Red,settings.ControllerInput.Green,settings.ControllerInput.Blue);
            }

            if (!settings.RumbleToAT && !settings.HapticsToTriggers && !settings.CurrentlyUsingUDP && settings.triggerFormat == "DSX") {
               controller.SetRightTrigger(
                    settings.ControllerInput.RightTriggerMode,
                    settings.R2DSXForces[0],
                    settings.R2DSXForces[1],
                    settings.R2DSXForces[2],
                    settings.R2DSXForces[3],
                    settings.R2DSXForces[4],
                    settings.R2DSXForces[5],
                    settings.R2DSXForces[6],
                    settings.R2DSXForces[7],
                    settings.R2DSXForces[8],
                    settings.R2DSXForces[9],
                    settings.R2DSXForces[10]);

                controller.SetLeftTrigger(settings.ControllerInput.LeftTriggerMode,
                    settings.L2DSXForces[0],
                    settings.L2DSXForces[1],
                    settings.L2DSXForces[2],
                    settings.L2DSXForces[3],
                    settings.L2DSXForces[4],
                    settings.L2DSXForces[5],
                    settings.L2DSXForces[6],
                    settings.L2DSXForces[7],
                    settings.L2DSXForces[8],
                    settings.L2DSXForces[9],
                    settings.L2DSXForces[10]);
            }
            else if (!settings.RumbleToAT && !settings.HapticsToTriggers && !settings.CurrentlyUsingUDP && settings.triggerFormat == "Sony") {
#pragma region Apply Sony Trigger
                TriggerSettings l2;
                
                if (settings.lmodestrSony == "Off") {
                    l2.Mode = Trigger::Rigid_B;
                    for (int i = 0; i < 11; i++) {
                       l2.Forces[i] = 0;
                    }
                }
                else if (settings.lmodestrSony == "Feedback") {
                    setTriggerSettings(l2, settings.lmodestrSony.c_str(), settings.L2Feedback, sizeof(settings.L2Feedback));
                }
                else if (settings.lmodestrSony == "Weapon") {
                    setTriggerSettings(l2, settings.lmodestrSony.c_str(), settings.L2WeaponFeedback, sizeof(settings.L2WeaponFeedback));
                }
                else if (settings.lmodestrSony == "Vibration") {
                    setTriggerSettings(l2, settings.lmodestrSony.c_str(), settings.L2VibrationFeedback, sizeof(settings.L2VibrationFeedback));
                }
                else if (settings.lmodestrSony == "Slope Feedback") {
                    setTriggerSettings(l2, settings.lmodestrSony.c_str(), settings.L2SlopeFeedback, sizeof(settings.L2SlopeFeedback));
                }
                else if (settings.lmodestrSony == "Multiple Position Feedback") {
                    setTriggerSettings(l2, settings.lmodestrSony.c_str(), settings.L2MultiplePositionFeedback, sizeof(settings.L2MultiplePositionFeedback));
                }
                else if (settings.lmodestrSony == "Multiple Position Vibration") {
                    setTriggerSettings(l2, settings.lmodestrSony.c_str(), settings.L2MultipleVibrationFeedback , sizeof(settings.L2MultipleVibrationFeedback));
                }

                TriggerSettings r2;
                r2.right = true;
                if (settings.rmodestrSony == "Off") {
                    r2.Mode = Trigger::Rigid_B;
                    for (int i = 0; i < 11; i++) {
                       r2.Forces[i] = 0;
                    }
                }
                else if (settings.rmodestrSony == "Feedback") {
                    setTriggerSettings(r2, settings.rmodestrSony.c_str(), settings.R2Feedback, sizeof(settings.R2Feedback));
                }
                else if (settings.rmodestrSony == "Weapon") {
                    setTriggerSettings(r2, settings.rmodestrSony.c_str(), settings.R2WeaponFeedback, sizeof(settings.R2WeaponFeedback));
                }
                else if (settings.rmodestrSony == "Vibration") {
                    setTriggerSettings(r2, settings.rmodestrSony.c_str(), settings.R2VibrationFeedback, sizeof(settings.R2VibrationFeedback));
                }
                else if (settings.rmodestrSony == "Slope Feedback") {
                    setTriggerSettings(r2, settings.rmodestrSony.c_str(), settings.R2SlopeFeedback, sizeof(settings.R2SlopeFeedback));
                }
                else if (settings.rmodestrSony == "Multiple Position Feedback") {
                    setTriggerSettings(r2, settings.rmodestrSony.c_str(), settings.R2MultiplePositionFeedback, sizeof(settings.R2MultiplePositionFeedback));
                }
                else if (settings.rmodestrSony == "Multiple Position Vibration") {
                    setTriggerSettings(r2, settings.rmodestrSony.c_str(), settings.R2MultipleVibrationFeedback , sizeof(settings.R2MultipleVibrationFeedback));
                }

                skipSonyTrigger:
                controller.SetRightTrigger(
                    r2.Mode,
                    r2.Forces[0],
                    r2.Forces[1],
                    r2.Forces[2],
                    r2.Forces[3],
                    r2.Forces[4],
                    r2.Forces[5],
                    r2.Forces[6],
                    r2.Forces[7],
                    r2.Forces[8],
                    r2.Forces[9],
                    r2.Forces[10]);

                controller.SetLeftTrigger(
                    l2.Mode,
                    l2.Forces[0],
                    l2.Forces[1],
                    l2.Forces[2],
                    l2.Forces[3],
                    l2.Forces[4],
                    l2.Forces[5],
                    l2.Forces[6],
                    l2.Forces[7],
                    l2.Forces[8],
                    l2.Forces[9],
                    l2.Forces[10]);
#pragma endregion
            }

            if (settings.RumbleToAT && !settings.CurrentlyUsingUDP) {
                int leftForces[3] = { 0 };
                int rightForces[3] = { 0 };

                if (settings.RumbleToAT_RigidMode) {
                    // stiffness
                    leftForces[1] = settings.SwapTriggersRumbleToAT ? min(settings.MaxRightTriggerRigid, settings.rrumble) : min(settings.MaxLeftTriggerRigid, settings.lrumble);
                    rightForces[1] = settings.SwapTriggersRumbleToAT ? min(settings.MaxLeftTriggerRigid, settings.lrumble) : min(settings.MaxRightTriggerRigid, settings.rrumble);

                    // set threshold
                    leftForces[0] = settings.SwapTriggersRumbleToAT ? 255 - settings.rrumble: 255 - settings.lrumble; 
                    rightForces[0] = settings.SwapTriggersRumbleToAT ? 255 - settings.lrumble : 255 - settings.rrumble;

                    controller.SetLeftTrigger(Trigger::Rigid, leftForces[0], leftForces[1], 0,0,0,0,0,0,0,0,0);
                    controller.SetRightTrigger(Trigger::Rigid,rightForces[0], rightForces[1], 0,0,0,0,0,0,0,0,0);
                }
                else {
                    // set frequency
                    if (!settings.SwapTriggersRumbleToAT && settings.lrumble < 25 || settings.SwapTriggersRumbleToAT && settings.rrumble < 25)
                        leftForces[0] = settings.SwapTriggersRumbleToAT ? min(settings.MaxRightRumbleToATFrequency, settings.rrumble) : min(settings.MaxLeftRumbleToATFrequency, settings.lrumble);
                    else
                        leftForces[0] = min(settings.MaxLeftRumbleToATFrequency, 25);

                    if(!settings.SwapTriggersRumbleToAT && settings.rrumble < 25 || settings.SwapTriggersRumbleToAT && settings.lrumble < 25)
                        rightForces[0] = settings.SwapTriggersRumbleToAT ? min(settings.MaxLeftRumbleToATFrequency, settings.lrumble) : min(settings.MaxRightRumbleToATFrequency, settings.rrumble);
                    else
                        rightForces[0] = min(settings.MaxRightRumbleToATFrequency, 25);

                    // set power
                    leftForces[1] = settings.SwapTriggersRumbleToAT ? min(settings.MaxRightRumbleToATIntensity, settings.rrumble) : min(settings.MaxLeftRumbleToATIntensity,settings.lrumble); 
                    rightForces[1] = settings.SwapTriggersRumbleToAT ? min(settings.MaxLeftRumbleToATIntensity,settings.lrumble) : min(settings.MaxRightRumbleToATIntensity,settings.rrumble);

                    // set static threshold
                    leftForces[2] = 20;
                    rightForces[2] = 20;

                    controller.SetLeftTrigger(Trigger::Pulse_B, leftForces[0], leftForces[1], leftForces[2],0,0,0,0,0,0,0,0);
                    controller.SetRightTrigger(Trigger::Pulse_B,rightForces[0], rightForces[1], rightForces[2],0,0,0,0,0,0,0,0);
                }           
            }

            Peaks peaks = controller.GetAudioPeaks();
            if (settings.HapticsToTriggers && !settings.CurrentlyUsingUDP) {
                if (settings.RumbleToAT && settings.lrumble > 0 || settings.RumbleToAT && settings.rrumble > 0) {
                    goto skiphapticstotriggers;
                }
         
                int ScaledLeftActuator = MyUtils::scaleFloatToInt(peaks.LeftActuator, settings.AudioToTriggersMaxFloatValue);
                int ScaledRightActuator = MyUtils::scaleFloatToInt(peaks.RightActuator, settings.AudioToTriggersMaxFloatValue);
                int leftForces[3] = { 0 };
                int rightForces[3] = { 0 };

                if (settings.RumbleToAT_RigidMode) {
                    // stiffness
                    leftForces[1] = settings.SwapTriggersRumbleToAT ? ScaledRightActuator : ScaledLeftActuator;
                    rightForces[1] = settings.SwapTriggersRumbleToAT ? ScaledLeftActuator : ScaledRightActuator;

                    // set threshold
                    leftForces[0] = settings.SwapTriggersRumbleToAT ? 255 - ScaledRightActuator: 255 - ScaledLeftActuator; 
                    rightForces[0] = settings.SwapTriggersRumbleToAT ? 255 - ScaledLeftActuator : 255 - ScaledRightActuator;

                    controller.SetLeftTrigger(Trigger::Rigid, leftForces[0], leftForces[1], 0,0,0,0,0,0,0,0,0);
                    controller.SetRightTrigger(Trigger::Rigid,rightForces[0], rightForces[1], 0,0,0,0,0,0,0,0,0);
                }
                else {
                    //cout << ScaledLeftActuator << " | " << ScaledRightActuator << endl;

                    // set frequency
                    if (!settings.SwapTriggersRumbleToAT && ScaledLeftActuator < 25 || settings.SwapTriggersRumbleToAT && ScaledRightActuator < 25)
                        leftForces[0] = settings.SwapTriggersRumbleToAT ? min(settings.MaxRightRumbleToATFrequency, ScaledRightActuator) : min(settings.MaxLeftRumbleToATFrequency, ScaledLeftActuator);
                    else
                        leftForces[0] = min(settings.MaxLeftRumbleToATFrequency, 25);

                    if(!settings.SwapTriggersRumbleToAT && ScaledRightActuator < 25 || settings.SwapTriggersRumbleToAT && ScaledLeftActuator < 25)
                        rightForces[0] = settings.SwapTriggersRumbleToAT ? min(settings.MaxLeftRumbleToATFrequency, ScaledLeftActuator) : min(settings.MaxRightRumbleToATFrequency, ScaledRightActuator);
                    else
                        rightForces[0] = min(settings.MaxRightRumbleToATFrequency, 25);

                    // set power
                    leftForces[1] = settings.SwapTriggersRumbleToAT ? min(settings.MaxRightRumbleToATFrequency, ScaledRightActuator) : min(settings.MaxLeftRumbleToATFrequency,ScaledLeftActuator); 
                    rightForces[1] = settings.SwapTriggersRumbleToAT ? min(settings.MaxLeftRumbleToATFrequency,ScaledLeftActuator) : min(settings.MaxRightRumbleToATFrequency,ScaledRightActuator);

                    // set static threshold
                    leftForces[2] = 20;
                    rightForces[2] = 20;

                    controller.SetLeftTrigger(Trigger::Pulse_B, leftForces[0], leftForces[1], leftForces[2],0,0,0,0,0,0,0,0);
                    controller.SetRightTrigger(Trigger::Pulse_B,rightForces[0], rightForces[1], rightForces[2],0,0,0,0,0,0,0,0);
                }  
            }
            skiphapticstotriggers:

            if (settings.FullyRetractTriggers) {

                if (settings.RumbleToAT || settings.RumbleToAT_RigidMode) {
                    if (settings.HapticsToTriggers && (peaks.LeftActuator > 0 || peaks.RightActuator > 0)) {
                        goto skiprumbleretract;
                    }

                    if (settings.lrumble == 0) {
                        if(!settings.SwapTriggersRumbleToAT)
                            controller.SetLeftTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                        else
                            controller.SetRightTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                    }
                    if (settings.rrumble == 0) {
                        if(!settings.SwapTriggersRumbleToAT)
                            controller.SetRightTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                        else
                            controller.SetLeftTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                    }
                }

                skiprumbleretract:
                if (settings.HapticsToTriggers)
                {
                    if (settings.lrumble > 0 || settings.rrumble > 0)
                        goto skipretract;

                    if (peaks.LeftActuator <= 0) {
                        if(!settings.SwapTriggersRumbleToAT)
                            controller.SetLeftTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                        else
                            controller.SetRightTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                    }
                    if (peaks.RightActuator <= 0) {
                        if(!settings.SwapTriggersRumbleToAT)
                            controller.SetRightTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                        else
                            controller.SetLeftTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                    }
                }               
            }
            skipretract:

            if (settings.TriggersAsButtons && !settings.CurrentlyUsingUDP && settings.emuStatus != None) {
                controller.SetLeftTrigger(Trigger::Rigid,40,255,255,255,255,255,255,255,255,255,255);
                controller.SetRightTrigger(Trigger::Rigid,40,255,255,255,255,255,255,255,255,255,255);
            }

            if (settings.AudioToLED && !settings.CurrentlyUsingUDP && (settings.emuStatus != DS4 || settings.OverrideDS4Lightbar) && X360animationplayed)
            {
                int peak = static_cast<int>(MyUtils::GetCurrentAudioPeak() * 500);

                if (settings.HapticsToLED){
                    float CurrentHighestPeak = max(peaks.LeftActuator, peaks.RightActuator);
                    peak = MyUtils::scaleFloatToInt(CurrentHighestPeak, settings.HapticsAndSpeakerToLedScale);
                }
                else if (settings.SpeakerToLED) {
                    peak = MyUtils::scaleFloatToInt(peaks.Speaker, settings.HapticsAndSpeakerToLedScale);
                }

                int r = 0, g = 0, b = 0;

                if (peak <= settings.QUIET_THRESHOLD)
                {
                    float t = static_cast<float>(peak) / settings.QUIET_THRESHOLD;
                    r = static_cast<int>(t * settings.QUIET_COLOR[0]);
                    g = static_cast<int>(t * settings.QUIET_COLOR[1]);
                    b = static_cast<int>(t * settings.QUIET_COLOR[2]);
                }
                else if (peak <= settings.MEDIUM_THRESHOLD)
                {
                    float t = static_cast<float>(peak - settings.QUIET_THRESHOLD) / (settings.MEDIUM_THRESHOLD - settings.QUIET_THRESHOLD);
                    r = static_cast<int>(settings.QUIET_COLOR[0] + t * (settings.MEDIUM_COLOR[0] - settings.QUIET_COLOR[0]));
                    g = static_cast<int>(settings.QUIET_COLOR[1] + t * (settings.MEDIUM_COLOR[1] - settings.QUIET_COLOR[1]));
                    b = static_cast<int>(settings.QUIET_COLOR[2] + t * (settings.MEDIUM_COLOR[2] - settings.QUIET_COLOR[2]));
                }
                else
                {
                    float t = static_cast<float>(peak - settings.MEDIUM_THRESHOLD) / (255 - settings.MEDIUM_THRESHOLD);
                    r = static_cast<int>(settings.MEDIUM_COLOR[0] + t * (settings.LOUD_COLOR[0] - settings.MEDIUM_COLOR[0]));
                    g = static_cast<int>(settings.MEDIUM_COLOR[1] + t * (settings.LOUD_COLOR[1] - settings.MEDIUM_COLOR[1]));
                    b = static_cast<int>(settings.MEDIUM_COLOR[2] + t * (settings.LOUD_COLOR[2] - settings.MEDIUM_COLOR[2]));
                }

                controller.SetLightbar(r, g, b);
            }

            if (settings.DiscoMode && !settings.CurrentlyUsingUDP && (settings.emuStatus != DS4 || settings.OverrideDS4Lightbar) && X360animationplayed) {
                constexpr int DEFAULT_SPEED = 1;
                int speed = settings.DiscoSpeed;
                int step = DEFAULT_SPEED * speed;

                controller.SetLightbar(led[0], led[1], led[2]);

                switch (colorState)
                {
                    case 0:
                        led[1] += step;
                        if (led[1] >= 255) colorState = 1;
                        break;

                    case 1:
                        led[0] -= step;
                        if (led[0] <= 0) colorState = 2;
                        break;

                    case 2:
                        led[2] += step;
                        if (led[2] >= 255) colorState = 3;
                        break;

                    case 3:
                        led[1] -= step;
                        if (led[1] <= 0) colorState = 4;
                        break;

                    case 4:
                        led[0] += step;
                        if (led[0] >= 255) colorState = 5;
                        break;

                    case 5:
                        led[2] -= step;
                        if (led[2] <= 0) colorState = 0;
                        break;
                }

                led[0] = std::clamp(led[0], 0, 255);
                led[1] = std::clamp(led[1], 0, 255);
                led[2] = std::clamp(led[2], 0, 255);
            }


            if (settings.BatteryLightbar && !settings.CurrentlyUsingUDP && (settings.emuStatus != DS4 || settings.OverrideDS4Lightbar) && X360animationplayed) {
                int batteryLevel = controller.State.battery.Level;

                if(batteryLevel > 20)
                {
                    batteryLevel = std::max<int>(0, std::min<int>(100, batteryLevel));

                    int red = (100 - batteryLevel) * 255 / 100;
                    int green = batteryLevel * 255 / 100;
                    int blue = 0;
                    controller.SetLightbar(red, green, blue);
                }
                else {
                    switch (colorStateBattery) {
                    case 0:
                        led[0] += step;
                        if (led[0] == 255) colorStateBattery = 1;
                        break;
                    case 1:
                        led[0] -= step;
                        if (led[0] == 0) colorStateBattery = 0;
                        break;
                    }

                    controller.SetLightbar(led[0], 0, 0);
                }

            }

            if (settings.MicScreenshot) {
                if (controller.State.micBtn && !MicShortcutTriggered && !controller.State.DpadDown && !controller.State.DpadLeft && !controller.State.DpadRight && !controller.State.DpadUp && !controller.State.touchBtn && !controller.State.triangle && !controller.State.L1 && !controller.State.L2) {
                    controller.PlayHaptics("screenshot");
                    MyUtils::TakeScreenShot();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    filesystem::create_directories(MyUtils::GetImagesFolderPath() + "\\DualSenseY\\");
                    string filename = MyUtils::GetImagesFolderPath() + "\\DualSenseY\\" + MyUtils::currentDateTimeWMS() + ".bmp";
                    MyUtils::SaveBitmapFromClipboard(filename.c_str());
                    MicShortcutTriggered = true;
                }
            }

            if (settings.MicFunc) {
                if (controller.State.micBtn && !MicShortcutTriggered) {
                    if (!mic) {
                        controller.SetMicrophoneLED(true, false);
                        controller.SetMicrophoneVolume(0);
                        mic = true;
                    }               
                    else{
                        controller.SetMicrophoneLED(false, false);
                        controller.SetMicrophoneVolume(80);
                        mic = false;
                    }
                    MicShortcutTriggered = true;
                }
            }

            if (udpServer.isActive && settings.UseUDP == true)
            {
                if (firstTimeUDP)
                {
                    preUDP = settings.ControllerInput;
                    firstTimeUDP = false;
                }
                settings.CurrentlyUsingUDP = true;
                settings.L2UDPDeadzone = udpServer.thisSettings.L2UDPDeadzone;
                settings.R2UDPDeadzone = udpServer.thisSettings.R2UDPDeadzone;
                udpServer.Battery = controller.State.battery.Level;
                controller.SetLightbar(
                    udpServer.thisSettings.ControllerInput.Red,
                    udpServer.thisSettings.ControllerInput.Green,
                    udpServer.thisSettings.ControllerInput.Blue);
                controller.SetLeftTrigger(
                    udpServer.thisSettings.ControllerInput.LeftTriggerMode,
                    udpServer.thisSettings.ControllerInput.LeftTriggerForces[0],
                    udpServer.thisSettings.ControllerInput.LeftTriggerForces[1],
                    udpServer.thisSettings.ControllerInput.LeftTriggerForces[2],
                    udpServer.thisSettings.ControllerInput.LeftTriggerForces[3],
                    udpServer.thisSettings.ControllerInput.LeftTriggerForces[4],
                    udpServer.thisSettings.ControllerInput.LeftTriggerForces[5],
                    udpServer.thisSettings.ControllerInput.LeftTriggerForces[6],
                    udpServer.thisSettings.ControllerInput.LeftTriggerForces[7],
                    udpServer.thisSettings.ControllerInput.LeftTriggerForces[8],
                    udpServer.thisSettings.ControllerInput.LeftTriggerForces[9],
                    udpServer.thisSettings.ControllerInput
                        .LeftTriggerForces[10]);

                controller.SetRightTrigger(
                    udpServer.thisSettings.ControllerInput.RightTriggerMode,
                    udpServer.thisSettings.ControllerInput
                        .RightTriggerForces[0],
                    udpServer.thisSettings.ControllerInput
                        .RightTriggerForces[1],
                    udpServer.thisSettings.ControllerInput
                        .RightTriggerForces[2],
                    udpServer.thisSettings.ControllerInput
                        .RightTriggerForces[3],
                    udpServer.thisSettings.ControllerInput
                        .RightTriggerForces[4],
                    udpServer.thisSettings.ControllerInput
                        .RightTriggerForces[5],
                    udpServer.thisSettings.ControllerInput
                        .RightTriggerForces[6],
                    udpServer.thisSettings.ControllerInput
                        .RightTriggerForces[7],
                    udpServer.thisSettings.ControllerInput
                        .RightTriggerForces[8],
                    udpServer.thisSettings.ControllerInput
                        .RightTriggerForces[9],
                    udpServer.thisSettings.ControllerInput
                        .RightTriggerForces[10]);

                while (!udpServer.thisSettings.AudioPlayQueue.empty()) {
                    AudioPlay& play = udpServer.thisSettings.AudioPlayQueue.front();

                    controller.LoadSound("UDP-" + play.File, play.File);
                    controller.PlayHaptics("UDP-" + play.File, play.DontPlayIfAlreadyPlaying, play.Loop);

                    cout << "Play: " << play.File << " | " << play.DontPlayIfAlreadyPlaying << " | " << play.Loop << endl;
                    udpServer.thisSettings.AudioPlayQueue.erase(udpServer.thisSettings.AudioPlayQueue.begin());
                }

                while (!udpServer.thisSettings.AudioEditQueue.empty())
                {
                    AudioEdit &edit = udpServer.thisSettings.AudioEditQueue.front();

                    switch (edit.EditType)
                    {
                    case AudioEditType::Pitch:
                        controller.SetSoundPitch("UDP-" + edit.File,
                                                 edit.Value);
                        break;
                    case AudioEditType::Volume:
                        controller.SetSoundVolume("UDP-" + edit.File,
                                                  edit.Value);
                        break;
                    case AudioEditType::Stop:
                        controller.StopHaptics("UDP-" + edit.File);
                        break;
                    case AudioEditType::StopAll:
                        controller.StopAllHaptics();
                        break;
                    }

                    //cout << "Edit: " << edit.File << " | " << edit.EditType << " | " << edit.Value << endl;
                    udpServer.thisSettings.AudioEditQueue.erase(udpServer.thisSettings.AudioEditQueue.begin());
                }

                std::chrono::high_resolution_clock::time_point Now =
                    std::chrono::high_resolution_clock::now();
                if ((Now - udpServer.LastTime) > 8s)
                {
                    udpServer.isActive = false;
                    settings.CurrentlyUsingUDP = false;
                    firstTimeUDP = true;
                    settings.ControllerInput = preUDP;
                    controller.StopSoundsThatStartWith("UDP-");
                }
            }
      
            if (settings.lrumble > 0 || settings.rrumble > 0)
            {
                controller.UseRumbleNotHaptics(true);
            }
            else {
                controller.UseRumbleNotHaptics(false);
            }

            if (settings.DisablePlayerLED) {
                controller.SetPlayerLED(0);
            }

            controller.SetSpeakerVolume(settings.ControllerInput.SpeakerVolume);
            controller.SetHeadsetVolume(settings.ControllerInput.HeadsetVolume);
            controller.UseHeadsetNotSpeaker(settings.UseHeadset);
            controller.SetRumble(settings.lrumble, settings.rrumble);

            if(controller.Connected)
            {
                if (!settings.StopWriting) {
                    controller.Write();
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }

        lastMic = controller.State.micBtn;
    }
}

void setTaskbarIcon(GLFWwindow* window) {
    int width, height, nrChannels;
    unsigned char* img = stbi_load("utilities/icon.png", &width, &height, &nrChannels, 4);
    
    if (img) {
        GLFWimage icon;
        icon.width = width;
        icon.height = height;
        icon.pixels = img;

        // Set the window icon
        glfwSetWindowIcon(window, 1, &icon);

        // Free the image memory after setting the icon
        stbi_image_free(img);
    } else {
        // Handle error loading image
        printf("Failed to load icon\n");
    }
}

static bool IsMinimized = false;
void window_iconify_callback(GLFWwindow* window, int iconified) {
    if (iconified) {
        IsMinimized = true;
    } else {
        IsMinimized = false;
    }
}

void mTray(Tray::Tray &tray, Config::AppConfig &AppConfig) {
    tray.addEntry(Tray::Button("Show window", [&] {
        AppConfig.ShowWindow = true;
    }));

    tray.run();
}

int main()
{
    timeBeginPeriod(1);
    
    Config::AppConfig appConfig = Config::ReadAppConfigFromFile();
    
    if(appConfig.ShowConsole == false)
    {
        FreeConsole();
    }

    bool UpdateAvailable = false;
    if(!appConfig.SkipVersionCheck)
    {
        try {
            std::cout << "Attempting to download version string" << std::endl;
            appConfig.SkipVersionCheck = true;
            Config::WriteAppConfigToFile(appConfig);
            cpr::Response response = cpr::Get(cpr::Url("https://raw.githubusercontent.com/WujekFoliarz/DualSenseY-v2/master/version"), cpr::Verbose(true));
            std::cout << "Download completed, status code: " << response.status_code << std::endl;

            if (response.status_code == 200) {
                if (response.text != "") {
                    appConfig.SkipVersionCheck = false;
                    Config::WriteAppConfigToFile(appConfig);

                    try {
                        int version = stoi(response.text);
                        if (version > VERSION) {
                            cout << "Update available!" << endl;
                            UpdateAvailable = true;
                        }
                        else {
                            cout << "Application is up-to-date! Remote version: " << version << endl;
                        }
                    }
                    catch (const std::invalid_argument& e) {
                        cout << "Invalid version string: " << e.what() << endl;
                    }
                    catch (const std::out_of_range& e) {
                        cout << "Version number out of range: " << e.what() << endl;
                    }
                }
                else {
                    cout << "Version string is empty" << endl;
                }
            }
            else {
                std::cout << "Version could not be downloaded, status code: " << response.status_code << std::endl;
            }
        }
        catch (const std::exception& e) {
            cout << "Standard exception: " << e.what() << endl;
        }
        catch (...) {
            cout << "Unknown exception occurred during download" << endl;
        }
    }

    if (appConfig.ElevateOnStartup) {
        MyUtils::ElevateNow();
    }
    else {
        cout << "Not elevating" << endl;
    }
    

    Tray::Tray tray("test", "utilities\\icon.ico");
    std::thread trayThread(mTray, std::ref(tray), std::ref(appConfig));
    trayThread.detach();

    bool WasElevated = MyUtils::IsRunAsAdministrator();

    // Write default english to file
    Strings enStrings;
    nlohmann::json enJson = enStrings.to_json();
    filesystem::create_directories(MyUtils::GetExecutableFolderPath() + "\\localizations\\");
    std::ofstream EnglishFILE(MyUtils::GetExecutableFolderPath() + "\\localizations\\en.json");  
    EnglishFILE << enJson.dump(4);
    EnglishFILE.close();
    enJson.clear();

    float soundpan = 1;
    
    Strings strings = ReadStringsFromFile(appConfig.Language); // Load language file

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    int InstancesCount = 0;
    if (Process32First(snapshot, &entry) == TRUE)
    {
        while (Process32Next(snapshot, &entry) == TRUE)
        {
            if (strcmp(entry.szExeFile, "DSX.exe") == 0)
            {
                InstancesCount++;
            }
        }
    }

    if (InstancesCount > 1) {
        MessageBox(0, "DSY/DSX is already running!", "Error", 0);
        return 1;
    }

    CloseHandle(snapshot);

   
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
        MessageBox(0, "GLFW could not be initialized!", "Error", 0);
        return 1;
    }

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char *glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char *glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    string title = "DualSenseY - Version " + std::to_string(VERSION);
    // Create window with graphics context
    auto *window = glfwCreateWindow(static_cast<std::int32_t>(1200),
                                    static_cast<std::int32_t>(900),
                                    title.c_str(),
                                    nullptr,
                                    nullptr);
    if (window == nullptr)
    {
        MessageBox(0, "Window could not be created!", "Error", 0);
        return 1;
    }

  
    glfwMakeContextCurrent(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    int imageWidth, imageHeight, channels;
    stbi_uc* backgroundTexture = stbi_load("textures\\background.png", &imageWidth, &imageHeight, &channels, 4); // Load as RGBA
    bool BackgroundTextureLoaded = false;

    if (!backgroundTexture) {
        // Error handling
        printf("Failed to load texture\n");
    }
    else {
        BackgroundTextureLoaded = true;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imageWidth, imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, backgroundTexture);

    stbi_image_free(backgroundTexture); // Free the image data after loading into GPU
    
    deque<Settings> ControllerSettings;
    vector<thread> readThreads;  // Store the threads for each controller
    vector<thread> writeThreads; // Audio to LED threads
    vector<thread> emuThreads;
    std::string CurrentController = "";
    bool firstController = true;
    vector<std::string> ControllerID = DualsenseUtils::EnumerateControllerIDs();
    
    int ControllerCount = DualsenseUtils::GetControllerCount();
    std::chrono::high_resolution_clock::time_point LastControllerCheck = std::chrono::high_resolution_clock::now();

    MyUtils::InitializeAudioEndpoint();

    UDP udpServer;

    ImGuiIO &io = ImGui::GetIO();

     static const ImWchar polishGlyphRange[] = {
                0x0020, 0x00FF, // Basic Latin + Latin Supplement
                0x0100, 0x017F, // Latin Extended-A
                0x0180, 0x024F, // Latin Extended-B (if you need even more coverage)
                0 };
      if (appConfig.Language == "ja")    
        ImFont* font_title = io.Fonts->AddFontFromFileTTF(std::string(MyUtils::GetExecutableFolderPath() + "\\fonts\\NotoSansJP-Bold.ttf").c_str(), 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
      else if (appConfig.Language == "zh")
        ImFont* font_title = io.Fonts->AddFontFromFileTTF(std::string(MyUtils::GetExecutableFolderPath() + "\\fonts\\NotoSansSC-Bold.otf").c_str(), 18.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
      else if (appConfig.Language == "ko")
        ImFont* font_title = io.Fonts->AddFontFromFileTTF(std::string(MyUtils::GetExecutableFolderPath() + "\\fonts\\NotoSansKR-Bold.ttf").c_str(), 18.0f, NULL, io.Fonts->GetGlyphRangesKorean());
      else if (appConfig.Language == "ru")
        ImFont* font_title = io.Fonts->AddFontFromFileTTF(std::string(MyUtils::GetExecutableFolderPath() + "\\fonts\\NotoSansRU-Bold.ttf").c_str(), 18.0f, NULL, io.Fonts->GetGlyphRangesCyrillic());
      else if (appConfig.Language == "vi")
          ImFont* font_title = io.Fonts->AddFontFromFileTTF(std::string(MyUtils::GetExecutableFolderPath() + "\\fonts\\Roboto-Bold.ttf").c_str(), 18.0f, NULL, io.Fonts->GetGlyphRangesVietnamese());       
      else {
        ImFont* font_title = io.Fonts->AddFontFromFileTTF(std::string(MyUtils::GetExecutableFolderPath() + "\\fonts\\Roboto-Bold.ttf").c_str(), 18.0f, NULL, polishGlyphRange);
      }

    std::vector<const char*> languageItems;
    for (const auto& lang : languages) {
        languageItems.push_back(lang.c_str());
    }
    int selectedLanguageIndex = 0;

    // set language index
    for (int i = 0; i < languages.size(); i++) {
        if (languages[i] == appConfig.Language) {
            selectedLanguageIndex = i;
            break;
        }
    }
    
    setTaskbarIcon(window);
    glfwSetWindowIconifyCallback(window, window_iconify_callback);
    bool WindowHiddenOnStartup = false;
  
    if (appConfig.HideWindowOnStartup) {
        IsMinimized = true;
        WindowHiddenOnStartup = true;
        appConfig.ShowWindow = false;
    }

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.3f;
    style.FrameRounding = 2.3f;
    style.ScrollbarRounding = 0;
    if(appConfig.DefaultStyleFilepath != "")
        MyUtils::LoadImGuiColorsFromFilepath(style, appConfig.DefaultStyleFilepath);

    HWND hwnd = glfwGetWin32Window(window);
    BOOL useDarkMode = TRUE;

    glfwSwapInterval(0);

    if (useDarkMode) {
        if (FAILED(DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode)))) {
            fprintf(stderr, "Failed to set dark mode for window.\n");
        }

        DWORD color = 0x000000; 
        if (FAILED(DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &color, sizeof(color)))) {
            fprintf(stderr, "Failed to set caption color.\n");
        }
    }

    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    int screenWidth = mode->width;
    int screenHeight = mode->height;
    bool ShowStyleEditWindow = false;
    float MainMenuBarHeight = 0;

    static bool show_popup = false;
    while (!glfwWindowShouldClose(window))
    {
        int windowWidth, windowHeight;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);

        if (IsMinimized && appConfig.HideToTray || IsMinimized && !appConfig.HideToTray && appConfig.HideWindowOnStartup && WindowHiddenOnStartup) {
            glfwHideWindow(window);
            WindowHiddenOnStartup = false;       
        }

        if (appConfig.ShowWindow) {
            glfwFocusWindow(window);
            glfwRestoreWindow(window);
            appConfig.ShowWindow = false;
            IsMinimized = false;
        }

        start_cycle();
        ImGui::NewFrame();
    
        // Get the viewport size (size of the application window)
        
        ImVec2 window_size = io.DisplaySize;

        // Set window flags to remove the ability to resize, move, or close the window
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing;

        // Begin the "Main" window and set its size and position
        ImGui::SetNextWindowSize(window_size);
        ImGui::SetNextWindowPos(
            ImVec2(0, 0)); // Position at the top-left corner
 
        io.FontGlobalScale = MyUtils::CalculateScaleFactor(screenHeight, screenHeight);  // Adjust the scale factor as needed

        if(BackgroundTextureLoaded)
        {
            if (ImGui::Begin("Background", nullptr, window_flags)) {
                ImVec2 availSize = ImGui::GetContentRegionAvail();
                ImGui::Image((void*)(intptr_t)textureID, availSize);
                ImGui::End();
            }
        }

        bool ShowNonControllerConfig = true;
       
        if (ShowStyleEditWindow) {

            if (ImGui::Begin("Style editor", &ShowStyleEditWindow, ImGuiWindowFlags_NoCollapse)) {

                if (!ImGui::IsWindowFocused()) {
                    ShowStyleEditWindow = false;
                }

              
                                    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
                                        ImVec4* color = &style.Colors[i];
                                        const char* colorName = ImGui::GetStyleColorName(i);

                                        ImGui::Text("%s:", colorName);
                                        ImGui::SameLine();

                                        ImGui::PushItemWidth(ImGui::GetWindowWidth() / 10.0f - 10);
                                        ImGui::SliderFloat(std::string(std::string("R##") + colorName).c_str(), &color->x, 0.0f, 1.0f);
                                        ImGui::SameLine();
                                        ImGui::SliderFloat(std::string(std::string("G##") + colorName).c_str(), &color->y, 0.0f, 1.0f);
                                        ImGui::SameLine();
                                        ImGui::SliderFloat(std::string(std::string("B##") + colorName).c_str(), &color->z, 0.0f, 1.0f);
                                        ImGui::SameLine();
                                        ImGui::SliderFloat(std::string(std::string("A##") + colorName).c_str(), &color->w, 0.0f, 1.0f);
                                        ImGui::PopItemWidth();
                                    }
            }
        }

        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(windowWidth), static_cast<float>(windowHeight-MainMenuBarHeight)));
        ImGui::SetNextWindowPos(ImVec2(0, MainMenuBarHeight));
        if (ImGui::Begin("Main", nullptr, window_flags))
        {
            // Limit these functions for better CPU usage 
            std::chrono::high_resolution_clock::time_point Now = std::chrono::high_resolution_clock::now();
            if ((Now - LastControllerCheck) > 3s) {
                LastControllerCheck = std::chrono::high_resolution_clock::now();
                ControllerID.clear();
                ControllerID = DualsenseUtils::EnumerateControllerIDs();
                ControllerCount = DualsenseUtils::GetControllerCount();
            }
          
            if (ImGui::BeginCombo("##", CurrentController.c_str()))
            {
                for (int n = 0; n < ControllerID.size(); n++)
                {
                    if (ControllerID[n] != "") {
                        bool is_selected = (CurrentController == ControllerID[n]);
                        if (ImGui::Selectable(ControllerID[n].c_str(), is_selected))
                        {
                            CurrentController = ControllerID[n].c_str();
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }

                }
                ImGui::EndCombo();
            }

            if (!udpServer.isActive)
            {
                ImGui::SameLine();
                std::string udpStatusString = std::string(strings.UDPStatus + ": " + strings.Inactive);
            ImGui::Text(udpStatusString.c_str());
            ImGui::SameLine();
            static float color[3] = {255, 0, 0};
            ImGui::ColorEdit3("##",
                              color,
                              ImGuiColorEditFlags_NoAlpha |
                                  ImGuiColorEditFlags_NoPicker |
                                  ImGuiColorEditFlags_NoOptions |
                                  ImGuiColorEditFlags_NoInputs |
                                  ImGuiColorEditFlags_NoTooltip |
                                  ImGuiColorEditFlags_NoLabel |
                                  ImGuiColorEditFlags_NoSidePreview |
                                  ImGuiColorEditFlags_NoDragDrop |
                                  ImGuiColorEditFlags_NoBorder);
            }
            else
            {
                ImGui::SameLine();
            ImGui::Text(std::string(strings.UDPStatus + ": " + strings.Active).c_str());
            ImGui::SameLine();
            static float color[3] = {0, 255, 0};
            ImGui::ColorEdit3("##",
                              color,
                              ImGuiColorEditFlags_NoAlpha |
                                  ImGuiColorEditFlags_NoPicker |
                                  ImGuiColorEditFlags_NoOptions |
                                  ImGuiColorEditFlags_NoInputs |
                                  ImGuiColorEditFlags_NoTooltip |
                                  ImGuiColorEditFlags_NoLabel |
                                  ImGuiColorEditFlags_NoSidePreview |
                                  ImGuiColorEditFlags_NoDragDrop |
                                  ImGuiColorEditFlags_NoBorder);
            }
                      
            int nloop = 0;
            for (std::string &id : ControllerID)
            {
                bool IsPresent = false;

                for (Dualsense &ds : DualSense)
                {
                    if (id == ds.GetPath())
                    {
                        IsPresent = true;
                        break;
                    }
                    else
                    {
                        nloop++;
                    }
                }

                if (!IsPresent)
                {
                        Dualsense x = Dualsense(id.c_str());                       
                        if (x.Connected)
                        {
                            DualSense.push_back(x);
                            
                            // Read default config if present
                            std::string configPath = "";
                            MyUtils::GetConfigPathForController(configPath, id);
                            Settings s;
                            if (configPath != "") {
                                s = Config::ReadFromFile(configPath);
                                if (s.emuStatus != None) {
                                    MyUtils::RunAsyncHidHideRequest(DualSense.back().GetPath(), "hide");
                                }
                            }

                            ControllerSettings.emplace_back(s);
                            ControllerSettings.back().ControllerInput.ID = id;

                            if (firstController)
                            {
                                ControllerSettings.back().UseUDP = true;
                                firstController = false;
                                CurrentController = id.c_str();
                            }

                            readThreads.emplace_back(readControllerState,
                                                     std::ref(DualSense.back()),
                                                     std::ref(ControllerSettings.back()));

                            writeThreads.emplace_back(writeControllerState,
                                                      std::ref(DualSense.back()),
                                                      std::ref(ControllerSettings.back()),
                                                      std::ref(udpServer));

                            emuThreads.emplace_back(writeEmuController,
                                                    std::ref(DualSense.back()),
                                                    std::ref(ControllerSettings.back()));
                        }

                }
            }

            int ActiveDualsenseCount = DualSense.size();
            ImGui::Separator();

            for (int i = 0; i < ActiveDualsenseCount; i++)
            {
                DualSense[i].Reconnect();
                DualSense[i].InitializeAudioEngine();
                DualSense[i].LoadSound("screenshot", "sounds\\screenshot.wav");
                DualSense[i].LoadSound("halted", "sounds\\halted.wav");
                DualSense[i].LoadSound("resumed", "sounds\\resumed.wav");
                DualSense[i].SetPlayerLED(i + 1);

                if (DualSense[i].GetPath() == CurrentController && DualSense[i].Connected)
                {                  
                    for (Settings &s : ControllerSettings)
                    {
                        if (s.ControllerInput.ID == CurrentController)
                        {
                            if(ImGui::BeginMainMenuBar())
                            {                              
                                if (ImGui::BeginMenu(strings.ConfigHeader.c_str())) {
                                    if (ImGui::Combo(strings.Language.c_str(), &selectedLanguageIndex, languageItems.data(), languageItems.size())) {
                                        const std::string& selectedLanguage = languages[selectedLanguageIndex];
                                        appConfig.Language = selectedLanguage;
                                        Config::WriteAppConfigToFile(appConfig);
                                    }

                                    if (ImGui::Button(strings.SaveConfig.c_str())) {
                                        Config::WriteToFile(s);
                                    }
                                    Tooltip(strings.Tooltip_SaveConfig.c_str());

                                    if (ImGui::Button(strings.LoadConfig.c_str())) {
                                        std::string configPath = Config::GetConfigPath();
                                        if (configPath != "") {
                                            s = Config::ReadFromFile(configPath);
                                            if (s.emuStatus == None) {
                                                MyUtils::RunAsyncHidHideRequest(DualSense[i].GetPath(), "show");
                                            }
                                            else {
                                                MyUtils::RunAsyncHidHideRequest(DualSense[i].GetPath(), "hide");
                                            }
                                            s.ControllerInput.ID = DualSense[i].GetPath();
                                        }
                                    }
                                    Tooltip(strings.Tooltip_LoadConfig.c_str());

                                    if (ImGui::Button(strings.SetDefaultConfig.c_str())) {
                                        std::string configPath = Config::GetConfigPath();
                                        if (configPath != "") {
                                            MyUtils::WriteDefaultConfigPath(configPath, DualSense[i].GetPath());
                                        }
                                    }
                                    Tooltip(strings.Tooltip_SetDefaultConfig.c_str());

                                    if (ImGui::Button(strings.RemoveDefaultConfig.c_str())) {
                                        MyUtils::RemoveConfig(DualSense[i].GetPath());
                                    }
                                    Tooltip(strings.Tooltip_RemoveDefaultConfig.c_str());

                                    if (ImGui::Checkbox(strings.RunAsAdmin.c_str(), &appConfig.ElevateOnStartup)) {
                                        Config::WriteAppConfigToFile(appConfig);

                                        if (!MyUtils::IsRunAsAdministrator()) {
                                            MyUtils::ElevateNow();
                                        }
                                    }
                                    Tooltip(strings.Tooltip_RunAsAdmin.c_str());

                                    if (ImGui::Checkbox(strings.HideToTray.c_str(), &appConfig.HideToTray)) {
                                        Config::WriteAppConfigToFile(appConfig);
                                    }
                                    Tooltip(strings.Tooltip_HideToTray.c_str());

                                    if (ImGui::Checkbox(strings.HideWindowOnStartup.c_str(), &appConfig.HideWindowOnStartup)) {
                                        Config::WriteAppConfigToFile(appConfig);
                                    }
                                    Tooltip(strings.Tooltip_HideWindowOnStartup.c_str());

                                    if (ImGui::Checkbox(strings.RunWithWindows.c_str(), &appConfig.RunWithWindows)) {
                                        if (appConfig.RunWithWindows) {
                                            MyUtils::AddToStartup();
                                        }
                                        else {
                                            MyUtils::RemoveFromStartup();
                                        }

                                        Config::WriteAppConfigToFile(appConfig);
                                    }
                                    Tooltip(strings.RunWithWindows.c_str());

                                    if (ImGui::Checkbox(strings.ShowConsole.c_str(), &appConfig.ShowConsole)) {
                                        Config::WriteAppConfigToFile(appConfig);
                                    }

                                    ImGui::EndMenu();
                                }

                                if (ImGui::BeginMenu(strings.Style.c_str())) {

                                    if (ImGui::Button(strings.SaveStyleToFile.c_str())) {
                                        MyUtils::SaveImGuiColorsToFile(style);
                                    }

                                    if (ImGui::Button(strings.LoadStyleFromFile.c_str())) {
                                        MyUtils::LoadImGuiColorsFromFile(style);
                                    }

                                    if (ImGui::Button(strings.SetDefaultStyleFile.c_str())) {
                                        std::string stylePath = Config::GetStylePath();
                                        if (stylePath != "") {
                                            appConfig.DefaultStyleFilepath = stylePath;
                                            Config::WriteAppConfigToFile(appConfig);
                                        }
                                    }

                                    if (ImGui::Button(strings.RemoveDefaultStyle.c_str())) {
                                        appConfig.DefaultStyleFilepath = "";
                                        Config::WriteAppConfigToFile(appConfig);
                                    }

                                    if (ImGui::Button(strings.ResetStyle.c_str())) {
                                        style = ImGuiStyle();
                                    }

                                    if (ImGui::Button("Show edit menu")) {
                                        ShowStyleEditWindow = true;
                                    }
                                    ImGui::EndMenu();
                                }
                                MainMenuBarHeight = ImGui::GetFrameHeight();
                                ImGui::EndMainMenuBar();
                             
                            }
                            
                           

                            DualSense[i].SetSoundVolume("screenshot", s.ScreenshotSoundLoudness);
                            DualsenseUtils::HapticFeedbackStatus HF_status = DualSense[i].GetHapticFeedbackStatus();
                            const char* bt_or_usb = DualSense[i].GetConnectionType() == Feature::USB ? "USB" : "BT";
                            ImGui::Text("%s %d | %s: %s | %s: %d%%", strings.ControllerNumberText.c_str(),i+1, strings.USBorBTconnectionType.c_str(), bt_or_usb, strings.BatteryLevel.c_str(), DualSense[i].State.battery.Level);

                            if (UpdateAvailable) {
                                ImGui::SameLine();
                                if (ImGui::Button(strings.InstallLatestUpdate.c_str())) {
                                    std::cout << "Downloading updater.exe" << std::endl;
                                    cpr::Response response = cpr::Get(cpr::Url{ "https://github.com/WujekFoliarz/DualSenseY-v2/raw/refs/heads/master/Updater.exe" });
                                    if (response.status_code == 200) {
                                        std::ofstream outFile("Updater.exe", std::ios::binary);
                                        if (outFile) {
                                            outFile << response.text;  // Write the response content to the file
                                            outFile.close();
                                            std::cout << "File downloaded successfully: Updater.exe" << std::endl;
                                        } else {
                                            std::cerr << "Error opening file for writing." << std::endl;
                                        }
                                    } else {
                                        // Handle HTTP errors
                                        std::cerr << "Failed to download file. HTTP status code: " << response.status_code << std::endl;
                                    } 

                                    if(filesystem::exists("Updater.exe")) {
                                       system("start Updater.exe");
                                       _exit(0);
                                    }
                                }
                            }                            

                            if (s.StopWriting) {
                                ImGui::TextColored(ImVec4(1, 0, 0, 1), strings.CommunicationPaused.c_str());
                                ImGui::SameLine();
                                if (ImGui::SmallButton(strings.Resume.c_str())) {
                                    s.StopWriting = false;
                                    DualSense[i].PlayHaptics("resumed");
                                }
                            }

                            if (!udpServer.isActive || !s.UseUDP)
                            {
                                if((s.emuStatus != DS4 || s.OverrideDS4Lightbar))
                                {
                                    if (ImGui::CollapsingHeader(strings.LedSection.c_str())) {
                                        ImGui::Separator();

                                        ImGui::Checkbox(strings.DisablePlayerLED.c_str(), &s.DisablePlayerLED);

                                        if (!s.DiscoMode && !s.BatteryLightbar) {
                                            ImGui::Checkbox(strings.AudioToLED.c_str(), &s.AudioToLED);                                        
                                            Tooltip(strings.Tooltip_AudioToLED.c_str());
                                        }

                                        if (!s.AudioToLED && !s.BatteryLightbar) {
                                            ImGui::Checkbox(strings.DiscoMode.c_str(), &s.DiscoMode);                                          
                                            Tooltip(strings.Tooltip_DiscoMode.c_str());
                                            ImGui::SameLine();
                                            ImGui::SetNextItemWidth(100);
                                            ImGui::SliderInt(std::string(strings.Speed + "##Disco").c_str(), &s.DiscoSpeed, 1, 10);
                                        }

                                        if (s.AudioToLED) {
                                            ImGui::Separator();
                                            if(HF_status == DualsenseUtils::HapticFeedbackStatus::Working)
                                            {
                                                if (!s.SpeakerToLED) {
                                                    ImGui::Checkbox(strings.UseControllerActuatorsInsteadOfSystemAudio.c_str(), &s.HapticsToLED);
                                                }
                                                if (!s.HapticsToLED) {
                                                    ImGui::Checkbox(strings.UseControllerSpeakerInsteadOfSystemAudio.c_str(), &s.SpeakerToLED);
                                                }
                                                if (s.SpeakerToLED || s.HapticsToLED) {
                                                    ImGui::SameLine();
                                                    ImGui::SetNextItemWidth(100);
                                                    ImGui::SliderFloat(strings.Scale.c_str(), &s.HapticsAndSpeakerToLedScale, 0.01f, 1.0f);
                                                }
                                            }
                                            else if (DualsenseUtils::HapticFeedbackStatus::BluetoothNotUSB) {
                                                s.SpeakerToLED = false;
                                                s.HapticsToLED = false;
                                            }

                                            ImGui::Text(strings.QuietColor.c_str());
                                            ImGui::SliderInt(string(strings.LED_Red + "##q").c_str(), &s.QUIET_COLOR[0], 0, 255);
                                            ImGui::SliderInt(string(strings.LED_Green + "##q").c_str(), &s.QUIET_COLOR[1], 0, 255);
                                            ImGui::SliderInt(string(strings.LED_Blue + "##q").c_str(), &s.QUIET_COLOR[2], 0, 255);
                                            ImGui::Spacing();

                                            ImGui::Text(strings.MediumColor.c_str());
                                            ImGui::SliderInt(string(strings.LED_Red + "##m").c_str(), &s.MEDIUM_COLOR[0], 0, 255);
                                            ImGui::SliderInt(string(strings.LED_Green + "##m").c_str(), &s.MEDIUM_COLOR[1], 0, 255);
                                            ImGui::SliderInt(string(strings.LED_Blue + "##m").c_str(), &s.MEDIUM_COLOR[2], 0, 255);
                                            ImGui::Spacing();

                                            ImGui::Text(strings.LoudColor.c_str());
                                            ImGui::SliderInt(string(strings.LED_Red + "##l").c_str(), &s.LOUD_COLOR[0], 0, 255);
                                            ImGui::SliderInt(string(strings.LED_Green + "##l").c_str(), &s.LOUD_COLOR[1], 0, 255);
                                            ImGui::SliderInt(string(strings.LED_Blue + "##l").c_str(), &s.LOUD_COLOR[2], 0, 255);
                                            ImGui::Spacing();

                                            ImGui::Separator();
                                        }

                                        if (!s.DiscoMode && !s.AudioToLED) {
                                            ImGui::Checkbox(strings.BatteryLightbar.c_str(), &s.BatteryLightbar);
                                            Tooltip(strings.Tooltip_BatteryLightbar.c_str());
                                        }

                                        if (!s.AudioToLED && !s.DiscoMode && !s.BatteryLightbar) {
                                            ImGui::SliderInt(strings.LED_Red.c_str(),
                                                &s.ControllerInput.Red,
                                                0,
                                                255);
                                            ImGui::SliderInt(strings.LED_Green.c_str(),
                                                &s.ControllerInput.Green,
                                                0,
                                                255);
                                            ImGui::SliderInt(strings.LED_Blue.c_str(),
                                                &s.ControllerInput.Blue,
                                                0,
                                                255);
                                        }

                                        ImGui::Separator();
                                    }
                                }
                                else {
                                    ImGui::Text(strings.LED_DS4_Unavailable.c_str());
                                }

                            if (ImGui::CollapsingHeader(strings.AdaptiveTriggers.c_str()))
                            {

                                ImGui::Checkbox(strings.RumbleToAT.c_str(), &s.RumbleToAT);
                                Tooltip(strings.Tooltip_RumbleToAT.c_str());

                                if(HF_status == DualsenseUtils::HapticFeedbackStatus::Working){
                                    ImGui::Checkbox(strings.HapticsToTriggers.c_str(), &s.HapticsToTriggers);
                                    Tooltip(strings.Tooltip_HapticsToTriggers.c_str());
                                    if(s.HapticsToTriggers)
                                    {
                                        ImGui::SameLine();
                                        ImGui::SetNextItemWidth(100);
                                        ImGui::SliderFloat(strings.Scale.c_str(), &s.AudioToTriggersMaxFloatValue, 0.01f, 10.0f);
                                    }
                                }
                               

                                if (s.RumbleToAT || s.HapticsToTriggers) {
                                    ImGui::Checkbox(strings.FullyRetractWhenNoData.c_str(), &s.FullyRetractTriggers);

                                    ImGui::Checkbox(strings.SwapTriggersRumbleToAT.c_str(), &s.SwapTriggersRumbleToAT);
                                    Tooltip(strings.Tooltip_SwapTriggersRumbleToAT.c_str());

                                    ImGui::Checkbox(strings.RumbleToAT_RigidMode.c_str(), &s.RumbleToAT_RigidMode);
                                    Tooltip(strings.Tooltip_RumbleToAT_RigidMode.c_str());

                                    if (!s.RumbleToAT_RigidMode) {
                                        ImGui::SliderInt(strings.MaxLeftIntensity.c_str(), &s.MaxLeftRumbleToATIntensity, 0, 255);
                                        Tooltip(strings.Tooltip_MaxIntensity.c_str());

                                        ImGui::SliderInt(strings.MaxLeftFrequency.c_str(), &s.MaxLeftRumbleToATFrequency, 0, 25);
                                        Tooltip(strings.Tooltip_MaxFrequency.c_str());

                                        ImGui::Separator();

                                        ImGui::SliderInt(strings.MaxRightIntensity.c_str(), &s.MaxRightRumbleToATIntensity, 0, 255);
                                        Tooltip(strings.Tooltip_MaxIntensity.c_str());

                                        ImGui::SliderInt(strings.MaxRightFrequency.c_str(), &s.MaxRightRumbleToATFrequency, 0, 25);
                                        Tooltip(strings.Tooltip_MaxFrequency.c_str());
                                    }
                                    else {
                                        ImGui::SliderInt(strings.MaxLeftIntensity.c_str(), &s.MaxLeftTriggerRigid, 0, 255);
                                        Tooltip(strings.Tooltip_MaxIntensity.c_str());

                                        ImGui::SliderInt(strings.MaxRightIntensity.c_str(), &s.MaxRightTriggerRigid, 0, 255);
                                        Tooltip(strings.Tooltip_MaxIntensity.c_str());
                                    }
                                }

                                ImGui::Separator();

                                if(!s.RumbleToAT && !s.HapticsToTriggers)
                                {
                                    ImGui::SetNextItemWidth(150);
                                    if (ImGui::BeginCombo(strings.TriggerFormat.c_str(), s.triggerFormat.c_str())) {
                                        if (ImGui::Selectable("Sony", false)) {
                                            s.triggerFormat = "Sony";
                                        }
                                        if (ImGui::Selectable("DSX", false)) {
                                            s.triggerFormat = "DSX";
                                        }

                                        ImGui::EndCombo();
                                    }

                                    ImGui::SetNextItemWidth(150);
                                    if (ImGui::BeginCombo(strings.SelectedTrigger.c_str(), s.curTrigger)) {
                                        if (ImGui::Selectable("L2", false)) {
                                            s.curTrigger = "L2";
                                        }
                                        if (ImGui::Selectable("R2", false)) {
                                            s.curTrigger = "R2";
                                        }

                                        ImGui::EndCombo();
                                    }

                                    ImGui::SetNextItemWidth(300);

                                    if(s.triggerFormat == "DSX")
                                    {
                                        if (s.curTrigger == "L2") {
                                            if (ImGui::BeginCombo(strings.LeftTriggerMode.c_str(),
                                                s.lmodestr.c_str())) {
                                                if (ImGui::Selectable("Off", false)) {
                                                    s.lmodestr = "Off";
                                                    s.ControllerInput.LeftTriggerMode =
                                                        Trigger::Off;
                                                }
                                                if (ImGui::Selectable("Rigid", false)) {
                                                    s.lmodestr = "Rigid";
                                                    s.ControllerInput.LeftTriggerMode =
                                                        Trigger::Rigid;
                                                }
                                                if (ImGui::Selectable("Pulse", false)) {
                                                    s.lmodestr = "Pulse";
                                                    s.ControllerInput.LeftTriggerMode =
                                                        Trigger::Pulse;
                                                }
                                                if (ImGui::Selectable("Rigid_A", false)) {
                                                    s.lmodestr = "Rigid_A";
                                                    s.ControllerInput.LeftTriggerMode =
                                                        Trigger::Rigid_A;
                                                }
                                                if (ImGui::Selectable("Rigid_B", false)) {
                                                    s.lmodestr = "Rigid_B";
                                                    s.ControllerInput.LeftTriggerMode =
                                                        Trigger::Rigid_B;
                                                }
                                                if (ImGui::Selectable("Rigid_AB", false)) {
                                                    s.lmodestr = "Rigid_AB";
                                                    s.ControllerInput.LeftTriggerMode =
                                                        Trigger::Rigid_AB;
                                                }
                                                if (ImGui::Selectable("Pulse_A", false)) {
                                                    s.lmodestr = "Pulse_A";
                                                    s.ControllerInput.LeftTriggerMode =
                                                        Trigger::Pulse_A;
                                                }
                                                if (ImGui::Selectable("Pulse_B", false)) {
                                                    s.lmodestr = "Pulse_B";
                                                    s.ControllerInput.LeftTriggerMode =
                                                        Trigger::Pulse_B;
                                                }
                                                if (ImGui::Selectable("Pulse_AB", false)) {
                                                    s.lmodestr = "Pulse_AB";
                                                    s.ControllerInput.LeftTriggerMode =
                                                        Trigger::Pulse_AB;
                                                }
                                                if (ImGui::Selectable("Calibration", false)) {
                                                    s.lmodestr = "Calibration";
                                                    s.ControllerInput.LeftTriggerMode =
                                                        Trigger::Calibration;
                                                }

                                                ImGui::EndCombo();
                                            }

                                            ImGui::SliderInt("LT 1",
                                                &s.L2DSXForces[0],
                                                0,
                                                255);
                                            ImGui::SliderInt("LT 2",
                                                &s.L2DSXForces[1],
                                                0,
                                                255);
                                            ImGui::SliderInt("LT 3",
                                                &s.L2DSXForces[2],
                                                0,
                                                255);
                                            ImGui::SliderInt("LT 4",
                                                &s.L2DSXForces[3],
                                                0,
                                                255);
                                            ImGui::SliderInt("LT 5",
                                                &s.L2DSXForces[4],
                                                0,
                                                255);
                                            ImGui::SliderInt("LT 6",
                                                &s.L2DSXForces[5],
                                                0,
                                                255);
                                            ImGui::SliderInt("LT 7",
                                                &s.L2DSXForces[6],
                                                0,
                                                255);
                                            ImGui::SliderInt("LT 8",
                                                &s.L2DSXForces[7],
                                                0,
                                                255);
                                            ImGui::SliderInt("LT 9",
                                                &s.L2DSXForces[8],
                                                0,
                                                255);
                                            ImGui::SliderInt("LT 10",
                                                &s.L2DSXForces[9],
                                                0,
                                                255);
                                            ImGui::SliderInt("LT 11",
                                                &s.L2DSXForces[10],
                                                0,
                                                255);
                                        }
                                        else {
                                            if (ImGui::BeginCombo(strings.RightTriggerMode.c_str(),
                                                s.rmodestr.c_str())) {
                                                if (ImGui::Selectable("Off", false)) {
                                                    s.rmodestr = "Off";
                                                    s.ControllerInput.RightTriggerMode =
                                                        Trigger::Off;
                                                }
                                                if (ImGui::Selectable("Rigid", false)) {
                                                    s.rmodestr = "Rigid";
                                                    s.ControllerInput.RightTriggerMode =
                                                        Trigger::Rigid;
                                                }
                                                if (ImGui::Selectable("Pulse", false)) {
                                                    s.rmodestr = "Pulse";
                                                    s.ControllerInput.RightTriggerMode =
                                                        Trigger::Pulse;
                                                }
                                                if (ImGui::Selectable("Rigid_A", false)) {
                                                    s.rmodestr = "Rigid_A";
                                                    s.ControllerInput.RightTriggerMode =
                                                        Trigger::Rigid_A;
                                                }
                                                if (ImGui::Selectable("Rigid_B", false)) {
                                                    s.rmodestr = "Rigid_B";
                                                    s.ControllerInput.RightTriggerMode =
                                                        Trigger::Rigid_B;
                                                }
                                                if (ImGui::Selectable("Rigid_AB", false)) {
                                                    s.rmodestr = "Rigid_AB";
                                                    s.ControllerInput.RightTriggerMode =
                                                        Trigger::Rigid_AB;
                                                }
                                                if (ImGui::Selectable("Pulse_A", false)) {
                                                    s.rmodestr = "Pulse_A";
                                                    s.ControllerInput.RightTriggerMode =
                                                        Trigger::Pulse_A;
                                                }
                                                if (ImGui::Selectable("Pulse_B", false)) {
                                                    s.rmodestr = "Pulse_B";
                                                    s.ControllerInput.RightTriggerMode =
                                                        Trigger::Pulse_B;
                                                }
                                                if (ImGui::Selectable("Pulse_AB", false)) {
                                                    s.rmodestr = "Pulse_AB";
                                                    s.ControllerInput.RightTriggerMode =
                                                        Trigger::Pulse_AB;
                                                }
                                                if (ImGui::Selectable("Calibration", false)) {
                                                    s.rmodestr = "Calibration";
                                                    s.ControllerInput.RightTriggerMode =
                                                        Trigger::Calibration;
                                                }
                                                ImGui::EndCombo();
                                            }

                                            ImGui::SliderInt("RT 1",
                                                &s.R2DSXForces[0],
                                                0,
                                                255);
                                            ImGui::SliderInt("RT 2",
                                                &s.R2DSXForces[1],
                                                0,
                                                255);
                                            ImGui::SliderInt("RT 3",
                                                &s.R2DSXForces[2],
                                                0,
                                                255);
                                            ImGui::SliderInt("RT 4",
                                                &s.R2DSXForces[3],
                                                0,
                                                255);
                                            ImGui::SliderInt("RT 5",
                                                &s.R2DSXForces[4],
                                                0,
                                                255);
                                            ImGui::SliderInt("RT 6",
                                                &s.R2DSXForces[5],
                                                0,
                                                255);
                                            ImGui::SliderInt("RT 7",
                                                &s.R2DSXForces[6],
                                                0,
                                                255);
                                            ImGui::SliderInt("RT 8",
                                                &s.R2DSXForces[7],
                                                0,
                                                255);
                                            ImGui::SliderInt("RT 9",
                                                &s.R2DSXForces[8],
                                                0,
                                                255);
                                            ImGui::SliderInt("RT 10",
                                                &s.R2DSXForces[9],
                                                0,
                                                255);
                                            ImGui::SliderInt("RT 11",
                                                &s.R2DSXForces[10],
                                                0,
                                                255);

                                        }
                                    }
                                    else if (s.triggerFormat == "Sony") {
                                        if (s.curTrigger == "L2") {
                                            if (ImGui::BeginCombo(strings.LeftTriggerMode.c_str(),
                                                s.lmodestrSony.c_str())) {
                                                if (ImGui::Selectable("Off", false)) {
                                                    s.lmodestrSony = "Off";
                                                }
                                                if (ImGui::Selectable("Feedback", false)) {
                                                    s.lmodestrSony = "Feedback";
                                                }
                                                if (ImGui::Selectable("Weapon", false)) {
                                                    s.lmodestrSony = "Weapon";
                                                }
                                                if (ImGui::Selectable("Vibration", false)) {
                                                    s.lmodestrSony = "Vibration";
                                                }
                                                if (ImGui::Selectable("Slope Feedback", false)) {
                                                    s.lmodestrSony = "Slope Feedback";
                                                }
                                                if (ImGui::Selectable("Multiple Position Feedback", false)) {
                                                    s.lmodestrSony = "Multiple Position Feedback";
                                                }
                                                if (ImGui::Selectable("Multiple Position Vibration", false)) {
                                                    s.lmodestrSony = "Multiple Position Vibration";
                                                }

                                                ImGui::EndCombo();
                                            }

                                            if (s.lmodestrSony == "Feedback") {
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.Position.c_str(), &s.L2Feedback[0], 0, 9);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.Strength.c_str(), &s.L2Feedback[1], 0, 8);
                                            }
                                            else if (s.lmodestrSony == "Weapon") {
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.StartPosition.c_str(), &s.L2WeaponFeedback[0], 2, 7);

                                                if (s.L2WeaponFeedback[1] < s.L2WeaponFeedback[0])
                                                    s.L2WeaponFeedback[1] = s.L2WeaponFeedback[0];

                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.EndPosition.c_str(), &s.L2WeaponFeedback[1], s.L2WeaponFeedback[0], 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.Strength.c_str(), &s.L2WeaponFeedback[2], 0, 8);
                                            }
                                            else if (s.lmodestrSony == "Vibration") {
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.Position.c_str(), &s.L2VibrationFeedback[0], 0, 9);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.Amplitude.c_str(), &s.L2VibrationFeedback[1], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.Frequency.c_str(), &s.L2VibrationFeedback[2], 0, 255);
                                            }
                                            else if (s.lmodestrSony == "Slope Feedback") {
                                                if (s.L2SlopeFeedback[1] < s.L2SlopeFeedback[0])
                                                    s.L2SlopeFeedback[0] = s.L2SlopeFeedback[1];

                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.StartPosition.c_str(), &s.L2SlopeFeedback[0], 0, s.L2SlopeFeedback[1]);

                                                if (s.L2SlopeFeedback[1] < s.L2SlopeFeedback[0])
                                                    s.L2SlopeFeedback[1] = s.L2SlopeFeedback[0];

                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.EndPosition.c_str(), &s.L2SlopeFeedback[1], s.L2SlopeFeedback[0], 9);

                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.StartStrength.c_str(), &s.L2SlopeFeedback[2], 1, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.EndStrength.c_str(), &s.L2SlopeFeedback[3], 1, 8);
                                            }
                                            else if (s.lmodestrSony == "Multiple Position Feedback") {

                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(1)).c_str(), &s.L2MultiplePositionFeedback[0], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(2)).c_str(), &s.L2MultiplePositionFeedback[1], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(3)).c_str(), &s.L2MultiplePositionFeedback[2], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(4)).c_str(), &s.L2MultiplePositionFeedback[3], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(5)).c_str(), &s.L2MultiplePositionFeedback[4], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(6)).c_str(), &s.L2MultiplePositionFeedback[5], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(7)).c_str(), &s.L2MultiplePositionFeedback[6], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(8)).c_str(), &s.L2MultiplePositionFeedback[7], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(9)).c_str(), &s.L2MultiplePositionFeedback[8], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(10)).c_str(), &s.L2MultiplePositionFeedback[9], 0, 8);
                                            }
                                            else if (s.lmodestrSony == "Multiple Position Vibration") {

                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt("Frequency", &s.L2MultipleVibrationFeedback[0], 0, 255);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Amplitude + " " + std::to_string(1)).c_str(), &s.L2MultipleVibrationFeedback[1], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(2)).c_str(), &s.L2MultipleVibrationFeedback[2], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(3)).c_str(), &s.L2MultipleVibrationFeedback[3], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(4)).c_str(), &s.L2MultipleVibrationFeedback[4], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(5)).c_str(), &s.L2MultipleVibrationFeedback[5], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(6)).c_str(), &s.L2MultipleVibrationFeedback[6], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(7)).c_str(), &s.L2MultipleVibrationFeedback[7], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(8)).c_str(), &s.L2MultipleVibrationFeedback[8], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(9)).c_str(), &s.L2MultipleVibrationFeedback[9], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(10)).c_str(), &s.L2MultipleVibrationFeedback[10], 0, 8);
                                            }
                                        }
                                        else if (s.curTrigger == "R2") {
                                            if (ImGui::BeginCombo(strings.RightTriggerMode.c_str(),
                                                s.rmodestrSony.c_str())) {
                                                if (ImGui::Selectable("Off", false)) {
                                                    s.rmodestrSony = "Off";
                                                }
                                                if (ImGui::Selectable("Feedback", false)) {
                                                    s.rmodestrSony = "Feedback";
                                                }
                                                if (ImGui::Selectable("Weapon", false)) {
                                                    s.rmodestrSony = "Weapon";
                                                }
                                                if (ImGui::Selectable("Vibration", false)) {
                                                    s.rmodestrSony = "Vibration";
                                                }
                                                if (ImGui::Selectable("Slope Feedback", false)) {
                                                    s.rmodestrSony = "Slope Feedback";
                                                }
                                                if (ImGui::Selectable("Multiple Position Feedback", false)) {
                                                    s.rmodestrSony = "Multiple Position Feedback";
                                                }
                                                if (ImGui::Selectable("Multiple Position Vibration", false)) {
                                                    s.rmodestrSony = "Multiple Position Vibration";
                                                }

                                                ImGui::EndCombo();
                                            }

                                            if (s.rmodestrSony == "Feedback") {
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.Position.c_str(), &s.R2Feedback[0], 0, 9);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.Strength.c_str(), &s.R2Feedback[1], 0, 8);
                                            }
                                            else if (s.rmodestrSony == "Weapon") {
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.StartPosition.c_str(), &s.R2WeaponFeedback[0], 2, 7);

                                                if (s.R2WeaponFeedback[1] < s.R2WeaponFeedback[0])
                                                    s.R2WeaponFeedback[1] = s.R2WeaponFeedback[0];

                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.EndPosition.c_str(), &s.R2WeaponFeedback[1], s.R2WeaponFeedback[0], 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.Strength.c_str(), &s.R2WeaponFeedback[2], 0, 8);
                                            }
                                            else if (s.rmodestrSony == "Vibration") {
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.Position.c_str(), &s.R2VibrationFeedback[0], 0, 9);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.Amplitude.c_str(), &s.R2VibrationFeedback[1], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.Frequency.c_str(), &s.R2VibrationFeedback[2], 0, 255);
                                            }
                                            else if (s.rmodestrSony == "Slope Feedback") {
                                                if (s.R2SlopeFeedback[1] < s.R2SlopeFeedback[0])
                                                    s.R2SlopeFeedback[0] = s.R2SlopeFeedback[1];

                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.StartPosition.c_str(), &s.R2SlopeFeedback[0], 0, s.R2SlopeFeedback[1]);

                                                if (s.R2SlopeFeedback[1] < s.R2SlopeFeedback[0])
                                                    s.R2SlopeFeedback[1] = s.R2SlopeFeedback[0];

                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.EndPosition.c_str(), &s.R2SlopeFeedback[1], s.R2SlopeFeedback[0], 9);

                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.StartStrength.c_str(), &s.R2SlopeFeedback[2], 1, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(strings.EndStrength.c_str(), &s.R2SlopeFeedback[3], 1, 8);
                                            }
                                            else if (s.rmodestrSony == "Multiple Position Feedback") {

                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(1)).c_str(), &s.R2MultiplePositionFeedback[0], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(2)).c_str(), &s.R2MultiplePositionFeedback[1], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(3)).c_str(), &s.R2MultiplePositionFeedback[2], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(4)).c_str(), &s.R2MultiplePositionFeedback[3], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(5)).c_str(), &s.R2MultiplePositionFeedback[4], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(6)).c_str(), &s.R2MultiplePositionFeedback[5], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(7)).c_str(), &s.R2MultiplePositionFeedback[6], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(8)).c_str(), &s.R2MultiplePositionFeedback[7], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(9)).c_str(), &s.R2MultiplePositionFeedback[8], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(10)).c_str(), &s.R2MultiplePositionFeedback[9], 0, 8);
                                            }
                                            else if (s.rmodestrSony == "Multiple Position Vibration") {

                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt("Frequency", &s.R2MultipleVibrationFeedback[0], 0, 255);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Amplitude + " " + std::to_string(1)).c_str(), &s.R2MultipleVibrationFeedback[1], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(2)).c_str(), &s.R2MultipleVibrationFeedback[2], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(3)).c_str(), &s.R2MultipleVibrationFeedback[3], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(4)).c_str(), &s.R2MultipleVibrationFeedback[4], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(5)).c_str(), &s.R2MultipleVibrationFeedback[5], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(6)).c_str(), &s.R2MultipleVibrationFeedback[6], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(7)).c_str(), &s.R2MultipleVibrationFeedback[7], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(8)).c_str(), &s.R2MultipleVibrationFeedback[8], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(9)).c_str(), &s.R2MultipleVibrationFeedback[9], 0, 8);
                                                ImGui::SetNextItemWidth(300);
                                                ImGui::SliderInt(std::string(strings.Strength + " " + std::to_string(10)).c_str(), &s.R2MultipleVibrationFeedback[10], 0, 8);
                                            }
                                        }
                                    }

                                    ImGui::Separator();
                                    ImGui::Spacing();
                                }
                            }
                            }
                            else {
                                ImGui::Text(strings.LEDandATunavailableUDP.c_str());
                            }
                            
                            if (ImGui::CollapsingHeader(strings.Motion.c_str())) {
                                ImGui::BeginDisabled();
                                ImGui::SetNextItemWidth(400);
                                ImGui::SliderInt(std::string(strings.Gyroscope + " X").c_str(), &DualSense[i].State.gyro.X, -9000, 9000);
                                ImGui::SetNextItemWidth(400);
                                ImGui::SliderInt(std::string(strings.Gyroscope + " Y").c_str(), &DualSense[i].State.gyro.Y, -9000, 9000);
                                ImGui::SetNextItemWidth(400);
                                ImGui::SliderInt(std::string(strings.Gyroscope + " Z").c_str(), &DualSense[i].State.gyro.Z, -9000, 9000);

                                ImGui::SetNextItemWidth(400);
                                ImGui::SliderInt(std::string(strings.Accelerometer + " X").c_str(), &DualSense[i].State.accelerometer.X, -9000, 9000);
                                ImGui::SetNextItemWidth(400);
                                ImGui::SliderInt(std::string(strings.Accelerometer + " Y").c_str(), &DualSense[i].State.accelerometer.Y, -9000, 9000);
                                ImGui::SetNextItemWidth(400);
                                ImGui::SliderInt(std::string(strings.Accelerometer + " Z").c_str(), &DualSense[i].State.accelerometer.Z, -9000, 9000);
                                ImGui::EndDisabled();

                                
                                ImGui::Separator();
                                ImGui::Checkbox(strings.GyroToMouse.c_str(), &s.GyroToMouse);
                                ImGui::SameLine();
                                Tooltip(strings.Tooltip_GyroToMouse.c_str());
                                ImGui::SetNextItemWidth(250);                             
                                ImGui::SliderFloat(std::string(strings.Sensitivity + "##gyrotomouse").c_str(), &s.GyroToMouseSensitivity, 0.001f, 0.030f);
                                ImGui::Checkbox(strings.R2ToMouseClick.c_str(), &s.R2ToMouseClick);

                                ImGui::Separator();
                                if(s.emuStatus == None)
                                    ImGui::Text(strings.ControllerEmulationRequired.c_str());
                                ImGui::Checkbox(strings.GyroToRightAnalogStick.c_str(), &s.GyroToRightAnalogStick);
                                Tooltip(strings.Tooltip_GyroToRightAnalogStick.c_str());
                                ImGui::SetNextItemWidth(250);  
                                ImGui::SliderFloat(std::string(strings.Sensitivity + "##gyrotorightanalogstick").c_str(), &s.GyroToRightAnalogStickSensitivity, 0.1f, 5.0f);
                                ImGui::SetNextItemWidth(250);
                                ImGui::SliderFloat(strings.GyroDeadzone.c_str(), &s.GyroToRightAnalogStickDeadzone, 0.001, 0.300);
                                ImGui::SetNextItemWidth(250);
                                ImGui::SliderInt(strings.MinimumStickValue.c_str(), &s.GyroToRightAnalogStickMinimumStickValue, 1, 127);

                                ImGui::Separator();
                                if (s.GyroToMouse || s.GyroToRightAnalogStick) {
                                    ImGui::SetNextItemWidth(250);
                                    ImGui::SliderInt(strings.L2Threshold.c_str(), &s.L2Threshold, 1, 255);
                                }
                            }

                            if (HF_status == DualsenseUtils::HapticFeedbackStatus::Working && s.RunAudioToHapticsOnStart && !s.WasAudioToHapticsRan && !s.WasAudioToHapticsCheckboxChecked) {
                                    MyUtils::StartAudioToHaptics(DualSense[i].GetAudioDeviceName());
                                    s.WasAudioToHapticsRan = true;
                            }

                            if (ImGui::CollapsingHeader(strings.HapticFeedback.c_str()))
                            {
                                ImGui::Text(strings.StandardRumble.c_str());
                                Tooltip(strings.Tooltip_HapticFeedback.c_str());
                                ImGui::SetNextItemWidth(300);
                                ImGui::SliderInt(strings.LeftMotor.c_str(), &s.lrumble, 0, 255);
                                ImGui::SetNextItemWidth(300);
                                ImGui::SliderInt(strings.RightMotor.c_str(), &s.rrumble, 0, 255);

                                ImGui::SetNextItemWidth(300);
                                ImGui::SliderInt(strings.MaxLeftMotor.c_str(), &s.MaxLeftMotor, 0, 255);
                                ImGui::SetNextItemWidth(300);
                                ImGui::SliderInt(strings.MaxRightMotor.c_str(), &s.MaxRightMotor, 0, 255);
                                ImGui::SameLine();
                                Tooltip(strings.Tooltip_MaxMotor.c_str());
                                ImGui::Separator();

                                if (ImGui::Checkbox(strings.RunAudioToHapticsOnStart.c_str(), &s.RunAudioToHapticsOnStart)) {
                                    s.WasAudioToHapticsCheckboxChecked = true;
                                }

                                if (HF_status == DualsenseUtils::HapticFeedbackStatus::Working)
                                {
                                    ImGui::Checkbox(strings.TouchpadToHaptics.c_str(), &s.TouchpadToHaptics);
                                    DualSense[i].TouchpadToHaptics = s.TouchpadToHaptics;
                                    ImGui::SameLine();
                                    ImGui::SetNextItemWidth(100);
                                    ImGui::SliderFloat(strings.Frequency.c_str(), &s.TouchpadToHapticsFrequency, 1.0f, 440.0f);
                                    DualSense[i].TouchpadToHapticsFrequency = s.TouchpadToHapticsFrequency;

                                    if (ImGui::Button(strings.StartAudioToHaptics.c_str()))
                                    {
                                        MyUtils::StartAudioToHaptics(DualSense[i].GetAudioDeviceName());
                                    };
                                    Tooltip(strings.Tooltip_StartAudioToHaptics.c_str());

                                    ImGui::SetNextItemWidth(300);
                                    ImGui::SliderInt(strings.SpeakerVolume.c_str(), &s.ControllerInput.SpeakerVolume, 0, 0x7C, "%d");
                                    Tooltip(strings.Tooltip_SpeakerVolume.c_str());
                                    ImGui::SetNextItemWidth(300);
                                    ImGui::SliderInt(strings.HeadsetVolume.c_str(), &s.ControllerInput.HeadsetVolume, 0, 0x7C, "%d");

                                    ImGui::Checkbox(strings.UseHeadset.c_str(), &s.UseHeadset);

                                    ImGui::Separator();
                                    ImGui::Text(MyUtils::WStringToString(DualSense[i].GetAudioDeviceInstanceID()).c_str());
                                    ImGui::Text(DualSense[i].GetAudioDeviceName());
                                }
                                else
                                {
                                    if (HF_status == DualsenseUtils::HapticFeedbackStatus::BluetoothNotUSB) {
                                        ImGui::Text(strings.HapticsUnavailable.c_str());
                                    }
                                    else if (HF_status == DualsenseUtils::HapticFeedbackStatus::AudioDeviceNotFound) {
                                        ImGui::Text(strings.HapticsUnavailableNoAudioDevice.c_str());
                                    }
                                    else if (HF_status == DualsenseUtils::HapticFeedbackStatus::AudioEngineNotInitialized) {
                                        ImGui::Text(strings.AudioEngineNotInitializedError.c_str());
                                    }
                                }
                              
                            }

                            if (ImGui::CollapsingHeader(strings._Touchpad.c_str()))
                            {
                                    // Define the touchpad's preview size in the UI
                                    ImVec2 touchpadSize(300 * io.FontGlobalScale, 140* io.FontGlobalScale);  // Adjusted to keep a similar aspect ratio (1920:900)

                                    // Draw touchpad background
                                    ImGui::InvisibleButton("##touchpad_bg", touchpadSize);
                                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                                    ImVec2 touchpadPos = ImGui::GetItemRectMin();
                                    int rcolor = DualSense[i].State.touchBtn ? 100 : 50;
                                    drawList->AddRectFilled(touchpadPos, ImVec2(touchpadPos.x + touchpadSize.x, touchpadPos.y + touchpadSize.y), IM_COL32(rcolor, 50, 50, 255));

                                    // Scale touch points to fit within the UI preview
                                    if (DualSense[i].State.trackPadTouch0.IsActive)
                                    {
                                            float scaledX = (DualSense[i].State.trackPadTouch0.X / 1900.0f) * touchpadSize.x;
                                            float scaledY = (DualSense[i].State.trackPadTouch0.Y / 1100.0f) * touchpadSize.y;
                                            ImVec2 touchPos(touchpadPos.x + scaledX, touchpadPos.y + scaledY);
                                            drawList->AddCircleFilled(touchPos, 6.0f * io.FontGlobalScale, IM_COL32(255, 0, 0, 255));                                    
                                    }

                                    if (DualSense[i].State.trackPadTouch1.IsActive)
                                    {
                                            float scaledX = (DualSense[i].State.trackPadTouch1.X / 1900.0f) * touchpadSize.x;
                                            float scaledY = (DualSense[i].State.trackPadTouch1.Y / 1100.0f) * touchpadSize.y;
                                            ImVec2 touchPos(touchpadPos.x + scaledX, touchpadPos.y + scaledY);
                                            drawList->AddCircleFilled(touchPos, 6.0f * io.FontGlobalScale, IM_COL32(255, 0, 0, 255));  // Red circle for active touch
                                    }
                                    ImGui::SameLine();
                                    ImGui::Text("%s:\n%s: %d\n\n%s 1:\nX: %d\nY: %d\n\n%s 2:\nX: %d\nY: %d",
                                        strings.GeneralData.c_str(),
                                        strings.TouchPacketNum.c_str(),
                                        DualSense[i].State.TouchPacketNum,
                                        strings.Touch.c_str(),                                       
                                        DualSense[i].State.trackPadTouch0.X,
                                        DualSense[i].State.trackPadTouch0.Y,
                                        strings.Touch.c_str(), 
                                        DualSense[i].State.trackPadTouch1.X,
                                        DualSense[i].State.trackPadTouch1.Y);

                                    ImGui::Checkbox(strings.TouchpadToMouse.c_str(), &s.TouchpadToMouse);
                                    Tooltip(strings.Tooltip_TouchpadToMouse.c_str());
                                    ImGui::SetNextItemWidth(300);
                                    ImGui::SliderFloat(strings.Sensitivity.c_str(), &s.swipeThreshold, 0.01f, 3.0f);
                            }

                            if (ImGui::CollapsingHeader(strings.MicButton.c_str())) {
                                ImGui::Checkbox(strings.TakeScreenshot.c_str(), &s.MicScreenshot);
                                Tooltip(strings.Tooltip_TakeScreenshot.c_str());
                                if (HF_status == DualsenseUtils::HapticFeedbackStatus::Working) {
                                    ImGui::SameLine();
                                    ImGui::SetNextItemWidth(100);
                                    ImGui::SliderFloat(strings.ScreenshotSoundVolume.c_str(), &s.ScreenshotSoundLoudness, 0, 1);
                                }
                                
                                ImGui::Checkbox(strings.RealMicFunctionality.c_str(), &s.MicFunc);
                                Tooltip(strings.Tooltip_RealMicFunctionality.c_str());

                                if (!s.MicFunc) {
                                    DualSense[i].SetMicrophoneLED(false, false);
                                    DualSense[i].SetMicrophoneVolume(80);
                                }

                                ImGui::Checkbox(strings.TouchpadShortcut.c_str(), &s.TouchpadToMouseShortcut);
                                ImGui::Checkbox(strings.SwapTriggersShortcut.c_str(), &s.SwapTriggersShortcut);
                                ImGui::Checkbox(strings.X360Shortcut.c_str(), &s.X360Shortcut);
                                ImGui::Checkbox(strings.DS4Shortcut.c_str(), &s.DS4Shortcut);
                                ImGui::Checkbox(strings.StopEmuShortcut.c_str(), &s.StopEmuShortcut);
                                ImGui::Checkbox(strings.StopWritingShortcut.c_str(), &s.StopWritingShortcut);
                                ImGui::Checkbox(strings.GyroToMouseShortcut.c_str(), &s.GyroToMouseShortcut);
                                ImGui::Checkbox(strings.GyroToRightAnalogStickShortcut.c_str(), &s.GyroToRightAnalogStickShortcut);
                            }

                            if (ImGui::CollapsingHeader(strings.EmulationHeader.c_str()))
                            {
                                    if (s.emuStatus == None) {
                                        if (ImGui::Button(strings.X360emu.c_str())) {
                                            s.emuStatus = X360;
                                            MyUtils::RunAsyncHidHideRequest(DualSense[i].GetPath(), "hide");
                                        }
                                        ImGui::SameLine();
                                        if (ImGui::Button(strings.DS4emu.c_str())) {
                                            s.emuStatus = DS4;
                                            DualSense[i].SetLightbar(0, 0, 64);
                                            MyUtils::RunAsyncHidHideRequest(DualSense[i].GetPath(), "hide");
                                        }
                                        ImGui::SameLine();
                                        ImGui::Checkbox(strings.DualShock4V2emu.c_str(), &s.DualShock4V2);
                                        Tooltip(strings.Tooltip_Dualshock4V2.c_str());
                                    }
                                    else {
                                        if (ImGui::Button(strings.STOPemu.c_str())) {
                                            MyUtils:: RunAsyncHidHideRequest(DualSense[i].GetPath(), "show");
                                            s.emuStatus = None;
                                        }
                                    }

                                    if (!WasElevated) {
                                        ImGui::Text(strings.ControllerEmuUserMode.c_str());
                                    }
                                    else {
                                        ImGui::Text(strings.ControllerEmuAppAsAdmin.c_str());
                                    }

                                    ImGui::Separator();

                                    ImGui::SetNextItemWidth(300);
                                    ImGui::SliderInt(strings.LeftAnalogStickDeadZone.c_str(), &s.LeftAnalogDeadzone, 0, 127);
                                    ImGui::SetNextItemWidth(300);
                                    ImGui::SliderInt(strings.RightAnalogStickDeadZone.c_str(), &s.RightAnalogDeadzone, 0, 127);
                                    ImGui::SetNextItemWidth(300);
                                    ImGui::SliderInt(strings.L2Deadzone.c_str(), &s.L2Deadzone, 1, 255);
                                    ImGui::SetNextItemWidth(300);
                                    ImGui::SliderInt(strings.R2Deadzone.c_str(), &s.R2Deadzone, 1, 255);
                                    ImGui::Checkbox(strings.TriggersAsButtons.c_str(), &s.TriggersAsButtons);
                                    Tooltip(strings.Tooltip_TriggersAsButtons.c_str());
                                    ImGui::Checkbox(strings.TouchpadAsSelect.c_str(), &s.TouchpadAsSelect);
                                    ImGui::Checkbox(strings.TouchpadAsStart.c_str(), &s.TouchpadAsStart);
                                    ImGui::Checkbox(strings.TouchpadAsRightStick.c_str(), &s.TouchpadToRXRY);
                                    ImGui::Checkbox(strings.OverrideDS4Lightbar.c_str(), &s.OverrideDS4Lightbar);
                            }

                            ShowNonControllerConfig = false;
                        }
                    }
                }
                else {
                    ShowNonControllerConfig = true;
                }
            }

            if(ShowNonControllerConfig)
            {
                ImGui::BeginMainMenuBar();
                if (ImGui::BeginMenu("Config")) {
                    ImGui::SetNextItemWidth(120.0f);
                    if (ImGui::Combo(strings.Language.c_str(), &selectedLanguageIndex, languageItems.data(), languageItems.size())) {
                        const std::string& selectedLanguage = languages[selectedLanguageIndex];
                        appConfig.Language = selectedLanguage;
                        Config::WriteAppConfigToFile(appConfig);
                    }

                    if (ImGui::Checkbox(strings.RunAsAdmin.c_str(), &appConfig.ElevateOnStartup)) {
                        Config::WriteAppConfigToFile(appConfig);

                        if (!MyUtils::IsRunAsAdministrator()) {
                            MyUtils::ElevateNow();
                        }
                    }
                    Tooltip(strings.Tooltip_RunAsAdmin.c_str());

                    if (ImGui::Checkbox(strings.HideToTray.c_str(), &appConfig.HideToTray)) {
                        Config::WriteAppConfigToFile(appConfig);
                    }
                    Tooltip(strings.Tooltip_HideToTray.c_str());

                    if (ImGui::Checkbox(strings.HideWindowOnStartup.c_str(), &appConfig.HideWindowOnStartup)) {
                        Config::WriteAppConfigToFile(appConfig);
                    }
                    Tooltip(strings.Tooltip_HideWindowOnStartup.c_str());

                    if (ImGui::Checkbox(strings.RunWithWindows.c_str(), &appConfig.RunWithWindows)) {
                        if (appConfig.RunWithWindows) {
                            MyUtils::AddToStartup();
                        }
                        else {
                            MyUtils::RemoveFromStartup();
                        }

                        Config::WriteAppConfigToFile(appConfig);
                    }
                    Tooltip(strings.RunWithWindows.c_str());

                    if (ImGui::Checkbox(strings.ShowConsole.c_str(), &appConfig.ShowConsole)) {
                        Config::WriteAppConfigToFile(appConfig);
                    }

                    ImGui::EndMenu();
                }

               if (ImGui::BeginMenu(strings.Style.c_str())) {
                   if (ImGui::Button(strings.SaveStyleToFile.c_str())) {
                                        MyUtils::SaveImGuiColorsToFile(style);
                                    }

                                    if (ImGui::Button(strings.LoadStyleFromFile.c_str())) {
                                        MyUtils::LoadImGuiColorsFromFile(style);
                                    }

                                    if (ImGui::Button(strings.SetDefaultStyleFile.c_str())) {
                                        std::string stylePath = Config::GetStylePath();
                                        if (stylePath != "") {
                                            appConfig.DefaultStyleFilepath = stylePath;
                                            Config::WriteAppConfigToFile(appConfig);
                                        }
                                    }

                                    if (ImGui::Button(strings.RemoveDefaultStyle.c_str())) {
                                        appConfig.DefaultStyleFilepath = "";
                                        Config::WriteAppConfigToFile(appConfig);
                                    }

                                    if (ImGui::Button(strings.ResetStyle.c_str())) {
                                        style = ImGuiStyle();
                                    }

                    if (ImGui::Button("Show edit menu")) {
                        ShowStyleEditWindow = true;
                    }
                    ImGui::EndMenu();
               }
               MainMenuBarHeight = ImGui::GetFrameHeight();
                ImGui::EndMenuBar();
            }          
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // 60 FPS

        ImGui::End();
        ImGui::Render();

        end_cycle(window);       
    }

    stop_thread = true; // Signal all threads to stop

    std::this_thread::sleep_for(std::chrono::milliseconds(15));

    for (auto &thread : readThreads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    for (auto &thread : writeThreads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    for (auto &thread : emuThreads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    if(WasElevated)
    {
        for (Dualsense& ds : DualSense) {
            MyUtils::StartHidHideRequest(ds.GetPath(), "show");
        }
    }

    udpServer.Dispose();
    if (trayThread.joinable())
        trayThread.join();
    tray.exit();
    tray.~Tray();
   
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    hid_exit();

    MyUtils::DeinitializeAudioEndpoint();
    //_CrtDumpMemoryLeaks();

    timeEndPeriod(1);
    return 0;
}

void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}
