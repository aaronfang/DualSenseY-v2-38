cmake_minimum_required(VERSION 3.28)
set(CMAKE_CXX_STANDARD 20)
set(EXE_SOURCES "main.cpp" "cycle.cpp" "DualSense.h")
set(EXE_HEADERS "cycle.hpp" "DualSense.h")
add_executable("DSX" ${EXE_SOURCES} ${EXE_HEADERS} "DualSense.h" "miniaudio.h"    "ControllerEmulation.h" "IMMNotificationClient.h" "UDP.h" "Settings.h"  "Config.h" "MyUtils.h" "MyUtils.cpp" "icon.rc" "stb_image.h" "icon.ico" "Strings.h")
set_property(TARGET "DSX" PROPERTY VS_DPI_AWARE "PerMonitor")

include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
                         GIT_TAG bb01c8db702fb41e5497aee9c0559ddf4bf13749) # Replace with your desired git commit from: https://github.com/libcpr/cpr/releases
FetchContent_MakeAvailable(cpr)

target_link_libraries("DSX" PRIVATE cpr::cpr)
target_link_libraries("DSX" PRIVATE fmt::fmt-header-only)
target_link_libraries("DSX" PRIVATE glfw)
target_link_libraries("DSX" PRIVATE OpenGL::GL)
target_link_libraries("DSX" PRIVATE imgui::imgui)
target_link_libraries("DSX" PRIVATE implot::implot)
target_link_libraries("DSX" PRIVATE "hidapi_winapi")
target_link_libraries("DSX" PRIVATE setupapi cfgmgr32)
target_link_libraries("DSX" PRIVATE setupapi ViGEmClient)
target_link_libraries("DSX" PRIVATE nlohmann_json)
target_link_libraries("DSX" PRIVATE "tray")
target_link_libraries("DSX" PRIVATE "Crashlogs")
target_link_libraries("DSX" PRIVATE winmm)

target_set_warnings(
    TARGET
    "DSX"
    ENABLE
    ${ENABLE_WARNINGS}
    AS_ERRORS
    ${ENABLE_WARNINGS_AS_ERRORS})

if(${ENABLE_LTO})
    target_enable_lto("DSX" optimized)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_clang_tidy_to_target("DSX")
endif()
