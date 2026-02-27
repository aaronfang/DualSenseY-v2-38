#include "MyUtils.h"
#include <errno.h>
#include <codecvt>
#include "IMMNotificationClient.h"
#define SYSERROR()  errno

NotificationClient *client;
IMMDeviceEnumerator *deviceEnumerator = nullptr;
IMMDevice *device = nullptr;
IAudioMeterInformation *meterInfo = nullptr;

namespace MyUtils {
    int ConvertRange(int value, int oldMin, int oldMax, int newMin, int newMax)
    {
        if (oldMin == oldMax)
        {
            throw std::invalid_argument("Old minimum and maximum cannot be equal.");
        }
        float ratio = static_cast<float>(newMax - newMin) /
                      static_cast<float>(oldMax - oldMin);
        float scaledValue = (value - oldMin) * ratio + newMin;
        return std::clamp(static_cast<int>(scaledValue), newMin, newMax);
    }

    void StartAudioToHaptics(const std::string& Name) {
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        std::string narrowId = Name; // Assuming narrow string return
        std::wstring wideId = converter.from_bytes(narrowId);
        std::wstring arg1 = L"\"" + wideId + L"\"";
        std::wstring command = L"utilities\\AudioPassthrough.exe " + arg1;

        std::wcout << L"Starting audio to haptics" << std::endl << L"Arg1: " << arg1 << std::endl;

        if (CreateProcessW(NULL, (LPWSTR)command.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

    BOOL IsRunAsAdministrator()
    {
        BOOL fIsRunAsAdmin = FALSE;
        DWORD dwError = ERROR_SUCCESS;
        PSID pAdministratorsGroup = NULL;

        // Allocate and initialize a SID of the administrators group.
        SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
        if (!AllocateAndInitializeSid(
            &NtAuthority, 
            2, 
            SECURITY_BUILTIN_DOMAIN_RID, 
            DOMAIN_ALIAS_RID_ADMINS, 
            0, 0, 0, 0, 0, 0, 
            &pAdministratorsGroup))
        {
            dwError = GetLastError();
            goto Cleanup;
        }

        // Determine whether the SID of administrators group is enabled in 
        // the primary access token of the process.
        if (!CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin))
        {
            dwError = GetLastError();
            goto Cleanup;
        }

    Cleanup:
        // Centralized cleanup for all allocated resources.
        if (pAdministratorsGroup)
        {
            FreeSid(pAdministratorsGroup);
            pAdministratorsGroup = NULL;
        }

        // Throw the error if something failed in the function.
        if (ERROR_SUCCESS != dwError)
        {
            throw dwError;
        }

        return fIsRunAsAdmin;
    }

    void ElevateNow()
    {
        BOOL bAlreadyRunningAsAdministrator = FALSE;
        try
        {
            bAlreadyRunningAsAdministrator = IsRunAsAdministrator();
        }
        catch(...)
        {
            std::cout << "Failed to determine if application was running with admin rights" << std::endl;
            DWORD dwErrorCode = GetLastError();
            TCHAR szMessage[256];
            _stprintf_s(szMessage, ARRAYSIZE(szMessage), _T("Error code returned was 0x%08lx"), dwErrorCode);
            std::cout << szMessage << std::endl;
        }

        if (!bAlreadyRunningAsAdministrator)
        {
            wchar_t szPath[MAX_PATH];
            if (GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath)))
            {
                // Launch itself as admin
                SHELLEXECUTEINFOW sei = { sizeof(sei) };
                sei.lpVerb = L"runas";
                sei.lpFile = szPath;
                sei.hwnd = NULL;
                sei.nShow = SW_NORMAL;

                if (!ShellExecuteExW(&sei))
                {
                    DWORD dwError = GetLastError();
                    if (dwError == ERROR_CANCELLED)
                    {
                        std::cout << "End user did not allow elevation" << std::endl;
                    }
                }
                else
                {
                    _exit(1);  // Quit itself
                }
            }
        }
    }

    float CalculateScaleFactor(int screenWidth, int screenHeight) {
        // Choose a baseline resolution, e.g., 1920x1080
        const int baseWidth = 1920;
        const int baseHeight = 1080;

        // Calculate the scale factor based on the vertical or horizontal scale difference
        float widthScale = static_cast<float>(screenWidth) / baseWidth;
        float heightScale = static_cast<float>(screenHeight) / baseHeight;

        // Use an average or the larger scale factor, depending on preference
        return (widthScale + heightScale) / 1.2f;
    }

    void StartHidHideRequest(std::string ID, std::string arg) {
        STARTUPINFO si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW; // Use this flag to control window visibility
        si.wShowWindow = SW_HIDE;          // Set to SW_HIDE to prevent the window from showing

        ZeroMemory(&pi, sizeof(pi));

        std::string arg1 = "\"" + ID + "\"";
        std::string arg2 = " \"" + arg + "\" ";
        std::string command = "utilities\\hidhide_service_request.exe " + arg1 + arg2;

        if (CreateProcess(NULL,              // No module name (use command line)
                          (LPSTR)command.c_str(), // Command line
                          NULL,               // Process handle not inheritable
                          NULL,               // Thread handle not inheritable
                          FALSE,              // Set handle inheritance to FALSE
                          0,                  // No creation flags
                          NULL,               // Use parent's environment block
                          NULL,               // Use parent's starting directory 
                          &si,                // Pointer to STARTUPINFO structure
                          &pi)                // Pointer to PROCESS_INFORMATION structure
           ) {
            // Close process and thread handles. 
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

    void RunAsyncHidHideRequest(std::string ID, std::string arg) {
        std::thread asyncThread(StartHidHideRequest, ID, arg);
        asyncThread.detach();  // Detach the thread to run independently
    }

    void InitializeAudioEndpoint()
    {
        CoInitialize(nullptr);

        CoCreateInstance(__uuidof(MMDeviceEnumerator),
                         nullptr,
                         CLSCTX_ALL,
                         __uuidof(IMMDeviceEnumerator),
                         (void **)&deviceEnumerator);
        deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);

        client = new NotificationClient();
        deviceEnumerator->RegisterEndpointNotificationCallback(client);

        device->Activate(__uuidof(IAudioMeterInformation),
                         CLSCTX_ALL,
                         nullptr,
                         (void **)&meterInfo);

        CoUninitialize();
    }

    void DeinitializeAudioEndpoint()
    {
        if (meterInfo)
        {
            meterInfo->Release();
            meterInfo = nullptr;
        }

        if (device)
        {
            device->Release();
            device = nullptr;
        }

        if (client)
        {
            deviceEnumerator->UnregisterEndpointNotificationCallback(client);
            delete client;
            client = nullptr;
        }

        if (deviceEnumerator)
        {
            deviceEnumerator->Release();
            deviceEnumerator = nullptr;
        }

        CoUninitialize();
    }

    std::string WStringToString(const std::wstring& wstr)
    {
	    std::string str;
	    size_t size;
	    str.resize(wstr.length());
	    wcstombs_s(&size, &str[0], str.size() + 1, wstr.c_str(), wstr.size());
	    return str;
    }

    void MoveCursor(int x, int y)
    {
        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        input.mi.dx = x;
        input.mi.dy = y;

        SendInput(1, &input, sizeof(INPUT));
    }

    float GetCurrentAudioPeak()
    {
        if (client->DeviceChanged)
        {
            if (meterInfo)
                meterInfo->Release();
            if (device)
                device->Release();
            if (deviceEnumerator)
                deviceEnumerator->Release();
            client->DeviceChanged = false;
            InitializeAudioEndpoint();
        }

        float peakValue = 0.0f;
        if (meterInfo->GetPeakValue(&peakValue) == S_OK)
        {
            return peakValue;
        }
        else
        {
            return 0;
        }
    }

    void SaveImGuiColorsToFile(const ImGuiStyle& style) {
        OPENFILENAMEW ofn;

        char szFileName[MAX_PATH] = "";

        ZeroMemory(&ofn, sizeof(ofn));

        ofn.lStructSize = sizeof(ofn); 
        ofn.hwndOwner = NULL;
        ofn.lpstrFilter = (LPCWSTR)L"DualSenseY Style (*.dsst)\0*.dsst";
        ofn.lpstrFile = (LPWSTR)szFileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        ofn.lpstrDefExt = (LPCWSTR)L"dscf";

        GetSaveFileNameW(&ofn);

        std::wstring ws( ofn.lpstrFile ); 
        std::string filename = std::string( ws.begin(), ws.end() );

        std::ofstream file(filename);

        if (file.is_open()) {
            for (int i = 0; i < ImGuiCol_COUNT; ++i) {
                const ImVec4& color = style.Colors[i];
                file << "style.Colors[" << i << "] = ImVec4("
                     << color.x << ", " << color.y << ", " << color.z << ", " << color.w << ");" << std::endl;
            }
            file.close();
            printf("ImGui style colors saved to %s\n", filename);
        } else {
            printf("Unable to open file for writing.\n");
        }
    }

    void LoadImGuiColorsFromFile(ImGuiStyle& style) {
        OPENFILENAMEW ofn;
        char szFileName[MAX_PATH] = "";

        ZeroMemory(&ofn, sizeof(ofn));

        ofn.lStructSize = sizeof(ofn); 
        ofn.hwndOwner = NULL;
        ofn.lpstrFilter = (LPCWSTR)L"DualSenseY Style (*.dsst)\0*.dsst";
        ofn.lpstrFile = (LPWSTR)szFileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        ofn.lpstrDefExt = (LPCWSTR)L"dscf";

        GetOpenFileNameW(&ofn);

        std::wstring ws( ofn.lpstrFile ); 
        std::string filename = std::string( ws.begin(), ws.end() );

        std::ifstream file(filename);
        std::string line;

        if (file.is_open()) {
            while (std::getline(file, line)) {
                int index;
                float r, g, b, a;
                if (sscanf(line.c_str(), "style.Colors[%d] = ImVec4(%f, %f, %f, %f);", &index, &r, &g, &b, &a) == 5) {
                    style.Colors[index] = ImVec4(r, g, b, a);
                }
            }
            file.close();
            printf("ImGui style colors loaded from %s\n", filename);
        } else {
            printf("Unable to open file for reading.\n");
        }
    }

    void LoadImGuiColorsFromFilepath(ImGuiStyle& style, std::string filename) {
        std::ifstream file(filename);
        std::string line;

        if (file.is_open()) {
            while (std::getline(file, line)) {
                int index;
                float r, g, b, a;
                if (sscanf(line.c_str(), "style.Colors[%d] = ImVec4(%f, %f, %f, %f);", &index, &r, &g, &b, &a) == 5) {
                    style.Colors[index] = ImVec4(r, g, b, a);
                }
            }
            file.close();
            printf("ImGui style colors loaded from %s\n", filename);
        } else {
            printf("Unable to open file for reading.\n");
        }
    }

    bool LoadTextureFromMemory(const void* data, size_t data_size, GLuint* out_texture, int* out_width, int* out_height)
    {
        // Load from file
        int image_width = 0;
        int image_height = 0;
        unsigned char* image_data = stbi_load_from_memory((const unsigned char*)data, (int)data_size, &image_width, &image_height, NULL, 4);
        if (image_data == NULL)
            return false;

        // Create a OpenGL texture identifier
        GLuint image_texture;
        glGenTextures(1, &image_texture);
        glBindTexture(GL_TEXTURE_2D, image_texture);

        // Setup filtering parameters for display
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Upload pixels into texture
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
        stbi_image_free(image_data);

        *out_texture = image_texture;
        *out_width = image_width;
        *out_height = image_height;

        return true;
    }

    // Open and read a file, then forward to LoadTextureFromMemory()
    bool LoadTextureFromFile(const char* file_name, GLuint* out_texture, int* out_width, int* out_height)
    {
        FILE* f = fopen(file_name, "rb");
        if (f == NULL)
            return false;
        fseek(f, 0, SEEK_END);
        size_t file_size = (size_t)ftell(f);
        if (file_size == -1)
            return false;
        fseek(f, 0, SEEK_SET);
        void* file_data = IM_ALLOC(file_size);
        fread(file_data, 1, file_size, f);
        bool ret = LoadTextureFromMemory(file_data, file_size, out_texture, out_width, out_height);
        IM_FREE(file_data);
        return ret;
    }

    std::string GetExecutablePath() {
    char buffer[MAX_PATH];
    HMODULE hModule = GetModuleHandle(NULL); // Get handle of current module
    if (hModule != NULL) {
        // Get the full path of the executable
        GetModuleFileName(hModule, buffer, sizeof(buffer));
        return std::string(buffer); // Return as std::string
    }
    return ""; // Return an empty string if failed
    }

    std::string GetExecutableFolderPath() {
        char buffer[MAX_PATH];
        HMODULE hModule = GetModuleHandle(NULL); // Get handle of current module
        if (hModule != NULL) {
            // Get the full path of the executable
            GetModuleFileName(hModule, buffer, sizeof(buffer));

            // Convert to std::string for easier manipulation
            std::string exePath(buffer);

            // Find the last occurrence of the path separator
            std::string::size_type pos = exePath.find_last_of("\\/");
            if (pos != std::string::npos) {
                // Return the substring up to the last separator
                return exePath.substr(0, pos);
            }
        }
        return ""; // Return an empty string if failed
    }

    void AddToStartup() {
        std::string progPath = GetExecutablePath(); // Update this path
        HKEY hkey;
    
        // Create or open the registry key
        long createStatus = RegCreateKey(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &hkey);
        if (createStatus == ERROR_SUCCESS) {
            // Set the value in the registry
            long status = RegSetValueEx(hkey, "DualSenseY", 0, REG_SZ, (BYTE*)progPath.c_str(), (progPath.size() + 1) * sizeof(wchar_t));
            RegCloseKey(hkey); // Close the key
        }
    }

    void RemoveFromStartup() {
        HKEY hkey;

        // Open the registry key
        long openStatus = RegOpenKey(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &hkey);
        if (openStatus == ERROR_SUCCESS) {
            // Delete the specified value from the registry
            long deleteStatus = RegDeleteValue(hkey, "DualSenseY");
            RegCloseKey(hkey); // Close the key

            if (deleteStatus == ERROR_SUCCESS) {
                std::cout << "Successfully removed from startup: " << "DualSenseY" << std::endl;
            } else {
                std::cerr << "Failed to remove registry value. It may not exist." << std::endl;
            }
        } else {
            std::cerr << "Failed to open registry key." << std::endl;
        }
    }

    std::string GetImagesFolderPath() {
        char path[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_MYPICTURES, NULL, 0, path) == S_OK) {
            return std::string(path);
        }
        return "";
    }

    std::string GetLocalFolderPath() {
        char path[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path) == S_OK) {
            return std::string(path);
        }
        return "";
    }

    std::string currentDateTime()
    {
        std::time_t now = std::time(nullptr);
        std::tm *nowTm = std::localtime(&now);
        char buf[100];
        std::strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M:%S %p", nowTm);
        return buf;
    }

    std::string sanitizeFileName(const std::string& input) {
        std::string sanitized = input;
        const std::string invalidChars = "\\/:*?\"<>|";

        sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(), [&](char c) {
            return invalidChars.find(c) != std::string::npos;
            }), sanitized.end());

        return sanitized;
    }

    int scaleFloatToInt(float input_float, float max_float) {
        const int max_int = 255;
    
        // Ensure input is within the valid range
        if (input_float < 0.0f) {
            input_float = 0.0f;
        } else if (input_float > max_float) {
            input_float = max_float;
        }

        // Scale the float to an integer in the range [0, 255]
        int scaled_int = static_cast<int>((input_float / max_float) * max_int);
    
        return scaled_int;
    }

    std::string USBtoHIDinstance(const std::string& input) {
        std::string result = input;

        // Replace "USB" with "HID"
        size_t pos = result.find("USB");
        if (pos != std::string::npos) {
            result.replace(pos, 3, "HID");
        }

        // Convert the string to uppercase
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);

        return result;
    }

    std::wstring ConvertToWideString(const std::string& str) {
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
        std::wstring wstr(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
        return wstr;
    }

    bool GetConfigPathForController(std::string &Path, std::string ControllerID) {
        std::ifstream configFile(MyUtils::GetLocalFolderPath() + "\\DualSenseY\\DefaultConfigs\\" + sanitizeFileName(ControllerID));
        if (configFile.good()) {
            getline(configFile, Path);
            configFile.close();
            return true;
        }
        else {
            std::cerr<<"Failed to open file : "<<SYSERROR()<<std::endl;
            return false;
        }
    }

    bool WriteDefaultConfigPath(std::string Path, std::string ControllerID) {
        std::filesystem::create_directories(MyUtils::GetLocalFolderPath() + "\\DualSenseY\\DefaultConfigs");
        std::ofstream configFile(MyUtils::GetLocalFolderPath() + "\\DualSenseY\\DefaultConfigs\\" + sanitizeFileName(ControllerID));
        if (configFile.is_open()) {
            configFile << Path;
            configFile.close();
            return true;
        }
        else {
            std::cerr<<"Failed to open file : "<<SYSERROR()<<std::endl;
            return false;
        }
    }

    void RemoveConfig(std::string ControllerID) {
        std::string path = MyUtils::GetLocalFolderPath() + "\\DualSenseY\\DefaultConfigs\\" + sanitizeFileName(ControllerID);
        std::remove(path.c_str());
    }

    std::string currentDateTimeWMS()
    {
        auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm *nowTm = std::localtime(&now_c);
    
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(nowTm, "%Y-%m-%d %H-%M-%S") << '-' << std::setfill('0') << std::setw(3) << milliseconds.count();
    return oss.str();
    }

    void SaveBitmapFromClipboard(const char* filename) {
    // Open the clipboard
    if (!OpenClipboard(NULL)) {
        std::cerr << "Failed to open clipboard" << std::endl;
        return;
    }

    // Get the bitmap from the clipboard
    HBITMAP hBitmap = (HBITMAP)GetClipboardData(CF_BITMAP);
    if (!hBitmap) {
        std::cerr << "No bitmap available in clipboard" << std::endl;
        CloseClipboard();
        return;
    }

    // Create a file to save the bitmap
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to create file" << std::endl;
        CloseClipboard();
        return;
    }

    // Get bitmap information
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    // Create a BITMAPFILEHEADER
    BITMAPFILEHEADER bfh;
    bfh.bfType = 0x4D42; // 'BM'
    bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + (bmp.bmWidthBytes * bmp.bmHeight);
    bfh.bfReserved1 = 0;
    bfh.bfReserved2 = 0;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    // Create a BITMAPINFOHEADER
    BITMAPINFOHEADER bih;
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = bmp.bmWidth;
    bih.biHeight = bmp.bmHeight;
    bih.biPlanes = 1;
    bih.biBitCount = 32; // Assuming 32 bits per pixel (ARGB)
    bih.biCompression = BI_RGB;
    bih.biSizeImage = 0;
    bih.biXPelsPerMeter = 0;
    bih.biYPelsPerMeter = 0;
    bih.biClrUsed = 0;
    bih.biClrImportant = 0;

    // Write headers to file
    file.write(reinterpret_cast<char*>(&bfh), sizeof(BITMAPFILEHEADER));
    file.write(reinterpret_cast<char*>(&bih), sizeof(BITMAPINFOHEADER));

    // Lock the bitmap bits
    HDC hdc = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hdc);
    SelectObject(hMemDC, hBitmap);
    
    // Write pixel data
    int widthBytes = bmp.bmWidthBytes;
    BYTE* pPixels = new BYTE[widthBytes * bmp.bmHeight];
    GetBitmapBits(hBitmap, widthBytes * bmp.bmHeight, pPixels);

    // Flip the pixel data vertically
    BYTE* pFlippedPixels = new BYTE[widthBytes * bmp.bmHeight];
    for (int y = 0; y < bmp.bmHeight; ++y) {
        memcpy(pFlippedPixels + (bmp.bmHeight - 1 - y) * widthBytes, 
               pPixels + y * widthBytes, 
               widthBytes);
    }

    // Write flipped pixel data to the file
    file.write(reinterpret_cast<char*>(pFlippedPixels), widthBytes * bmp.bmHeight);

    // Clean up
    delete[] pPixels;
    delete[] pFlippedPixels;
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hdc);
    CloseClipboard();
    file.close();

    std::cout << "Bitmap saved to " << filename << std::endl;
}

    void TakeScreenShot() {
        keybd_event(VK_CONTROL, 0, 0, 0); 
        keybd_event(VK_SNAPSHOT, 0, 0, 0); 

        keybd_event(VK_SNAPSHOT, 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    }
}
