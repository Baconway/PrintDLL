#include "../include/config.h"
#include "../include/logger.h"
#include <windows.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

static AppConfig g_config;
static std::string g_dllDir;
static std::string g_exeDir;

static std::string ExtractDirWithSlash(const std::string& filePath) {
    size_t pos = filePath.find_last_of("\\/");
    if (pos != std::string::npos) {
        return filePath.substr(0, pos + 1);
    }
    return "";
}

static std::string ExpandEnvVars(const std::string& input) {
    if (input.empty()) return input;
    char buf[MAX_PATH * 2] = {0};
    DWORD res = ExpandEnvironmentStringsA(input.c_str(), buf, sizeof(buf));
    if (res > 0 && res < sizeof(buf)) {
        return std::string(buf);
    }
    return input;
}

static std::string NormalizeFullPath(const std::string& path) {
    if (path.empty()) return "";
    std::string expanded = ExpandEnvVars(path);
    char fullBuf[MAX_PATH] = {0};
    if (GetFullPathNameA(expanded.c_str(), MAX_PATH, fullBuf, NULL) != 0) {
        return std::string(fullBuf);
    }
    return expanded;
}

static bool IsRegularFile(const std::string& path) {
    if (path.empty()) return false;
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static bool IsDirectoryPath(const std::string& path) {
    if (path.empty()) return false;
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

static void EnsureModuleDirsDetected() {
    if (g_exeDir.empty()) {
        char exePath[MAX_PATH] = {0};
        if (GetModuleFileNameA(NULL, exePath, MAX_PATH) != 0) {
            g_exeDir = ExtractDirWithSlash(exePath);
        }
    }
    if (g_dllDir.empty()) {
        HMODULE hMod = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)&EnsureModuleDirsDetected, &hMod) && hMod != NULL) {
            char dllPath[MAX_PATH] = {0};
            if (GetModuleFileNameA(hMod, dllPath, MAX_PATH) != 0) {
                g_dllDir = ExtractDirWithSlash(dllPath);
            }
        }
    }
}

void SetModuleDirectories(const std::string& dllDir, const std::string& exeDir) {
    if (!dllDir.empty()) {
        g_dllDir = dllDir;
        if (g_dllDir.back() != '\\' && g_dllDir.back() != '/') g_dllDir += "\\";
    }
    if (!exeDir.empty()) {
        g_exeDir = exeDir;
        if (g_exeDir.back() != '\\' && g_exeDir.back() != '/') g_exeDir += "\\";
    }
}

#ifdef GetDllDirectory
#undef GetDllDirectory
#endif

std::string GetDllDirectory() {
    EnsureModuleDirsDetected();
    return g_dllDir;
}

std::string GetExeDirectory() {
    EnsureModuleDirsDetected();
    return g_exeDir;
}

std::string GetPrintDllDirectory() {
    return GetDllDirectory();
}

std::string GetPrintExeDirectory() {
    return GetExeDirectory();
}

AppConfig& GetConfig() {
    return g_config;
}

static void AddCandidate(std::vector<std::string>& candidates, const std::string& rawPath) {
    if (rawPath.empty()) return;
    std::string norm = NormalizeFullPath(rawPath);
    if (norm.empty()) return;
    for (const auto& existing : candidates) {
        if (_stricmp(existing.c_str(), norm.c_str()) == 0) {
            return;
        }
    }
    candidates.push_back(norm);
}

std::string FindConfigFile(const char* hintPath, std::vector<std::string>* outSearchedPaths) {
    EnsureModuleDirsDetected();
    std::vector<std::string> candidates;

    char cwdBuf[MAX_PATH] = {0};
    GetCurrentDirectoryA(MAX_PATH, cwdBuf);
    std::string cwd = cwdBuf;
    if (!cwd.empty() && cwd.back() != '\\' && cwd.back() != '/') cwd += "\\";

    // 1. Environment variable override: PRINTDLL_INI
    char envBuf[MAX_PATH] = {0};
    DWORD envLen = GetEnvironmentVariableA("PRINTDLL_INI", envBuf, MAX_PATH);
    if (envLen > 0 && envLen < MAX_PATH) {
        std::string envStr(envBuf);
        if (IsDirectoryPath(envStr)) {
            AddCandidate(candidates, envStr + "\\PrintDLL.ini");
        } else {
            AddCandidate(candidates, envStr);
        }
    }

    // 2. If caller passed an explicit file path (e.g. LoadConfig("test_custom.ini") or custom path)
    if (hintPath && hintPath[0] != '\0') {
        std::string hint(hintPath);
        bool isDefaultName = (_stricmp(hint.c_str(), "PrintDLL.ini") == 0);
        if (!isDefaultName) {
            if (IsDirectoryPath(hint)) {
                AddCandidate(candidates, hint + "\\PrintDLL.ini");
                AddCandidate(candidates, hint + "\\..\\..\\UserData\\PrintDLL.ini");
            } else {
                // Check if it looks like a file path (.ini / .cfg) or existing file
                size_t dotPos = hint.find_last_of('.');
                bool hasIniExt = (dotPos != std::string::npos) &&
                                 (_stricmp(hint.c_str() + dotPos, ".ini") == 0 || _stricmp(hint.c_str() + dotPos, ".cfg") == 0);
                if (hasIniExt || IsRegularFile(hint)) {
                    AddCandidate(candidates, hint);
                    if (!g_exeDir.empty()) AddCandidate(candidates, g_exeDir + hint);
                    if (!g_dllDir.empty()) AddCandidate(candidates, g_dllDir + hint);
                } else {
                    // Might be a relative directory name like "Plugins" or "CardMaker_Data/Plugins"
                    AddCandidate(candidates, hint + "\\PrintDLL.ini");
                    if (!g_exeDir.empty()) AddCandidate(candidates, g_exeDir + hint + "\\PrintDLL.ini");
                }
            }
        }
    }

    // 3. UserData folders (MelonLoader / Unity UserData convention)
    if (!cwd.empty()) {
        AddCandidate(candidates, cwd + "UserData\\PrintDLL.ini");
        AddCandidate(candidates, cwd + "UserData\\Config\\PrintDLL.ini");
    }
    if (!g_exeDir.empty()) {
        AddCandidate(candidates, g_exeDir + "UserData\\PrintDLL.ini");
        AddCandidate(candidates, g_exeDir + "UserData\\Config\\PrintDLL.ini");
    }
    if (!g_dllDir.empty()) {
        // If PrintDLL.dll is in <GameRoot>/CardMaker_Data/Plugins/, ../../UserData is <GameRoot>/UserData
        AddCandidate(candidates, g_dllDir + "..\\..\\UserData\\PrintDLL.ini");
        // If PrintDLL.dll is in <GameRoot>/Plugins/, ../UserData is <GameRoot>/UserData
        AddCandidate(candidates, g_dllDir + "..\\UserData\\PrintDLL.ini");
        AddCandidate(candidates, g_dllDir + "UserData\\PrintDLL.ini");
    }

    // 4. Current working directory & Game Root directory
    if (!cwd.empty()) {
        AddCandidate(candidates, cwd + "PrintDLL.ini");
    }
    if (!g_exeDir.empty()) {
        AddCandidate(candidates, g_exeDir + "PrintDLL.ini");
    }
    if (!g_dllDir.empty()) {
        AddCandidate(candidates, g_dllDir + "..\\..\\PrintDLL.ini");
        AddCandidate(candidates, g_dllDir + "..\\PrintDLL.ini");
    }

    // 5. DLL directory & Plugins directories
    if (!g_dllDir.empty()) {
        AddCandidate(candidates, g_dllDir + "PrintDLL.ini");
    }
    if (!g_exeDir.empty()) {
        AddCandidate(candidates, g_exeDir + "CardMaker_Data\\Plugins\\PrintDLL.ini");
        AddCandidate(candidates, g_exeDir + "CardMaker_Data\\Plugins\\x86_64\\PrintDLL.ini");
        AddCandidate(candidates, g_exeDir + "Plugins\\PrintDLL.ini");
    }
    if (!cwd.empty()) {
        AddCandidate(candidates, cwd + "CardMaker_Data\\Plugins\\PrintDLL.ini");
        AddCandidate(candidates, cwd + "Plugins\\PrintDLL.ini");
    }

    if (outSearchedPaths) {
        *outSearchedPaths = candidates;
    }

    for (const auto& candidate : candidates) {
        if (IsRegularFile(candidate)) {
            return candidate;
        }
    }

    // Default fallback path if no file exists on disk yet
    if (!g_dllDir.empty()) return NormalizeFullPath(g_dllDir + "PrintDLL.ini");
    return NormalizeFullPath("PrintDLL.ini");
}

static std::string WideToUtf8Str(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    if (size <= 1) return std::string();
    std::string str(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, NULL, NULL);
    return str;
}

void LoadConfig(const char* iniPath) {
    EnsureModuleDirsDetected();

    std::vector<std::string> searchedPaths;
    std::string resolvedIni = FindConfigFile(iniPath, &searchedPaths);
    bool iniFoundOnDisk = IsRegularFile(resolvedIni);

    // Check if the INI file redirects to another CustomIniPath (e.g. pointing to a shared config)
    if (iniFoundOnDisk) {
        char redirectBuf[MAX_PATH] = {0};
        GetPrivateProfileStringA("General", "CustomIniPath", "", redirectBuf, MAX_PATH, resolvedIni.c_str());
        if (redirectBuf[0] != '\0') {
            std::string redirected = NormalizeFullPath(redirectBuf);
            if (IsDirectoryPath(redirected)) {
                redirected = NormalizeFullPath(redirected + "\\PrintDLL.ini");
            }
            if (IsRegularFile(redirected) && _stricmp(redirected.c_str(), resolvedIni.c_str()) != 0) {
                resolvedIni = redirected;
            }
        }
    }

    const char* fullIniPath = resolvedIni.c_str();
    g_config.loadedIniPath = resolvedIni;
    g_config.loadedIniDir = ExtractDirWithSlash(resolvedIni);

    // General
    g_config.outputMode = (OutputMode)GetPrivateProfileIntA("General", "OutputMode", 2, fullIniPath);
    g_config.simulateDelayMs = (uint32_t)GetPrivateProfileIntA("General", "SimulateDelayMs", 2000, fullIniPath);
    int enableLogGen = GetPrivateProfileIntA("General", "EnableLog", 1, fullIniPath);
    g_config.enableLog = (GetPrivateProfileIntA("Logging", "EnableLog", enableLogGen, fullIniPath) != 0);

    // Detailed Logging ([Logging] section with fallback to [General])
    char logLevelBuf[32] = {0};
    GetPrivateProfileStringA("General", "LogLevel", "DEBUG", logLevelBuf, sizeof(logLevelBuf), fullIniPath);
    GetPrivateProfileStringA("Logging", "LogLevel", logLevelBuf, logLevelBuf, sizeof(logLevelBuf), fullIniPath);
    g_config.logLevel = logLevelBuf;

    char logFileBuf[MAX_PATH] = {0};
    GetPrivateProfileStringA("General", "LogFile", "", logFileBuf, MAX_PATH, fullIniPath);
    GetPrivateProfileStringA("Logging", "LogFile", logFileBuf, logFileBuf, MAX_PATH, fullIniPath);
    std::string rawLogFile = ExpandEnvVars(logFileBuf);
    if (rawLogFile.empty()) {
        if (!g_dllDir.empty()) {
            g_config.logFilePath = NormalizeFullPath(g_dllDir + "PrintDLL.log");
        } else if (!g_config.loadedIniDir.empty()) {
            g_config.logFilePath = NormalizeFullPath(g_config.loadedIniDir + "PrintDLL.log");
        } else {
            g_config.logFilePath = NormalizeFullPath("PrintDLL.log");
        }
    } else {
        g_config.logFilePath = NormalizeFullPath(rawLogFile);
    }

    g_config.logAppend = GetPrivateProfileIntA("Logging", "AppendLog", 1, fullIniPath) != 0;
    g_config.logToConsole = GetPrivateProfileIntA("Logging", "LogToConsole", 0, fullIniPath) != 0;
    g_config.logToDebugger = GetPrivateProfileIntA("Logging", "LogToDebugger", 1, fullIniPath) != 0;
    g_config.logTimestamps = GetPrivateProfileIntA("Logging", "LogTimestamps", 1, fullIniPath) != 0;
    g_config.logThreadId = GetPrivateProfileIntA("Logging", "LogThreadId", 1, fullIniPath) != 0;
    g_config.logApiCalls = GetPrivateProfileIntA("Logging", "LogApiCalls", 1, fullIniPath) != 0;

    // Configure Logger immediately so subsequent config details are logged with user preferences
    Logger::Instance().Configure(
        g_config.enableLog,
        Logger::ParseLevel(g_config.logLevel),
        g_config.logFilePath,
        g_config.logAppend,
        g_config.logToConsole,
        g_config.logToDebugger,
        g_config.logTimestamps,
        g_config.logThreadId,
        g_config.logApiCalls
    );

    // Printer & Stub Mode
    wchar_t printerBuf[256] = {0};
    wchar_t fullIniPathW[MAX_PATH] = {0};
    MultiByteToWideChar(CP_ACP, 0, fullIniPath, -1, fullIniPathW, MAX_PATH);
    GetPrivateProfileStringW(L"Printer", L"PrinterName", L"", printerBuf, 256, fullIniPathW);
    g_config.printerName = printerBuf;
    g_config.scaleMode = (ScaleMode)GetPrivateProfileIntA("Printer", "ScaleMode", 0, fullIniPath);

    int stubGen = GetPrivateProfileIntA("General", "StubIfNoPrinter", 1, fullIniPath);
    g_config.stubIfNoPrinter = GetPrivateProfileIntA("Printer", "StubIfNoPrinter", stubGen, fullIniPath) != 0;
    g_config.ignoreVirtualPrinters = GetPrivateProfileIntA("Printer", "IgnoreVirtualPrinters", 1, fullIniPath) != 0;
    g_config.skipOfflinePrinters = GetPrivateProfileIntA("Printer", "SkipOfflinePrinters", 1, fullIniPath) != 0;
    int forceStubGen = GetPrivateProfileIntA("General", "ForceStubMode", 0, fullIniPath);
    g_config.forceStubMode = GetPrivateProfileIntA("Printer", "ForceStubMode", forceStubGen, fullIniPath) != 0;

    // Advanced Print Scaling & PPI/DPI settings
    g_config.customPPI = GetPrivateProfileIntA("Printer", "CustomPPI", 0, fullIniPath);

    char wBuf[32] = {0};
    GetPrivateProfileStringA("Printer", "CardWidthMm", "63.20", wBuf, sizeof(wBuf), fullIniPath);
    g_config.cardWidthMm = strtod(wBuf, NULL);
    if (g_config.cardWidthMm <= 0.0) g_config.cardWidthMm = 63.20;

    char hBuf[32] = {0};
    GetPrivateProfileStringA("Printer", "CardHeightMm", "88.10", hBuf, sizeof(hBuf), fullIniPath);
    g_config.cardHeightMm = strtod(hBuf, NULL);
    if (g_config.cardHeightMm <= 0.0) g_config.cardHeightMm = 88.10;

    char scaleBuf[32] = {0};
    GetPrivateProfileStringA("Printer", "PrintScale", "1.0", scaleBuf, sizeof(scaleBuf), fullIniPath);
    g_config.printScale = strtod(scaleBuf, NULL);
    if (g_config.printScale <= 0.0) g_config.printScale = 1.0;

    g_config.printerDPI = GetPrivateProfileIntA("Printer", "PrinterDPI", 0, fullIniPath);

    // FileExport
    char outDirBuf[MAX_PATH] = {0};
    GetPrivateProfileStringA("FileExport", "OutputDir", "./printed_cards", outDirBuf, MAX_PATH, fullIniPath);
    g_config.outputDir = ExpandEnvVars(outDirBuf);

    char fmtBuf[16] = {0};
    GetPrivateProfileStringA("FileExport", "ImageFormat", "png", fmtBuf, 16, fullIniPath);
    if (_stricmp(fmtBuf, "bmp") == 0) {
        g_config.exportFormat = FORMAT_BMP;
    } else {
        g_config.exportFormat = FORMAT_PNG;
    }

    g_config.fixColors = GetPrivateProfileIntA("FileExport", "FixColors", 1, fullIniPath) != 0;

    char encBuf[16] = {0};
    GetPrivateProfileStringA("FileExport", "ColorEncoding", "RGB", encBuf, 16, fullIniPath);
    g_config.colorEncoding = encBuf;
    if (_stricmp(encBuf, "BGR") == 0) {
        g_config.fixColors = false;
    }

    g_config.saveHoloMask = GetPrivateProfileIntA("FileExport", "SaveHoloMask", 1, fullIniPath) != 0;
    g_config.combineHolo = GetPrivateProfileIntA("FileExport", "CombineHolo", 1, fullIniPath) != 0;
    g_config.fileDPI = GetPrivateProfileIntA("FileExport", "FileDPI", 0, fullIniPath);

    // HardwareEmulation
    char serialBuf[32] = {0};
    GetPrivateProfileStringA("HardwareEmulation", "SerialNumber", "12345678", serialBuf, 32, fullIniPath);
    g_config.serialNumber = serialBuf;

    char tidBuf[64] = {0};
    GetPrivateProfileStringA("HardwareEmulation", "CardTID", "E00401001234567890ABCDEF", tidBuf, 64, fullIniPath);
    if (strlen(tidBuf) >= 24) {
        for (int i = 0; i < 12; i++) {
            char byteStr[3] = { tidBuf[i * 2], tidBuf[i * 2 + 1], 0 };
            g_config.cardTID[i] = (uint8_t)strtoul(byteStr, NULL, 16);
        }
    }

    // CardBack
    g_config.cardBack.enabled = GetPrivateProfileIntA("CardBack", "EnableCardBack", 1, fullIniPath) != 0;

    char cardBackImgBuf[MAX_PATH] = {0};
    GetPrivateProfileStringA("CardBack", "CardBackImage", "card_backs/UI_CMU_GachaScene_100000.png", cardBackImgBuf, MAX_PATH, fullIniPath);
    g_config.cardBack.customImagePath = ExpandEnvVars(cardBackImgBuf);

    char printModeBuf[32] = {0};
    GetPrivateProfileStringA("CardBack", "PrintCardBackMode", "0", printModeBuf, sizeof(printModeBuf), fullIniPath);
    if (_stricmp(printModeBuf, "separate") == 0 || _stricmp(printModeBuf, "separate_job") == 0 || strcmp(printModeBuf, "1") == 0) {
        g_config.cardBack.printMode = CARDBACK_PRINT_SEPARATE_JOB;
    } else if (_stricmp(printModeBuf, "file_only") == 0 || _stricmp(printModeBuf, "file") == 0 || strcmp(printModeBuf, "2") == 0) {
        g_config.cardBack.printMode = CARDBACK_PRINT_FILE_ONLY;
    } else {
        g_config.cardBack.printMode = CARDBACK_PRINT_DUPLEX;
    }

    g_config.cardBack.saveCardBack = GetPrivateProfileIntA("CardBack", "SaveCardBack", 1, fullIniPath) != 0;
    g_config.cardBack.saveCardSpread = GetPrivateProfileIntA("CardBack", "SaveCardSpread", 1, fullIniPath) != 0;
    g_config.cardBack.autoGenerateTemplate = GetPrivateProfileIntA("CardBack", "AutoGenerateTemplateIfMissing", 1, fullIniPath) != 0;

    // Bleed & Cutting Border
    g_config.bleed.enabled = GetPrivateProfileIntA("Bleed", "EnableBleed", 1, fullIniPath) != 0;

    char bleedWBuf[32] = {0};
    GetPrivateProfileStringA("Bleed", "FinalCardWidthMm", "63.2", bleedWBuf, sizeof(bleedWBuf), fullIniPath);
    g_config.bleed.finalCardWidthMm = strtod(bleedWBuf, NULL);
    if (g_config.bleed.finalCardWidthMm <= 0.0) g_config.bleed.finalCardWidthMm = 63.2;

    char bleedHBuf[32] = {0};
    GetPrivateProfileStringA("Bleed", "FinalCardHeightMm", "88.1", bleedHBuf, sizeof(bleedHBuf), fullIniPath);
    g_config.bleed.finalCardHeightMm = strtod(bleedHBuf, NULL);
    if (g_config.bleed.finalCardHeightMm <= 0.0) g_config.bleed.finalCardHeightMm = 88.1;

    char bleedMarginBuf[32] = {0};
    GetPrivateProfileStringA("Bleed", "BleedMm", "1.0", bleedMarginBuf, sizeof(bleedMarginBuf), fullIniPath);
    g_config.bleed.bleedMm = strtod(bleedMarginBuf, NULL);
    if (g_config.bleed.bleedMm < 0.0) g_config.bleed.bleedMm = 1.0;

    char radiusBuf[32] = {0};
    GetPrivateProfileStringA("Bleed", "CornerRadiusMm", "2.5", radiusBuf, sizeof(radiusBuf), fullIniPath);
    g_config.bleed.cornerRadiusMm = strtod(radiusBuf, NULL);
    if (g_config.bleed.cornerRadiusMm < 0.0) g_config.bleed.cornerRadiusMm = 2.5;

    char colorBuf[64] = {0};
    GetPrivateProfileStringA("Bleed", "BleedColor", "0,0,0", colorBuf, sizeof(colorBuf), fullIniPath);
    if (_stricmp(colorBuf, "black") == 0 || strcmp(colorBuf, "0") == 0) {
        g_config.bleed.bleedColorR = 0;
        g_config.bleed.bleedColorG = 0;
        g_config.bleed.bleedColorB = 0;
    } else if (colorBuf[0] == '#' || (strlen(colorBuf) == 6 && strchr(colorBuf, ',') == NULL)) {
        const char* hexStart = (colorBuf[0] == '#') ? colorBuf + 1 : colorBuf;
        unsigned int hexVal = (unsigned int)strtoul(hexStart, NULL, 16);
        g_config.bleed.bleedColorR = (uint8_t)((hexVal >> 16) & 0xFF);
        g_config.bleed.bleedColorG = (uint8_t)((hexVal >> 8) & 0xFF);
        g_config.bleed.bleedColorB = (uint8_t)(hexVal & 0xFF);
    } else {
        int r = 0, g = 0, b = 0;
        if (sscanf_s(colorBuf, "%d,%d,%d", &r, &g, &b) == 3) {
            g_config.bleed.bleedColorR = (uint8_t)std::clamp(r, 0, 255);
            g_config.bleed.bleedColorG = (uint8_t)std::clamp(g, 0, 255);
            g_config.bleed.bleedColorB = (uint8_t)std::clamp(b, 0, 255);
        } else {
            g_config.bleed.bleedColorR = 0;
            g_config.bleed.bleedColorG = 0;
            g_config.bleed.bleedColorB = 0;
        }
    }

    g_config.bleed.applyToFileExport = GetPrivateProfileIntA("Bleed", "ApplyToFileExport", 1, fullIniPath) != 0;
    g_config.bleed.applyToPhysicalPrint = GetPrivateProfileIntA("Bleed", "ApplyToPhysicalPrint", 1, fullIniPath) != 0;

    // Log diagnostic summary of configuration resolution
    if (iniFoundOnDisk) {
        LOG_CFG("Loaded configuration from: " + resolvedIni);
    } else {
        LOG_WARN("No PrintDLL.ini file found on disk; using built-in defaults (searched " +
                 std::to_string(searchedPaths.size()) + " candidate locations).");
        for (size_t i = 0; i < searchedPaths.size(); i++) {
            LOG_DEBUG("  Searched [" + std::to_string(i + 1) + "]: " + searchedPaths[i]);
        }
    }

    LOG_CFG("Active Settings: OutputMode=" + std::to_string((int)g_config.outputMode) +
            ", OutputDir='" + NormalizeFullPath(g_config.outputDir) + "'" +
            ", Format=" + std::string(g_config.exportFormat == FORMAT_BMP ? "BMP" : "PNG") +
            ", FixColors=" + std::to_string(g_config.fixColors) +
            ", ColorEncoding=" + g_config.colorEncoding +
            ", FileDPI=" + std::to_string(g_config.fileDPI) +
            ", PrinterName='" + (g_config.printerName.empty() ? "<System Default>" : WideToUtf8Str(g_config.printerName)) + "'" +
            ", ScaleMode=" + std::to_string((int)g_config.scaleMode) +
            ", CustomPPI=" + std::to_string(g_config.customPPI) +
            ", CardDimensionsMm=" + std::to_string(g_config.cardWidthMm) + "x" + std::to_string(g_config.cardHeightMm) +
            ", PrintScale=" + std::to_string(g_config.printScale) +
            ", PrinterDPI=" + std::to_string(g_config.printerDPI) +
            ", StubIfNoPrinter=" + std::to_string(g_config.stubIfNoPrinter) +
            ", IgnoreVirtualPrinters=" + std::to_string(g_config.ignoreVirtualPrinters) +
            ", SkipOfflinePrinters=" + std::to_string(g_config.skipOfflinePrinters) +
            ", ForceStubMode=" + std::to_string(g_config.forceStubMode) +
            ", LogLevel=" + g_config.logLevel +
            ", LogFile='" + g_config.logFilePath + "'");

    LOG_CFG("CardBack Settings: Enabled=" + std::to_string(g_config.cardBack.enabled) +
            ", ImagePath='" + g_config.cardBack.customImagePath + "'" +
            ", PrintMode=" + std::to_string((int)g_config.cardBack.printMode) +
            ", SaveCardBack=" + std::to_string(g_config.cardBack.saveCardBack) +
            ", SaveCardSpread=" + std::to_string(g_config.cardBack.saveCardSpread) +
            ", AutoGenerateTemplate=" + std::to_string(g_config.cardBack.autoGenerateTemplate));

    LOG_CFG("Bleed Settings: Enabled=" + std::to_string(g_config.bleed.enabled) +
            ", FinalCard=" + std::to_string(g_config.bleed.finalCardWidthMm) + "x" + std::to_string(g_config.bleed.finalCardHeightMm) + " mm" +
            ", BleedMargin=" + std::to_string(g_config.bleed.bleedMm) + " mm" +
            ", CornerRadius=" + std::to_string(g_config.bleed.cornerRadiusMm) + " mm" +
            ", Color=RGB(" + std::to_string(g_config.bleed.bleedColorR) + "," + std::to_string(g_config.bleed.bleedColorG) + "," + std::to_string(g_config.bleed.bleedColorB) + ")" +
            ", FileExport=" + std::to_string(g_config.bleed.applyToFileExport) +
            ", PhysicalPrint=" + std::to_string(g_config.bleed.applyToPhysicalPrint));
}

