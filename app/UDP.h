#include "Settings.h"
#include "MyUtils.h"
#include <ctime>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <ctime>
#include <ratio>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <direct.h>
#pragma comment(lib, "Ws2_32.lib")

enum ConnectionType {
    USB,
    BLUETOOTH,
    DONGLE
};

enum DeviceType {
    DUALSENSE,
    DUALSENSE_EDGE,
    DUALSHOCK_V1,
    DUALSHOCK_V2,
    DUALSHOCK_DONGLE,
    PS_VR2_LeftController,
    PS_VR2_RightController,
    ACCESS_CONTROLLER
};

struct Device {
    int Index;
    std::string MacAddress;
    DeviceType DeviceType;
    ConnectionType ConnectionType;
    int BatteryLevel;
    bool IsSupportAT;
    bool IsSupportLightBar;
    bool IsSupportPlayerLED;
    bool IsSupportMicLED;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["Index"] = Index;
        j["MacAddress"] = MacAddress;
        j["DeviceType"] = DeviceType; // Convert enum to string
        j["ConnectionType"] = ConnectionType; // Convert enum to string
        j["BatteryLevel"] = BatteryLevel;
        j["IsSupportAT"] = IsSupportAT;
        j["IsSupportLightBar"] = IsSupportLightBar;
        j["IsSupportPlayerLED"] = IsSupportPlayerLED;
        j["IsSupportMicLED"] = IsSupportMicLED;
        return j;
    }
};

struct UDPResponse
{
    std::string Status;
    std::string TimeReceived;
    bool isControllerConnected;
    int BatteryLevel;
    std::vector<Device> Devices;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["Status"] = Status;
        j["TimeReceived"] = TimeReceived;
        j["isControllerConnected"] = isControllerConnected;
        j["BatteryLevel"] = BatteryLevel;
        for (const auto& device : Devices) {
            j["Devices"].push_back(device.to_json());
        }
        return j;
    }
};

class UDP
{
public:
    bool isActive = false;
    int Battery;
    Settings thisSettings;    
    std::chrono::high_resolution_clock::time_point LastTime = std::chrono::high_resolution_clock::now();

    UDP(int port = 6969) : serverOn(false), Battery(100), unavailable(false) {
        // Create directory with error checking
        std::error_code ec;
        if (!filesystem::create_directories("C:/Temp/DualSenseX", ec) && ec) {
            std::cerr << "Directory creation failed: " << ec.message() << std::endl;
        }

        std::ofstream portFile("C:/Temp/DualSenseX/DualSenseX_PortNumber.txt", std::ios::trunc);
        if (!portFile.is_open()) {
            std::cerr << "Failed to open port file" << std::endl;
        }
        else {
            portFile << port;
        }
        
        if (!portFile.good()) {
            std::cerr << "Failed to write to port file" << std::endl;
        }

        Connect();
    }

    ~UDP()
    {
        Dispose();
    }

    void Connect()
    {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0)
        {
            std::cerr << "WSAStartup failed: " << result << "\n";
            return;
        }

        clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (clientSocket == INVALID_SOCKET)
        {
            std::cerr << "Socket creation failed.\n";
            WSACleanup();
            return;
        }


        int portNumber = 6969;

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(portNumber);
        serverAddr.sin_addr.s_addr = INADDR_ANY;

        if (::bind(clientSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            std::cerr << "Bind failed. " << err << endl;
            closesocket(clientSocket);
            WSACleanup();
            return;
        }
        else {
            cout << "Server started at: " << portNumber << endl;
        }

        serverOn = true;
        std::thread(&UDP::Listen, this).detach();
    }

    int getParameterValue(const nlohmann::json& param) {
        if (param.is_null()) {
            return 0; // Default value for null
        }
        else if (param.is_number()) {
            return param.get<int>(); // Convert number to int
        }
        else {
            return 0;
        }
    }

    bool getBoolValue(const nlohmann::json& param) {
        if (param.is_null()) {
            return false;
        }
        else if (param.is_boolean()) {
            return param.get<bool>();
        }
        else {
            return false;
        }
    }

    float getFloatValue(const nlohmann::json& param) {
        if (param.is_null()) {
            return 0.0f;
        }
        else if (param.is_number_float() || param.is_number()) {
            return param.get<float>();
        }
        else if (param.is_string()) {
            try {
                // Convert the string to float
                return std::stof(param.get<std::string>());
            } catch (const std::invalid_argument& e) {
                return 0.0f;
            } catch (const std::out_of_range& e) {
                return 0.0f;
            }
        }
        else {
            return 0.0f;
        }
    }

    std::string getParameterValueAsString(const nlohmann::json& param) {
        if (param.is_null()) {
            return ""; // Default value for null
        }
        else if (param.is_string()) {
            return param.get<std::string>(); // Return the string directly
        }
        else if (param.is_boolean()) {
            return param.get<bool>() ? "true" : "false"; // Convert boolean to "true" or "false"
        }
        else if (param.is_number()) {
            return std::to_string(param.get<double>()); // Convert number to string
        }
        else {
            return ""; // Default value for unsupported types
        }
    }

    void Listen()
    {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
        sockaddr_in clientAddr{};
        int clientAddrSize = sizeof(clientAddr);
        char recvBuffer[1024];

        while (serverOn)
        {                 
            int bytesReceived = recvfrom(clientSocket,
                                         recvBuffer,
                                         sizeof(recvBuffer),
                                         0,
                                         (SOCKADDR *)&clientAddr,
                                         &clientAddrSize);

            if (bytesReceived == SOCKET_ERROR)
            {
                std::cerr << "recvfrom failed.\n";
                continue;
            }

            LastTime = std::chrono::high_resolution_clock::now();

            std::string packetString(recvBuffer, bytesReceived);
            
            newPacket = true;

            Device dev{
                1,                          // Index
                "FU:CK:YO:UU:LO:OL",       // MacAddress
                DeviceType::DUALSENSE,     // DeviceType
                ConnectionType::USB,  // ConnectionType
                Battery,                         // BatteryLevel
                true,                       // IsSupportAT
                true,                      // IsSupportLightBar
                true,                       // IsSupportPlayerLED
                true                       // IsSupportMicLED
            };

            UDPResponse response{
                "DSX Received UDP Instructions",
                MyUtils::currentDateTime(),     // TimeReceived
                true,                       // isControllerConnected
                Battery,                         // BatteryLevel
                {dev}                       // Devices (vector with one device)
            };

            nlohmann::json j = response.to_json();
            std::string responseStr = j.dump(4);
            std::cout << responseStr << std::endl;
            sendto(clientSocket,
                   responseStr.c_str(),
                   responseStr.size(),
                   0,
                   (SOCKADDR *)&clientAddr,
                   clientAddrSize);

            isActive = true;
            nlohmann::json data = nlohmann::json::parse(packetString);
            //cout << packetString << endl;

            for (auto &instr : data["instructions"])
            {
                uint8_t triggers[11];
                Trigger::TriggerMode mode = Trigger::Off;
                AudioEdit edit;
                AudioPlay play;
             
                int type = getParameterValue(instr["type"]);
                int intParam0 = getParameterValue(instr["parameters"][0]);
                int intParam1 = getParameterValue(instr["parameters"][1]);
                int intParam2 = getParameterValue(instr["parameters"][2]);
                int intParam3 = getParameterValue(instr["parameters"][3]);
                int intParam4 = getParameterValue(instr["parameters"][4]);
                int intParam5 = getParameterValue(instr["parameters"][5]);
                int intParam6 = getParameterValue(instr["parameters"][6]);
                int intParam7 = getParameterValue(instr["parameters"][7]);
                int intParam8 = getParameterValue(instr["parameters"][8]);
                int intParam9 = getParameterValue(instr["parameters"][9]);
                int intParam10 = getParameterValue(instr["parameters"][10]);
                int intParam11 = getParameterValue(instr["parameters"][11]);
                int intParam12 = getParameterValue(instr["parameters"][12]);
                int intParam13 = getParameterValue(instr["parameters"][13]);
                //int intParam12 = !instr["parameters"][12].is_null() ? static_cast<int>(instr["parameters"][12]) : 0;

                switch (type)
                {
                    {
                    case 1:
                        uint8_t start = 0;
                        uint8_t end = 0;
                        uint8_t snapForce = 0;
                        uint8_t force = 0;
                        uint8_t firstFoot = 0;
                        uint8_t secondFoot = 0;
                        uint8_t frequency = 0;
                        switch (intParam2) {
                        case 0: //Normal
                        {
                            mode = Trigger::Rigid_B;
                            triggers[0] = 0;
                            triggers[1] = 0;
                            triggers[2] = 0;
                            triggers[3] = 0;
                            triggers[4] = 0;
                            triggers[5] = 0;
                            triggers[6] = 0;
                            triggers[7] = 0;
                            triggers[8] = 0;
                            triggers[9] = 0;
                            triggers[10] = 0;
                            break;
                        }
                        case 1: //GameCube
                        {
                            mode = Trigger::Pulse;
                            triggers[0] = 144;
                            triggers[1] = 160;
                            triggers[2] = 255;
                            triggers[3] = 0;
                            triggers[4] = 0;
                            triggers[5] = 0;
                            triggers[6] = 0;
                            triggers[7] = 0;
                            triggers[8] = 0;
                            triggers[9] = 0;
                            triggers[10] = 0;
                            break;
                        }
                        case 2: //VerySoft
                        {
                            mode = Trigger::Pulse;
                            triggers[0] = 128;
                            triggers[1] = 160;
                            triggers[2] = 255;
                            triggers[3] = 0;
                            triggers[4] = 0;
                            triggers[5] = 0;
                            triggers[6] = 0;
                            triggers[7] = 0;
                            triggers[8] = 0;
                            triggers[9] = 0;
                            triggers[10] = 0;
                            break;
                        }
                        case 3: //Soft
                        {
                            mode = Trigger::Rigid_A;
                            triggers[0] = 69;
                            triggers[1] = 160;
                            triggers[2] = 255;
                            triggers[3] = 0;
                            triggers[4] = 0;
                            triggers[5] = 0;
                            triggers[6] = 0;
                            triggers[7] = 0;
                            triggers[8] = 0;
                            triggers[9] = 0;
                            triggers[10] = 0;
                            break;
                        }
                        case 4: //Hard
                        {
                            mode = Trigger::Rigid_A;
                            triggers[0] = 32;
                            triggers[1] = 160;
                            triggers[2] = 255;
                            triggers[3] = 255;
                            triggers[4] = 255;
                            triggers[5] = 255;
                            triggers[6] = 255;
                            triggers[7] = 0;
                            triggers[8] = 0;
                            triggers[9] = 0;
                            triggers[10] = 0;
                            break;
                        }
                        case 5: //VeryHard
                        {
                            mode = Trigger::Rigid_A;
                            triggers[0] = 16;
                            triggers[1] = 160;
                            triggers[2] = 255;
                            triggers[3] = 255;
                            triggers[4] = 255;
                            triggers[5] = 255;
                            triggers[6] = 255;
                            triggers[7] = 0;
                            triggers[8] = 0;
                            triggers[9] = 0;
                            triggers[10] = 0;
                            break;
                        }
                        case 6: //Hardest
                        {
                            mode = Trigger::Pulse;
                            triggers[0] = 0;
                            triggers[1] = 255;
                            triggers[2] = 255;
                            triggers[3] = 255;
                            triggers[4] = 255;
                            triggers[5] = 255;
                            triggers[6] = 255;
                            triggers[7] = 0;
                            triggers[8] = 0;
                            triggers[9] = 0;
                            triggers[10] = 0;
                            break;
                        }
                        case 7: //Rigid
                        {
                            mode = Trigger::Rigid;
                            triggers[0] = 0;
                            triggers[1] = 255;
                            triggers[2] = 0;
                            triggers[3] = 0;
                            triggers[4] = 0;
                            triggers[5] = 0;
                            triggers[6] = 0;
                            triggers[7] = 0;
                            triggers[8] = 0;
                            triggers[9] = 0;
                            triggers[10] = 0;
                            break;
                        }
                        case 8: //VibrateTrigger
                        {
                            mode = Trigger::Pulse_AB;
                            triggers[0] = 37;
                            triggers[1] = 35;
                            triggers[2] = 6;
                            triggers[3] = 39;
                            triggers[4] = 33;
                            triggers[5] = 35;
                            triggers[6] = 34;
                            triggers[7] = 0;
                            triggers[8] = 0;
                            triggers[9] = 0;
                            triggers[10] = 0;
                            break;
                        }
                        case 9: //Choppy:
                        {
                            mode = Trigger::Rigid_A;
                            triggers[0] = 2;
                            triggers[1] = 39;
                            triggers[2] = 33;
                            triggers[3] = 39;
                            triggers[4] = 38;
                            triggers[5] = 2;
                            triggers[6] = 0;
                            triggers[7] = 0;
                            triggers[8] = 0;
                            triggers[9] = 0;
                            triggers[10] = 0;
                            break;
                        }
                        case 10: //Medium
                        {
                            mode = Trigger::Pulse_A;
                            triggers[0] = 2;
                            triggers[1] = 35;
                            triggers[2] = 1;
                            triggers[3] = 6;
                            triggers[4] = 6;
                            triggers[5] = 1;
                            triggers[6] = 33;
                            triggers[7] = 0;
                            triggers[8] = 0;
                            triggers[9] = 0;
                            triggers[10] = 0;
                            break;
                        }
                        case 11: //VibrateTriggerPulse
                        {
                            mode = Trigger::Pulse_AB;
                            triggers[0] = 37;
                            triggers[1] = 35;
                            triggers[2] = 6;
                            triggers[3] = 39;
                            triggers[4] = 33;
                            triggers[5] = 35;
                            triggers[6] = 34;
                            triggers[7] = 0;
                            triggers[8] = 0;
                            triggers[9] = 0;
                            triggers[10] = 0;
                            break;
                        }
                        case 12: //CustomTriggerValue
                        {
                            switch (intParam3) {
                            case 0:
                                mode = Trigger::Off;
                                break;
                            case 1:
                                mode = Trigger::Rigid;
                                break;
                            case 2:
                                mode = Trigger::Rigid_A;
                                break;
                            case 3:
                                mode = Trigger::Rigid_B;
                                break;
                            case 4:
                                mode = Trigger::Rigid_AB;
                                break;
                            case 5:
                                mode = Trigger::Pulse;
                                break;
                            case 6:
                                mode = Trigger::Pulse_A;
                                break;
                            case 7:
                                mode = Trigger::Pulse_B;
                                break;
                            case 8:
                                mode = Trigger::Pulse_AB;
                                break;
                            case 9:
                                mode = Trigger::Pulse_B;
                                break;
                            case 10:
                                mode = Trigger::Pulse_B2;
                                break;
                            case 11:
                                mode = Trigger::Pulse_B;
                                break;
                            case 12:
                                mode = Trigger::Pulse_B2;
                                break;
                            case 13:
                                mode = Trigger::Pulse_AB;
                                break;
                            case 14:
                                mode = Trigger::Pulse_AB;
                                break;
                            case 15:
                                mode = Trigger::Pulse_AB;
                                break;
                            case 16:
                                mode = Trigger::Pulse_AB;
                                break;
                            default:
                                cout << "Unknown trigger: " << intParam2 << endl;
                                for (auto& a : instr["parameters"]) {
                                    cout << " " << a << " ";
                                }
                                cout << endl;
                                break;
                            }

                            triggers[0] = intParam4;
                            triggers[1] = intParam5;
                            triggers[2] = intParam6;
                            triggers[3] = intParam7;
                            triggers[4] = intParam8;
                            triggers[5] = intParam9;
                            triggers[6] = intParam10;
                            triggers[7] = 0;
                            triggers[8] = 0;
                            triggers[9] = intParam11;
                            triggers[10] = 0;

                            break;
                        }
                        case 13: //Resistance
                        {
                            mode = Trigger::Rigid_B;
                            uint8_t start = intParam3;
                            uint8_t force = intParam4;

                            if (start > 9 || force > 8) {
                                break;
                            }

                            if (force > 0) {
                                uint8_t b = static_cast<uint8_t>((force - 1) & 7);
                                uint32_t num = 0;
                                uint16_t num2 = 0;

                                for (int i = static_cast<int>(start); i < 10;
                                    ++i) {
                                    num |=
                                        (static_cast<uint32_t>(b) << (3 * i));
                                    num2 |= (1 << i);
                                }

                                triggers[0] = static_cast<uint8_t>(num2 & 0xFF);
                                triggers[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                                triggers[2] = static_cast<uint8_t>(num & 0xFF);
                                triggers[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
                                triggers[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
                                triggers[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
                                triggers[6] = 0;
                                triggers[7] = 0;
                                triggers[8] = 0;
                                triggers[9] = 0;
                                triggers[10] = 0;
                            }
                            break;
                        }
                        case 14: //Bow
                        {
                            mode = Trigger::Pulse_A;
                            start = intParam3;
                            end = intParam4;
                            force = intParam5;
                            snapForce = intParam6;

                            if (start > 8 || end > 8 || start >= end ||
                                force > 8 || snapForce > 8) {
                                break;
                            }

                            if (end > 0 && force > 0 && snapForce > 0) {
                                uint16_t num = static_cast<uint16_t>((1 << start) | (1 << end));
                                uint32_t num2 = static_cast<uint32_t>(((force - 1) & 7) | (((snapForce - 1) & 7) << 3));

                                triggers[0] = static_cast<uint8_t>(num & 0xFF);
                                triggers[1] =
                                    static_cast<uint8_t>((num >> 8) & 0xFF);
                                triggers[2] = static_cast<uint8_t>(num2 & 0xFF);
                                triggers[3] =
                                    static_cast<uint8_t>((num2 >> 8) & 0xFF);
                                triggers[4] = 0;
                                triggers[5] = 0;
                                triggers[6] = 0;
                                triggers[7] = 0;
                                triggers[8] = 0;
                                triggers[9] = 0;
                                triggers[10] = 0;
                            }

                            break;
                        }
                        case 15: //Galloping
                        {
                            mode = Trigger::Pulse_A2;

                            start = intParam3;
                            end = intParam4;
                            firstFoot = intParam5;
                            secondFoot = intParam6;
                            frequency = intParam7;

                            if (start > 8) {
                                break;
                            }
                            if (end > 9) {
                                break;
                            }
                            if (start >= end) {
                                break;
                            }
                            if (secondFoot > 7) {
                                break;
                            }
                            if (firstFoot > 6) {
                                break;
                            }
                            if (firstFoot >= secondFoot) {
                                break;
                            }
                            if (frequency > 0) {
                                uint16_t num = static_cast<uint16_t>((1 << start) | (1 << end));
                                uint32_t num2 = static_cast<uint32_t>((secondFoot & 7) | ((firstFoot & 7) << 3));

                                triggers[0] = static_cast<uint8_t>(num & 0xFF);
                                triggers[1] = static_cast<uint8_t>((num >> 8) & 0xFF);
                                triggers[2] = static_cast<uint8_t>(num2 & 0xFF);
                                triggers[3] = frequency;
                                triggers[4] = 0;
                                triggers[5] = 0;
                                triggers[6] = 0;
                                triggers[7] = 0;
                                triggers[8] = 0;
                                triggers[9] = 0;
                            }
                            break;
                        }
                        case 16: //SemiAutomaticGun
                        {
                            mode = Trigger::Rigid_AB;

                            start = intParam3;
                            end = intParam4;
                            force = intParam5;

                            if (start > 7 || start < 2 || end > 8 ||
                                end <= start || force > 8) {
                                break;
                            }

                            if (force > 0) {
                                uint16_t num = static_cast<uint16_t>((1 << start) | (1 << end));

                                triggers[0] = static_cast<uint8_t>(num & 0xFF);
                                triggers[1] = static_cast<uint8_t>((num >> 8) & 0xFF);
                                triggers[2] = force - 1;
                                triggers[3] = 0;
                                triggers[4] = 0;
                                triggers[5] = 0;
                                triggers[6] = 0;
                            }

                            break;
                        }
                        case 17: //AutomaticGun
                        {
                            mode = Trigger::Pulse_B2;
                            start = intParam3;
                            uint8_t strength = intParam4;
                            uint8_t frequency = intParam5;

                            if (start > 9) {
                                break;
                            }
                            if (strength > 8) {
                                break;
                            }
                            if (strength > 0 && frequency > 0) {
                                uint8_t b = (strength - 1) & 7;
                                uint32_t num = 0;
                                uint16_t num2 = 0;

                                for (int i = static_cast<int>(start); i < 10; i++) {
                                    num |= static_cast<uint32_t>(b) << (3 * i);
                                    num2 |= static_cast<uint16_t>(1 << i);
                                }

                                triggers[0] = static_cast<uint8_t>(num2 & 0xFF);
                                triggers[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                                triggers[2] = static_cast<uint8_t>(num & 0xFF);
                                triggers[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
                                triggers[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
                                triggers[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
                                triggers[6] = 0;
                                triggers[7] = 0;
                                triggers[8] = frequency;
                                triggers[9] = 0;
                            }

                            break;
                        }
                        case 18: //Machine
                        {
                            mode = Trigger::Pulse_AB;
                            start = intParam3;
                            end = intParam4;
                            uint8_t strengthA = intParam5;
                            uint8_t strengthB = intParam6;
                            frequency = intParam7;
                            uint8_t period = intParam8;

                            if (start > 8) {
                                break;
                            }
                            if (end > 9) {
                                break;
                            }
                            if (end <= start) {
                                break;
                            }
                            if (strengthA > 7) {
                                break;
                            }
                            if (strengthB > 7) {
                                break;
                            }
                            if (frequency > 0) {
                                uint16_t num = static_cast<uint16_t>((1 << start) | (1 << end));
                                uint32_t num2 = static_cast<uint32_t>((strengthA & 7) | ((strengthB & 7) << 3));

                                triggers[0] = static_cast<uint8_t>(num & 0xFF);
                                triggers[1] = static_cast<uint8_t>((num >> 8) & 0xFF);
                                triggers[2] = static_cast<uint8_t>(num2 & 0xFF);
                                triggers[3] = frequency;
                                triggers[4] = period;
                                triggers[5] = 0;
                                triggers[6] = 0;
                                triggers[7] = 0;
                                triggers[8] = 0;
                                triggers[9] = 0;
                                triggers[10] = 0;
                            }
                            break;

                        }
                        case 19: // VIBRATE_TRIGGER_10Hz
                        {
                            // Idk what this is, im just gonna put this as placeholder
                            mode = Trigger::Pulse_B;
                            triggers[0] = 10;
                            triggers[1] = 255;
                            triggers[2] = 40;
                            triggers[3] = 0;
                            triggers[4] = 0;
                            triggers[5] = 0;
                            triggers[6] = 0;
                            triggers[7] = 0;
                            triggers[8] = 0;
                            triggers[9] = 0;
                            triggers[10] = 0;
                            break;
                        }
                        case 20: // OFF
                        {
                            mode = Trigger::Rigid_B;
                            for (int i = 0; i < 11; i++) {
                                triggers[i] = 0;
                            }
                            break;
                        }
                        case 21: // FEEDBACK
                        {
                            uint8_t position = intParam3;  // Maps to position
                            uint8_t strength = intParam4;  // Maps to strength

                            if (position > 9) {
                                break;
                            }
                            if (strength > 8) {
                                break;
                            }
                            if (strength > 0) {
                                uint8_t b = (strength - 1) & 7;
                                uint32_t num = 0;
                                uint16_t num2 = 0;

                                for (int i = position; i < 10; i++) {
                                    num |= static_cast<uint32_t>(b) << (3 * i);
                                    num2 |= static_cast<uint16_t>(1 << i);
                                }

                                mode = Trigger::Rigid_A;
                                triggers[0] = static_cast<uint8_t>(num2 & 0xFF);
                                triggers[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                                triggers[2] = static_cast<uint8_t>(num & 0xFF);
                                triggers[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
                                triggers[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
                                triggers[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
                                triggers[6] = 0;
                                triggers[7] = 0;
                                triggers[8] = 0;
                                triggers[9] = 0;
                                triggers[10] = 0;
                            } else {
                                mode = Trigger::Rigid_B;
                                for (int i = 0; i < 11; i++) {
                                    triggers[i] = 0;
                                }
                            }

                            break;
                        }
                        case 22: // WEAPON
                        {
                            uint8_t startPosition = intParam3;  // Maps to startPosition
                            uint8_t endPosition = intParam4;    // Maps to endPosition
                            uint8_t strength = intParam5;       // Maps to strength

                            if (startPosition > 7 || startPosition < 2) {
                                break;
                            }
                            if (endPosition > 8) {
                                break;
                            }
                            if (endPosition <= startPosition) {
                                break;
                            }
                            if (strength > 8) {
                                break;
                            }
                            if (strength > 0) {
                                uint16_t num = static_cast<uint16_t>(1 << startPosition | 1 << endPosition);

                                mode = Trigger::Rigid_AB;
                                triggers[0] = static_cast<uint8_t>(num & 0xFF);
                                triggers[1] = static_cast<uint8_t>((num >> 8) & 0xFF);
                                triggers[2] = strength - 1;
                                triggers[3] = 0;
                                triggers[4] = 0;
                                triggers[5] = 0;
                                triggers[6] = 0;
                                triggers[7] = 0;
                                triggers[8] = 0;
                                triggers[9] = 0;
                                triggers[10] = 0;
                            } else {
                                mode = Trigger::Rigid_B;
                                for (int i = 0; i < 11; i++) {
                                    triggers[i] = 0;
                                }
                            }
                            break;
                        }
                        case 23: // VIBRATION
                        {
                            mode = Trigger::Vibration;
                            uint8_t position = intParam3;  // Maps to position
                            uint8_t amplitude = intParam4; // Maps to amplitude
                            uint8_t frequency = intParam5; // Maps to frequency

                            if (position > 9) {
                                break;
                            }
                            if (amplitude > 8) {
                                break;
                            }
                            if (amplitude > 0 && frequency > 0) {
                                uint8_t b = (amplitude - 1) & 7;
                                uint32_t num = 0;
                                uint16_t num2 = 0;

                                for (int i = position; i < 10; i++) {
                                    num |= static_cast<uint32_t>(b) << (3 * i);
                                    num2 |= static_cast<uint16_t>(1 << i);
                                }

                                if(intParam2 == 2)
                                {
                                    triggers[1] = static_cast<uint8_t>(num2 & 0xFF);
                                    triggers[2] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                                    triggers[3] = static_cast<uint8_t>(num & 0xFF);
                                    triggers[4] = static_cast<uint8_t>((num >> 8) & 0xFF);
                                    triggers[5] = static_cast<uint8_t>((num >> 16) & 0xFF);
                                    triggers[6] = static_cast<uint8_t>((num >> 24) & 0xFF);
                                    triggers[7] = 0;
                                    triggers[8] = 0;
                                    triggers[9] = frequency;
                                    triggers[10] = 0;
                                }
                                else {
                                    triggers[0] = static_cast<uint8_t>(num2 & 0xFF);
                                    triggers[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                                    triggers[2] = static_cast<uint8_t>(num & 0xFF);
                                    triggers[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
                                    triggers[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
                                    triggers[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
                                    triggers[6] = 0;
                                    triggers[7] = 0;
                                    triggers[8] = frequency;
                                    triggers[9] = 0;
                                    triggers[10] = 0;
                                }
                            } else {
                                mode = Trigger::Rigid_B;
                                for (int i = 0; i < 11; i++) {
                                    triggers[i] = 0;
                                }
                            }

                            break;
                        }
                        case 24: // SLOPE_FEEDBACK
                        {
                            uint8_t startPosition = intParam3;
                            uint8_t endPosition = intParam4;
                            uint8_t startStrength = intParam5;
                            uint8_t endStrength = intParam6;

                            if (startPosition > 8 || startPosition < 0) {
                                break;
                            }
                            if (endPosition > 9) {
                                break;
                            }
                            if (endPosition <= startPosition) {
                                break;
                            }
                            if (startStrength > 8 || startStrength < 1) {
                                break;
                            }
                            if (endStrength > 8 || endStrength < 1) {
                                break;
                            }

                            uint8_t array[10] = {0};
                            float slope = static_cast<float>(endStrength - startStrength) / static_cast<float>(endPosition - startPosition);
                            for (int i = startPosition; i < 10; i++) {
                                if (i <= endPosition) {
                                    float strengthFloat = static_cast<float>(startStrength) + slope * static_cast<float>(i - startPosition);
                                    array[i] = static_cast<uint8_t>(std::round(strengthFloat));
                                } else {
                                    array[i] = endStrength;
                                }
                            }

                            bool anyStrength = false;
                            for (int i = 0; i < 10; i++) {
                                if (array[i] > 0) {
                                    anyStrength = true;
                                }
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

                                // Set the triggers array
                                mode = Trigger::Rigid_A;
                                triggers[0] = static_cast<uint8_t>(num2 & 0xFF);        
                                triggers[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                                triggers[2] = static_cast<uint8_t>(num & 0xFF);         
                                triggers[3] = static_cast<uint8_t>((num >> 8) & 0xFF);  
                                triggers[4] = static_cast<uint8_t>((num >> 16) & 0xFF); 
                                triggers[5] = static_cast<uint8_t>((num >> 24) & 0xFF); 
                                triggers[6] = 0;
                                triggers[7] = 0; 
                                triggers[8] = 0;  
                                triggers[9] = 0; 
                                triggers[10] = 0; 
                            } else {
                                mode = Trigger::Rigid_B;
                                for (int i = 0; i < 11; i++) {
                                    triggers[i] = 0;
                                }
                            }
                            break;
                        }
                        case 25: // MULTIPLE_POSITION_FEEDBACK
                        {
                            uint8_t strength[10] = {
                                static_cast<uint8_t>(intParam3), static_cast<uint8_t>(intParam4),
                                static_cast<uint8_t>(intParam5), static_cast<uint8_t>(intParam6),
                                static_cast<uint8_t>(intParam7), static_cast<uint8_t>(intParam8),
                                static_cast<uint8_t>(intParam9), static_cast<uint8_t>(intParam10),
                                static_cast<uint8_t>(intParam11), static_cast<uint8_t>(intParam12)
                            };

                            bool anyStrength = false;
                            for (int i = 0; i < 10; i++) {
                                if (strength[i] > 0) {
                                    anyStrength = true;
                                }
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

                                mode = Trigger::Rigid_A;
                                triggers[0] = static_cast<uint8_t>(num2 & 0xFF);
                                triggers[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                                triggers[2] = static_cast<uint8_t>(num & 0xFF);
                                triggers[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
                                triggers[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
                                triggers[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
                                triggers[6] = 0;
                                triggers[7] = 0;
                                triggers[8] = 0;
                                triggers[9] = 0;
                                triggers[10] = 0;
                            } else {
                                mode = Trigger::Rigid_B;
                                for (int i = 0; i < 11; i++) {
                                    triggers[i] = 0;
                                }
                            }
                            break;
                        }
                        case 26: // MULTIPLE_POSITION_VIBRATION
                        {
                            uint8_t frequency = static_cast<uint8_t>(intParam3);
                            uint8_t amplitude[10] = {
                                static_cast<uint8_t>(intParam4), static_cast<uint8_t>(intParam5),
                                static_cast<uint8_t>(intParam6), static_cast<uint8_t>(intParam7),
                                static_cast<uint8_t>(intParam8), static_cast<uint8_t>(intParam9),
                                static_cast<uint8_t>(intParam10), static_cast<uint8_t>(intParam11),
                                static_cast<uint8_t>(intParam12), static_cast<uint8_t>(intParam13)
                            };

                            if (frequency > 0) {
                                bool anyAmplitude = false;
                                for (int i = 0; i < 10; i++) {
                                    if (amplitude[i] > 0) {
                                        anyAmplitude = true;
                                    }
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

                                    mode = Trigger::Pulse_B2;
                                    triggers[0] = static_cast<uint8_t>(num2 & 0xFF);
                                    triggers[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                                    triggers[2] = static_cast<uint8_t>(num & 0xFF);
                                    triggers[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
                                    triggers[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
                                    triggers[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
                                    triggers[6] = 0;
                                    triggers[7] = 0;
                                    triggers[8] = frequency;
                                    triggers[9] = 0;
                                    triggers[10] = 0;
                                } else {
                                    mode = Trigger::Rigid_B;
                                    for (int i = 0; i < 11; i++) {
                                        triggers[i] = 0;
                                    }
                                }
                            } else {
                                mode = Trigger::Rigid_B;
                                for (int i = 0; i < 11; i++) {
                                    triggers[i] = 0;
                                }                            
                            }
                            break;
                        }
                        }

                        if (intParam1 == 1)
                        {
                            thisSettings.ControllerInput.LeftTriggerMode = mode;
                            thisSettings.ControllerInput.LeftTriggerForces[0] =
                                triggers[0];
                            thisSettings.ControllerInput.LeftTriggerForces[1] =
                                triggers[1];
                            thisSettings.ControllerInput.LeftTriggerForces[2] =
                                triggers[2];
                            thisSettings.ControllerInput.LeftTriggerForces[3] =
                                triggers[3];
                            thisSettings.ControllerInput.LeftTriggerForces[4] =
                                triggers[4];
                            thisSettings.ControllerInput.LeftTriggerForces[5] =
                                triggers[5];
                            thisSettings.ControllerInput.LeftTriggerForces[6] =
                                triggers[6];
                            thisSettings.ControllerInput.LeftTriggerForces[7] =
                                triggers[7];
                            thisSettings.ControllerInput.LeftTriggerForces[8] =
                                triggers[8];
                            thisSettings.ControllerInput.LeftTriggerForces[9] =
                                triggers[9];
                            thisSettings.ControllerInput.LeftTriggerForces[10] =
                                triggers[10];
                        }
                        else if (intParam1 == 2)
                        {
                            thisSettings.ControllerInput.RightTriggerMode =
                                mode;
                            thisSettings.ControllerInput.RightTriggerForces[0] =
                                triggers[0];
                            thisSettings.ControllerInput.RightTriggerForces[1] =
                                triggers[1];
                            thisSettings.ControllerInput.RightTriggerForces[2] =
                                triggers[2];
                            thisSettings.ControllerInput.RightTriggerForces[3] =
                                triggers[3];
                            thisSettings.ControllerInput.RightTriggerForces[4] =
                                triggers[4];
                            thisSettings.ControllerInput.RightTriggerForces[5] =
                                triggers[5];
                            thisSettings.ControllerInput.RightTriggerForces[6] =
                                triggers[6];
                            thisSettings.ControllerInput.RightTriggerForces[7] =
                                triggers[7];
                            thisSettings.ControllerInput.RightTriggerForces[8] =
                                triggers[8];
                            thisSettings.ControllerInput.RightTriggerForces[9] =
                                triggers[9];
                            thisSettings.ControllerInput.RightTriggerForces[10] =
                                triggers[10];
                        }
                        break;
                    }
                case 2:
                {
                    thisSettings.ControllerInput.Red = intParam1;
                    thisSettings.ControllerInput.Green = intParam2;
                    thisSettings.ControllerInput.Blue = intParam3;
                    break;
                }
                case 4: // Trigger threshold
                {
                    if (intParam1 == 1) {
                        thisSettings.L2UDPDeadzone = intParam2;
                    }
                    else if (intParam1 == 2) {
                        thisSettings.R2UDPDeadzone = intParam2;
                    }
                    break;
                }
                case 20: // Haptic feedback
                {
                    //cout << "Haptic Feedback instruction received: " << getParameterValueAsString(instr["parameters"][0]) << endl;
                    play.File = getParameterValueAsString(instr["parameters"][0]);
                    play.DontPlayIfAlreadyPlaying = getBoolValue(instr["parameters"][1]);
                    play.Loop = getBoolValue(instr["parameters"][2]);
                    thisSettings.AudioPlayQueue.push_back(play);
                    break;
                }
                case 21: // Edit audio            
                {
                    edit.File = getParameterValueAsString(instr["parameters"][0]);
                    edit.EditType = (AudioEditType)intParam1;
                    edit.Value = getFloatValue(instr["parameters"][2]);                 
                    thisSettings.AudioEditQueue.push_back(edit);
                    break;
                }
                default:
                    cout << "Unrecognized instruction received! Number: " << type << endl;
                    break;
                }
            }
        }

        cout << "Server stopped!" << endl;
    }

    static void StartFakeDSXProcess()
    {
        if (filesystem::exists("utilities\\DSX.exe")) {
            STARTUPINFO si;
            PROCESS_INFORMATION pi;

            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW; // Use this flag to control window visibility
            si.wShowWindow = SW_HIDE;          // Set to SW_HIDE to hide the window

            ZeroMemory(&pi, sizeof(pi));

            std::string command = "utilities\\DSX.exe";

            if (CreateProcess(NULL,             // No module name (use command line)
                              (LPSTR)command.c_str(), // Command line
                              NULL,              // Process handle not inheritable
                              NULL,              // Thread handle not inheritable
                              FALSE,             // Set handle inheritance to FALSE
                              0,                 // No creation flags
                              NULL,              // Use parent's environment block
                              NULL,              // Use parent's starting directory 
                              &si,               // Pointer to STARTUPINFO structure
                              &pi)               // Pointer to PROCESS_INFORMATION structure
               ) {
                // Close process and thread handles.
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
        } else {
            std::cout << "DSX.exe is not present" << std::endl;
        }
    }


    void Dispose()
    {
        serverOn = false;
        closesocket(clientSocket);
        WSACleanup();
    }

private:
    SOCKET clientSocket = INVALID_SOCKET;
    bool serverOn;
    bool newPacket = false;
    bool unavailable;
};
