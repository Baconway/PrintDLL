#include <windows.h>
#include "../include/logger.h"
#include "../include/config.h"

static std::string ExtractDir(const char* fullPath) {
    if (!fullPath) return "";
    std::string s(fullPath);
    size_t pos = s.find_last_of("\\/");
    if (pos != std::string::npos) {
        return s.substr(0, pos + 1);
    }
    return "";
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);

            char dllPath[MAX_PATH] = {0};
            GetModuleFileNameA(hModule, dllPath, MAX_PATH);
            std::string dllDir = ExtractDir(dllPath);

            char exePath[MAX_PATH] = {0};
            GetModuleFileNameA(NULL, exePath, MAX_PATH);
            std::string exeDir = ExtractDir(exePath);

            SetModuleDirectories(dllDir, exeDir);

            // Load configuration from any supported directory (UserData, ExeDir, DllDir, etc.)
            // Note: Do NOT call EnumPrintersW inside DllMain to avoid Windows Loader Lock deadlocks.
            LoadConfig(nullptr);
            LOG_INFO("PrintDLL attached to process (DLL='" + std::string(dllPath) + "', EXE='" + std::string(exePath) + "')");
            break;
        }
        case DLL_PROCESS_DETACH:
            LOG_INFO("PrintDLL detached from process");
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}

