#include "../include/win32_printer.h"
#include "../include/logger.h"
#include "../include/config.h"
#include <winspool.h>
#include <objbase.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <sstream>

#pragma comment(lib, "winspool.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")

static std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    if (size <= 1) return std::string();
    std::string str(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, NULL, NULL);
    return str;
}

static std::wstring ToLowerW(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    return s;
}

static bool ContainsIgnoreCaseW(const std::wstring& haystack, const std::wstring& needle) {
    return ToLowerW(haystack).find(ToLowerW(needle)) != std::wstring::npos;
}

static bool CheckIsVirtualPromptPrinter(const std::wstring& printerName,
                                        const std::wstring& driverName,
                                        const std::wstring& portName) {
    if (ContainsIgnoreCaseW(printerName, L"Microsoft Print to PDF") ||
        ContainsIgnoreCaseW(printerName, L"Microsoft XPS Document Writer") ||
        ContainsIgnoreCaseW(printerName, L"OneNote") ||
        ContainsIgnoreCaseW(printerName, L"Fax") ||
        ContainsIgnoreCaseW(driverName, L"Microsoft Print To PDF") ||
        ContainsIgnoreCaseW(driverName, L"Microsoft XPS Document Writer")) {
        return true;
    }
    if (ContainsIgnoreCaseW(portName, L"PORTPROMPT:") ||
        ContainsIgnoreCaseW(portName, L"FILE:") ||
        ContainsIgnoreCaseW(portName, L"SHRFAX:")) {
        return true;
    }
    return false;
}

static std::string FormatPrinterStatusFlags(DWORD status, DWORD attributes) {
    std::ostringstream oss;
    oss << "Status=0x" << std::hex << status << ", Attr=0x" << attributes << std::dec;
    std::vector<std::string> flags;
    if (attributes & PRINTER_ATTRIBUTE_WORK_OFFLINE) flags.push_back("WORK_OFFLINE");
    if (status & PRINTER_STATUS_OFFLINE)             flags.push_back("STATUS_OFFLINE");
    if (status & PRINTER_STATUS_NOT_AVAILABLE)       flags.push_back("NOT_AVAILABLE");
    if (status & PRINTER_STATUS_ERROR)               flags.push_back("ERROR");
    if (status & PRINTER_STATUS_PAPER_OUT)           flags.push_back("PAPER_OUT");
    if (status & PRINTER_STATUS_PAPER_JAM)           flags.push_back("PAPER_JAM");
    if (status & PRINTER_STATUS_NO_TONER)            flags.push_back("NO_TONER");
    if (status & PRINTER_STATUS_DOOR_OPEN)           flags.push_back("DOOR_OPEN");
    if (status & PRINTER_STATUS_USER_INTERVENTION)   flags.push_back("USER_INTERVENTION");
    if (status & PRINTER_STATUS_PAUSED)              flags.push_back("PAUSED");
    if (status & PRINTER_STATUS_PENDING_DELETION)    flags.push_back("PENDING_DELETION");
    if (status & PRINTER_STATUS_SERVER_UNKNOWN)      flags.push_back("SERVER_UNKNOWN");

    if (flags.empty()) {
        oss << " (READY/ONLINE)";
    } else {
        oss << " (";
        for (size_t i = 0; i < flags.size(); i++) {
            if (i > 0) oss << "|";
            oss << flags[i];
        }
        oss << ")";
    }
    return oss.str();
}

std::vector<std::wstring> EnumerateInstalledPrinters() {
    std::vector<std::wstring> printers;
    DWORD needed = 0, returned = 0;
    EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 4, NULL, 0, &needed, &returned);
    if (needed > 0) {
        std::vector<BYTE> buffer(needed);
        if (EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 4, buffer.data(), needed, &needed, &returned)) {
            PRINTER_INFO_4W* pInfo = (PRINTER_INFO_4W*)buffer.data();
            for (DWORD i = 0; i < returned; i++) {
                if (pInfo[i].pPrinterName) {
                    printers.push_back(pInfo[i].pPrinterName);
                }
            }
        }
    }
    return printers;
}

PrinterStatusInfo InspectPrinterStatus(const std::wstring& requestedPrinter) {
    PrinterStatusInfo info;
    const AppConfig& cfg = GetConfig();

    std::wstring targetPrinter = requestedPrinter;
    bool usingSystemDefault = false;
    if (targetPrinter.empty()) {
        usingSystemDefault = true;
        wchar_t defaultBuf[256] = {0};
        DWORD bufSize = 256;
        if (GetDefaultPrinterW(defaultBuf, &bufSize) && defaultBuf[0] != L'\0') {
            targetPrinter = defaultBuf;
        } else {
            DWORD err = GetLastError();
            info.found = false;
            info.isUsablePhysical = false;
            info.statusSummary = "No system default printer configured (Win32 Error=" + std::to_string(err) + ")";
            return info;
        }
    }

    info.printerName = targetPrinter;

    HANDLE hPrinter = NULL;
    if (!OpenPrinterW((LPWSTR)targetPrinter.c_str(), &hPrinter, NULL) || !hPrinter) {
        DWORD err = GetLastError();
        info.found = false;
        info.isUsablePhysical = false;
        info.statusSummary = "OpenPrinterW failed for '" + WideToUtf8(targetPrinter) + "' (Win32 Error=" + std::to_string(err) + ")";
        return info;
    }

    info.found = true;
    DWORD needed = 0;
    GetPrinterW(hPrinter, 2, NULL, 0, &needed);
    if (needed > 0) {
        std::vector<BYTE> buffer(needed, 0);
        if (GetPrinterW(hPrinter, 2, buffer.data(), needed, &needed)) {
            PRINTER_INFO_2W* pInfo2 = (PRINTER_INFO_2W*)buffer.data();
            if (pInfo2->pDriverName) info.driverName = pInfo2->pDriverName;
            if (pInfo2->pPortName)   info.portName   = pInfo2->pPortName;
            info.status     = pInfo2->Status;
            info.attributes = pInfo2->Attributes;
            info.queuedJobs = pInfo2->cJobs;
        }
    }
    ClosePrinter(hPrinter);

    info.isVirtualPrompt = CheckIsVirtualPromptPrinter(info.printerName, info.driverName, info.portName);

    const DWORD offlineMask = PRINTER_STATUS_OFFLINE |
                              PRINTER_STATUS_NOT_AVAILABLE |
                              PRINTER_STATUS_ERROR |
                              PRINTER_STATUS_PENDING_DELETION |
                              PRINTER_STATUS_SERVER_UNKNOWN;
    if ((info.attributes & PRINTER_ATTRIBUTE_WORK_OFFLINE) != 0 ||
        (info.status & offlineMask) != 0) {
        info.isOfflineOrUnavailable = true;
    }

    std::ostringstream summary;
    summary << "Printer='" << WideToUtf8(info.printerName) << "'"
            << (usingSystemDefault ? " [System Default]" : " [Configured]")
            << ", Driver='" << WideToUtf8(info.driverName) << "'"
            << ", Port='" << WideToUtf8(info.portName) << "'"
            << ", " << FormatPrinterStatusFlags(info.status, info.attributes)
            << ", VirtualPrompt=" << (info.isVirtualPrompt ? "YES" : "NO")
            << ", Offline=" << (info.isOfflineOrUnavailable ? "YES" : "NO");

    if (cfg.forceStubMode) {
        info.isUsablePhysical = false;
        summary << " -> Decision: STUB (ForceStubMode=1)";
    } else if (info.isVirtualPrompt && cfg.ignoreVirtualPrinters) {
        info.isUsablePhysical = false;
        summary << " -> Decision: STUB (Virtual prompt printer skipped by IgnoreVirtualPrinters=1)";
    } else if (info.isOfflineOrUnavailable && cfg.skipOfflinePrinters) {
        info.isUsablePhysical = false;
        summary << " -> Decision: STUB (Offline/disconnected printer skipped by SkipOfflinePrinters=1)";
    } else {
        info.isUsablePhysical = true;
        summary << " -> Decision: USABLE PHYSICAL PRINTER";
    }

    info.statusSummary = summary.str();
    return info;
}

bool IsPhysicalPrinterAvailable(const std::wstring& printerName, std::string* outReason) {
    const AppConfig& cfg = GetConfig();
    if (cfg.forceStubMode) {
        if (outReason) *outReason = "ForceStubMode=1 is enabled in configuration";
        return false;
    }
    std::wstring target = printerName.empty() ? cfg.printerName : printerName;
    PrinterStatusInfo info = InspectPrinterStatus(target);
    if (outReason) {
        *outReason = info.statusSummary;
    }
    return info.isUsablePhysical;
}

void LogInstalledPrintersSummary() {
    std::vector<std::wstring> printers = EnumerateInstalledPrinters();
    LOG_PRINTER(LOG_LEVEL_INFO, "Enumerated " + std::to_string(printers.size()) + " installed Windows printer(s):");
    for (size_t i = 0; i < printers.size(); i++) {
        PrinterStatusInfo pInfo = InspectPrinterStatus(printers[i]);
        LOG_PRINTER(LOG_LEVEL_INFO, "  [" + std::to_string(i + 1) + "] " + pInfo.statusSummary);
    }

    std::string activeReason;
    bool usable = IsPhysicalPrinterAvailable(GetConfig().printerName, &activeReason);
    if (usable) {
        LOG_PRINTER(LOG_LEVEL_INFO, "Target printer check: READY -> " + activeReason);
    } else {
        LOG_STUB("No usable physical printer active (" + activeReason +
                 "). Stub mode will save cards directly to OutputDir ('" + GetConfig().outputDir + "').");
    }
}

PrintRect CalculateCR80PrintRect(HDC hdc, int imageWidth, int imageHeight) {
    const AppConfig& cfg = GetConfig();
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    int pageW = GetDeviceCaps(hdc, HORZRES);
    int pageH = GetDeviceCaps(hdc, VERTRES);

    int targetW = 0;
    int targetH = 0;

    if (cfg.customPPI > 0) {
        // Direct pixel sizing using target PPI:
        // Size in physical dots = (image pixels / customPPI) * printerDPI
        targetW = (int)std::round(((double)imageWidth / (double)cfg.customPPI) * (double)dpiX);
        targetH = (int)std::round(((double)imageHeight / (double)cfg.customPPI) * (double)dpiY);
    } else {
        // Physical Card Dimensions in mm (default: standard CR80 53.98 mm x 85.60 mm)
        double cardW_mm = (cfg.cardWidthMm > 0.0) ? cfg.cardWidthMm : 53.98;
        double cardH_mm = (cfg.cardHeightMm > 0.0) ? cfg.cardHeightMm : 85.60;

        targetW = (int)std::round(cardW_mm * (double)dpiX / 25.4);
        targetH = (int)std::round(cardH_mm * (double)dpiY / 25.4);

        // If image aspect ratio is preserved:
        double imgAspect = (double)imageWidth / (double)imageHeight;
        double cardAspect = cardW_mm / cardH_mm;
        if (imgAspect > cardAspect) {
            // Wider than card -> fit width
            targetH = (int)std::round((double)targetW / imgAspect);
        } else {
            // Taller than card -> fit height
            targetW = (int)std::round((double)targetH * imgAspect);
        }
    }

    // Apply PrintScale multiplier if configured and not 1.0
    if (cfg.printScale > 0.0 && std::abs(cfg.printScale - 1.0) > 1e-6) {
        targetW = (int)std::round((double)targetW * cfg.printScale);
        targetH = (int)std::round((double)targetH * cfg.printScale);
    }

    // Center on printable area
    int posX = (pageW - targetW) / 2;
    int posY = (pageH - targetH) / 2;
    if (posX < 0) posX = 0;
    if (posY < 0) posY = 0;

    PrintRect rect;
    rect.x = posX;
    rect.y = posY;
    rect.width = targetW;
    rect.height = targetH;
    return rect;
}

// Pure-C SEH wrapper around Win32 GDI printer driver calls so faulty printer drivers can never crash the process
static bool ExecuteGdiPrintJobSEH(const wchar_t* targetPrinter,
                                  DEVMODEW* pDevMode,
                                  const uint8_t* bgrFront,
                                  const uint8_t* bgrBack,
                                  int width,
                                  int height,
                                  int scaleMode,
                                  int printMode,
                                  DWORD* outWin32Err,
                                  DWORD* outSehCode,
                                  int* outStepFailed) {
    *outWin32Err = 0;
    *outSehCode = 0;
    *outStepFailed = 0;

    HDC hdc = NULL;
    __try {
        if (pDevMode) {
            hdc = CreateDCW(L"WINSPOOL", targetPrinter, NULL, pDevMode);
            if (!hdc) {
                hdc = CreateDCW(NULL, targetPrinter, NULL, pDevMode);
            }
        }
        if (!hdc) {
            hdc = CreateDCW(L"WINSPOOL", targetPrinter, NULL, NULL);
        }
        if (!hdc) {
            hdc = CreateDCW(NULL, targetPrinter, NULL, NULL);
        }
        if (!hdc) {
            *outWin32Err = GetLastError();
            *outStepFailed = 1; // CreateDCW failed
            return false;
        }

        BITMAPINFO bmi = {0};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height; // Negative for top-down DIB
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 24;
        bmi.bmiHeader.biCompression = BI_RGB;

        PrintRect printRect;
        if (scaleMode == SCALE_CR80) {
            printRect = CalculateCR80PrintRect(hdc, width, height);
        } else if (scaleMode == SCALE_FIT_PAGE) {
            int pageW = GetDeviceCaps(hdc, HORZRES);
            int pageH = GetDeviceCaps(hdc, VERTRES);
            double imgAspect = (double)width / (double)height;
            double pageAspect = (double)pageW / (double)pageH;
            if (imgAspect > pageAspect) {
                printRect.width = pageW;
                printRect.height = (int)std::round((double)pageW / imgAspect);
                printRect.x = 0;
                printRect.y = (pageH - printRect.height) / 2;
            } else {
                printRect.height = pageH;
                printRect.width = (int)std::round((double)pageH * imgAspect);
                printRect.x = (pageW - printRect.width) / 2;
                printRect.y = 0;
            }
        } else { // SCALE_STRETCH
            printRect.x = 0;
            printRect.y = 0;
            printRect.width = GetDeviceCaps(hdc, HORZRES);
            printRect.height = GetDeviceCaps(hdc, VERTRES);
        }

        SetStretchBltMode(hdc, HALFTONE);
        SetBrushOrgEx(hdc, 0, 0, NULL);

        bool doTwoSided = (bgrBack != nullptr) && (printMode != CARDBACK_PRINT_FILE_ONLY);

        if (!doTwoSided) {
            // Single-sided print job
            DOCINFOW di = { sizeof(DOCINFOW), L"CardMaker Print Job", NULL, NULL, 0 };
            if (StartDocW(hdc, &di) <= 0) {
                *outWin32Err = GetLastError();
                *outStepFailed = 2; // StartDocW failed
                DeleteDC(hdc);
                return false;
            }

            if (StartPage(hdc) <= 0) {
                *outWin32Err = GetLastError();
                *outStepFailed = 3; // StartPage failed
                AbortDoc(hdc);
                DeleteDC(hdc);
                return false;
            }

            int stretchRes = StretchDIBits(
                hdc,
                printRect.x, printRect.y, printRect.width, printRect.height,
                0, 0, width, height,
                bgrFront,
                &bmi,
                DIB_RGB_COLORS,
                SRCCOPY
            );

            if (stretchRes == GDI_ERROR) {
                *outWin32Err = GetLastError();
                *outStepFailed = 4; // StretchDIBits failed
                AbortDoc(hdc);
                DeleteDC(hdc);
                return false;
            }

            EndPage(hdc);
            EndDoc(hdc);
            DeleteDC(hdc);
            return true;
        } else if (printMode == CARDBACK_PRINT_DUPLEX) {
            // Duplex print job: Page 1 Front, Page 2 Back in a single Win32 print document
            DOCINFOW di = { sizeof(DOCINFOW), L"CardMaker Print Job (Duplex)", NULL, NULL, 0 };
            if (StartDocW(hdc, &di) <= 0) {
                *outWin32Err = GetLastError();
                *outStepFailed = 2; // StartDocW failed
                DeleteDC(hdc);
                return false;
            }

            // Page 1: Front
            if (StartPage(hdc) <= 0) {
                *outWin32Err = GetLastError();
                *outStepFailed = 3; // Page 1 StartPage failed
                AbortDoc(hdc);
                DeleteDC(hdc);
                return false;
            }

            if (StretchDIBits(hdc, printRect.x, printRect.y, printRect.width, printRect.height,
                             0, 0, width, height, bgrFront, &bmi, DIB_RGB_COLORS, SRCCOPY) == GDI_ERROR) {
                *outWin32Err = GetLastError();
                *outStepFailed = 4; // Page 1 StretchDIBits failed
                AbortDoc(hdc);
                DeleteDC(hdc);
                return false;
            }

            if (EndPage(hdc) <= 0) {
                *outWin32Err = GetLastError();
                *outStepFailed = 5; // Page 1 EndPage failed
                AbortDoc(hdc);
                DeleteDC(hdc);
                return false;
            }

            // Page 2: Back
            if (StartPage(hdc) <= 0) {
                *outWin32Err = GetLastError();
                *outStepFailed = 6; // Page 2 StartPage failed
                AbortDoc(hdc);
                DeleteDC(hdc);
                return false;
            }

            if (StretchDIBits(hdc, printRect.x, printRect.y, printRect.width, printRect.height,
                             0, 0, width, height, bgrBack, &bmi, DIB_RGB_COLORS, SRCCOPY) == GDI_ERROR) {
                *outWin32Err = GetLastError();
                *outStepFailed = 7; // Page 2 StretchDIBits failed
                AbortDoc(hdc);
                DeleteDC(hdc);
                return false;
            }

            EndPage(hdc);
            EndDoc(hdc);
            DeleteDC(hdc);
            return true;
        } else {
            // Separate Job Mode: Spools Front document then Back document sequentially
            DOCINFOW diFront = { sizeof(DOCINFOW), L"CardMaker Print Job (Front)", NULL, NULL, 0 };
            if (StartDocW(hdc, &diFront) <= 0) {
                *outWin32Err = GetLastError();
                *outStepFailed = 2;
                DeleteDC(hdc);
                return false;
            }

            if (StartPage(hdc) <= 0) {
                *outWin32Err = GetLastError();
                *outStepFailed = 3;
                AbortDoc(hdc);
                DeleteDC(hdc);
                return false;
            }

            if (StretchDIBits(hdc, printRect.x, printRect.y, printRect.width, printRect.height,
                             0, 0, width, height, bgrFront, &bmi, DIB_RGB_COLORS, SRCCOPY) == GDI_ERROR) {
                *outWin32Err = GetLastError();
                *outStepFailed = 4;
                AbortDoc(hdc);
                DeleteDC(hdc);
                return false;
            }

            EndPage(hdc);
            EndDoc(hdc);

            // Job 2: Back
            DOCINFOW diBack = { sizeof(DOCINFOW), L"CardMaker Print Job (Back)", NULL, NULL, 0 };
            if (StartDocW(hdc, &diBack) <= 0) {
                *outWin32Err = GetLastError();
                *outStepFailed = 8;
                DeleteDC(hdc);
                return false;
            }

            if (StartPage(hdc) <= 0) {
                *outWin32Err = GetLastError();
                *outStepFailed = 9;
                AbortDoc(hdc);
                DeleteDC(hdc);
                return false;
            }

            if (StretchDIBits(hdc, printRect.x, printRect.y, printRect.width, printRect.height,
                             0, 0, width, height, bgrBack, &bmi, DIB_RGB_COLORS, SRCCOPY) == GDI_ERROR) {
                *outWin32Err = GetLastError();
                *outStepFailed = 10;
                AbortDoc(hdc);
                DeleteDC(hdc);
                return false;
            }

            EndPage(hdc);
            EndDoc(hdc);
            DeleteDC(hdc);
            return true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *outSehCode = GetExceptionCode();
        *outStepFailed = 99; // SEH Exception in printer driver
        return false;
    }
}

bool PrintToWin32Printer(const uint8_t* frontRgb, const uint8_t* backRgb, int width, int height, const std::wstring& printerName, CardBackPrintMode printMode) {
    if (!frontRgb || width <= 0 || height <= 0) {
        LOG_ERR("PrintToWin32Printer called with invalid image buffer parameters");
        return false;
    }

    const AppConfig& cfg = GetConfig();
    PrinterStatusInfo statusInfo = InspectPrinterStatus(printerName);
    LOG_PRINTER(LOG_LEVEL_INFO, "Inspecting printer before spooling: " + statusInfo.statusSummary);

    if (!statusInfo.isUsablePhysical) {
        if (cfg.stubIfNoPrinter || cfg.forceStubMode) {
            LOG_STUB("No usable physical printer connected (" + statusInfo.statusSummary +
                     "). Bypassing Win32 GDI print spooler (stub mode active).");
            return true;
        } else {
            LOG_ERR("Printer is not usable (" + statusInfo.statusSummary + ") and StubIfNoPrinter=0.");
            return false;
        }
    }

    // Initialize COM on this worker thread in case the printer driver invokes shell/COM components
    HRESULT hrCom = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    bool shouldUninitCom = SUCCEEDED(hrCom);

    std::wstring targetPrinter = statusInfo.printerName;
    LOG_PRINTER(LOG_LEVEL_INFO, "Opening Win32 printer DC for: " + WideToUtf8(targetPrinter));

    std::vector<BYTE> dmBuffer;
    DEVMODEW* pDevMode = NULL;
    HANDLE hPrinter = NULL;
    if (OpenPrinterW((LPWSTR)targetPrinter.c_str(), &hPrinter, NULL) && hPrinter) {
        LONG dmSize = DocumentPropertiesW(NULL, hPrinter, (LPWSTR)targetPrinter.c_str(), NULL, NULL, 0);
        if (dmSize > 0) {
            dmBuffer.resize(dmSize, 0);
            DEVMODEW* candidateDm = (DEVMODEW*)dmBuffer.data();
            if (DocumentPropertiesW(NULL, hPrinter, (LPWSTR)targetPrinter.c_str(), candidateDm, NULL, DM_OUT_BUFFER) == IDOK) {
                pDevMode = candidateDm;
                if (cfg.printerDPI > 0) {
                    pDevMode->dmPrintQuality = (short)cfg.printerDPI;
                    pDevMode->dmYResolution = (short)cfg.printerDPI;
                    pDevMode->dmFields |= DM_PRINTQUALITY | DM_YRESOLUTION;
                    LOG_PRINTER(LOG_LEVEL_INFO, "Requested printer hardware resolution: " + std::to_string(cfg.printerDPI) + " DPI in DEVMODE");
                }
            }
        }
        ClosePrinter(hPrinter);
    }

    // Convert raw RGB to DWORD-aligned BGR for Win32 DIB (StretchDIBits requires 4-byte scanline stride!)
    int stride = ((width * 3 + 3) / 4) * 4;
    std::vector<uint8_t> bgrFront((size_t)stride * height, 0);
    for (int y = 0; y < height; y++) {
        const uint8_t* srcRow = frontRgb + (size_t)y * width * 3;
        uint8_t* dstRow = bgrFront.data() + (size_t)y * stride;
        for (int x = 0; x < width; x++) {
            dstRow[x * 3 + 0] = srcRow[x * 3 + 2]; // Blue
            dstRow[x * 3 + 1] = srcRow[x * 3 + 1]; // Green
            dstRow[x * 3 + 2] = srcRow[x * 3 + 0]; // Red
        }
    }

    std::vector<uint8_t> bgrBack;
    if (backRgb != nullptr && printMode != CARDBACK_PRINT_FILE_ONLY) {
        bgrBack.assign((size_t)stride * height, 0);
        for (int y = 0; y < height; y++) {
            const uint8_t* srcRow = backRgb + (size_t)y * width * 3;
            uint8_t* dstRow = bgrBack.data() + (size_t)y * stride;
            for (int x = 0; x < width; x++) {
                dstRow[x * 3 + 0] = srcRow[x * 3 + 2]; // Blue
                dstRow[x * 3 + 1] = srcRow[x * 3 + 1]; // Green
                dstRow[x * 3 + 2] = srcRow[x * 3 + 0]; // Red
            }
        }
    }

    DWORD win32Err = 0;
    DWORD sehCode = 0;
    int stepFailed = 0;
    bool ok = ExecuteGdiPrintJobSEH(
        targetPrinter.c_str(),
        pDevMode,
        bgrFront.data(),
        bgrBack.empty() ? nullptr : bgrBack.data(),
        width,
        height,
        (int)cfg.scaleMode,
        (int)printMode,
        &win32Err,
        &sehCode,
        &stepFailed
    );

    if (shouldUninitCom) {
        CoUninitialize();
    }

    if (!ok) {
        std::ostringstream errOss;
        errOss << "Win32 print failed at step " << stepFailed << " for printer '" << WideToUtf8(targetPrinter)
               << "' (Win32Err=" << win32Err << ", SEH=0x" << std::hex << sehCode << std::dec << ")";
        if (cfg.stubIfNoPrinter) {
            LOG_WARN(errOss.str() + " -> Recovered safely via StubIfNoPrinter=1.");
            return true;
        }
        LOG_ERR(errOss.str());
        return false;
    }

    std::string printType = (backRgb != nullptr && printMode != CARDBACK_PRINT_FILE_ONLY)
        ? (printMode == CARDBACK_PRINT_DUPLEX ? "two-sided (Duplex)" : "two-sided (Separate Jobs)")
        : "single-sided";

    LOG_PRINTER(LOG_LEVEL_INFO, "Successfully spooled " + printType + " card (" + std::to_string(width) + "x" +
                std::to_string(height) + ") to physical printer: " + WideToUtf8(targetPrinter));
    return true;
}

bool PrintToWin32Printer(const uint8_t* rgbBuffer, int width, int height, const std::wstring& printerName) {
    return PrintToWin32Printer(rgbBuffer, nullptr, width, height, printerName, CARDBACK_PRINT_FILE_ONLY);
}

