#pragma once

#include <string>
#include <vector>
#include <stdint.h>

enum OutputMode {
    OUTPUT_FILE_ONLY = 0,
    OUTPUT_PRINTER_ONLY = 1,
    OUTPUT_BOTH = 2
};

enum ScaleMode {
    SCALE_CR80 = 0,
    SCALE_FIT_PAGE = 1,
    SCALE_STRETCH = 2
};

enum ExportFormat {
    FORMAT_PNG = 0,
    FORMAT_BMP = 1
};

enum CardBackPrintMode {
    CARDBACK_PRINT_DUPLEX = 0,       // Win32 duplex (Page 1 Front, Page 2 Back in 1 document)
    CARDBACK_PRINT_SEPARATE_JOB = 1, // Two separate print jobs (Front doc, Back doc)
    CARDBACK_PRINT_FILE_ONLY = 2     // Save back to file only; print front only physically
};

struct BleedConfig {
    bool enabled = true;                     // EnableBleed (default: true)
    double finalCardWidthMm = 63.2;          // FinalCardWidthMm (default: 63.2 mm)
    double finalCardHeightMm = 88.1;         // FinalCardHeightMm (default: 88.1 mm)
    double bleedMm = 1.0;                    // BleedMm (default: 1.0 mm per side)
    double cornerRadiusMm = 2.5;             // CornerRadiusMm (default: 2.5 mm)
    uint8_t bleedColorR = 0;                 // BleedColor R (default: 0 = black for easy cutting)
    uint8_t bleedColorG = 0;                 // BleedColor G (default: 0)
    uint8_t bleedColorB = 0;                 // BleedColor B (default: 0)
    bool applyToFileExport = true;           // ApplyToFileExport (default: true)
    bool applyToPhysicalPrint = true;        // ApplyToPhysicalPrint (default: true)
};

struct CardBackConfig {
    bool enabled = true;                     // EnableCardBack (default: true -> two-sided printing)
    std::string customImagePath = "card_backs/UI_CMU_GachaScene_100000.png"; // CardBackImage
    CardBackPrintMode printMode = CARDBACK_PRINT_DUPLEX; // PrintCardBackMode (default: 0 = duplex)
    bool saveCardBack = true;                 // SaveCardBack (default: true)
    bool saveCardSpread = true;               // SaveCardSpread (default: true)
    bool autoGenerateTemplate = true;         // AutoGenerateTemplateIfMissing (default: true)
};

struct AppConfig {
    OutputMode outputMode = OUTPUT_BOTH;
    uint32_t simulateDelayMs = 2000;
    bool enableLog = true;

    // Printer & Stub settings
    std::wstring printerName = L"";
    ScaleMode scaleMode = SCALE_CR80;
    bool stubIfNoPrinter = true;
    bool ignoreVirtualPrinters = true;
    bool skipOfflinePrinters = true;
    bool forceStubMode = false;
    int customPPI = 0;              // Target physical print PPI (0 = use standard CR80 / ScaleMode calculation)
    double cardWidthMm = 63.20;     // Physical card width in mm (default: 63.20 mm)
    double cardHeightMm = 88.10;    // Physical card height in mm (default: 88.10 mm)
    double printScale = 1.0;        // Multiplier scale for physical print size (default: 1.0 = 100%)
    int printerDPI = 0;             // Requested printer hardware resolution in DEVMODE (0 = driver default)

    // File export settings
    std::string outputDir = "./printed_cards";
    ExportFormat exportFormat = FORMAT_PNG;
    bool fixColors = true;
    std::string colorEncoding = "RGB";
    bool saveHoloMask = true;
    bool combineHolo = true;
    int fileDPI = 0;                // Target DPI metadata embedded in saved PNG/BMP (0 = default GDI+ 96 DPI)

    // Detailed logging settings
    std::string logLevel = "DEBUG";
    std::string logFilePath = "";
    bool logAppend = true;
    bool logToConsole = false;
    bool logToDebugger = true;
    bool logTimestamps = true;
    bool logThreadId = true;
    bool logApiCalls = true;

    // Card back settings
    CardBackConfig cardBack;

    // Bleed & cutting border settings
    BleedConfig bleed;

    // Hardware emulation settings
    std::string serialNumber = "12345678";
    uint8_t cardTID[12] = { 0xE0, 0x04, 0x01, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF };

    // Resolved INI metadata
    std::string loadedIniPath = "";
    std::string loadedIniDir = "";
};

#ifdef GetDllDirectory
#undef GetDllDirectory
#endif

AppConfig& GetConfig();
void SetModuleDirectories(const std::string& dllDir, const std::string& exeDir);
std::string GetDllDirectory();
std::string GetExeDirectory();
std::string GetPrintDllDirectory();
std::string GetPrintExeDirectory();
std::string FindConfigFile(const char* hintPath = nullptr, std::vector<std::string>* outSearchedPaths = nullptr);
void LoadConfig(const char* iniPath = nullptr);

