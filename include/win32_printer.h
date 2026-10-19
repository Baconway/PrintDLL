#pragma once

#include <windows.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "config.h"

struct PrintRect {
    int x;
    int y;
    int width;
    int height;
};

struct PrinterStatusInfo {
    bool found = false;
    bool isVirtualPrompt = false;
    bool isOfflineOrUnavailable = false;
    bool isUsablePhysical = false;
    std::wstring printerName;
    std::wstring driverName;
    std::wstring portName;
    DWORD status = 0;
    DWORD attributes = 0;
    DWORD queuedJobs = 0;
    std::string statusSummary;
};

// Inspects the specified printer (or system default if empty) and determines if it is connected & usable
PrinterStatusInfo InspectPrinterStatus(const std::wstring& printerName = L"");

// Returns true if a usable physical printer is connected and online, or false if stub mode should be used
bool IsPhysicalPrinterAvailable(const std::wstring& printerName = L"", std::string* outReason = nullptr);

// Logs all installed printers and their current status to the logger
void LogInstalledPrintersSummary();

// Calculates exact physical CR80 dimensions in pixels based on printer DPI
PrintRect CalculateCR80PrintRect(HDC hdc, int imageWidth, int imageHeight);

// Prints raw RGB24 buffer to Windows printer (converts RGB to BGR internally, or stubs if no printer connected)
bool PrintToWin32Printer(const uint8_t* rgbBuffer, int width, int height, const std::wstring& printerName);

// Prints two-sided (or single-sided if backRgb is null) raw RGB24 buffers to Windows printer
bool PrintToWin32Printer(const uint8_t* frontRgb, const uint8_t* backRgb, int width, int height, const std::wstring& printerName, CardBackPrintMode printMode = CARDBACK_PRINT_DUPLEX);

// Enumerates all installed printers and returns their names
std::vector<std::wstring> EnumerateInstalledPrinters();

