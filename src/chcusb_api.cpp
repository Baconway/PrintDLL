#include "../include/chcusb.h"
#include "../include/config.h"
#include "../include/logger.h"
#include "../include/win32_printer.h"
#include "../include/file_export.h"
#include "../include/card_back.h"
#include "../include/bleed_helper.h"
#include <mutex>
#include <vector>
#include <chrono>
#include <thread>
#include <cstring>
#include <exception>

struct PrinterContext {
    bool isOpen = false;
    unsigned short currentStandbyPos = 0;

    // Image parameters
    unsigned short format = 3;
    unsigned short ncomp = 3;
    unsigned short depth = 8;
    unsigned short width = kWIDTH;
    unsigned short height = kHEIGHT;

    // Buffers
    std::vector<uint8_t> colorBuffer;
    std::vector<uint8_t> holoBuffer;
    bool hasHolo = false;

    // Page state
    unsigned short currentPageId = 1;
    bool isPageOpen = false;
    std::chrono::steady_clock::time_point pageEndTime;
    bool isPrinting = false;

    // Counters
    uint32_t printCounter = 0;
    uint32_t headCounter = 0;
    uint32_t cutterCounter = 0;

    // Paper & print mode state cached from setPrinterInfo
    uint8_t paperInfo[10] = { 2, 0x00, 0x03, 0x1C, 0x04, 0, 0, 0, 0, 0 };
    uint8_t polishMode[2] = { 21, 0 };

    std::mutex mutex;
};

static PrinterContext g_context;
static bool g_printersLoggedOnce = false;

static const char* GetTagName(unsigned short tagNumber) {
    switch (tagNumber) {
        case kPINFTAG_PAPER:      return "kPINFTAG_PAPER(0)";
        case kPINFTAG_TONER:      return "kPINFTAG_TONER(1)";
        case kPINFTAG_ENGID:      return "kPINFTAG_ENGID(3)";
        case kPINFTAG_PRINTCNT2:  return "kPINFTAG_PRINTCNT2(5)";
        case kPINFTAG_PRINTSTAND: return "kPINFTAG_PRINTSTAND(10)";
        case kPINFTAG_MEMORY:     return "kPINFTAG_MEMORY(15)";
        case kPINFTAG_PRINTMODE:  return "kPINFTAG_PRINTMODE(20)";
        case kPINFTAG_CARDINFO:   return "kPINFTAG_CARDINFO(25)";
        case kPINFTAG_SERIALINFO: return "kPINFTAG_SERIALINFO(26)";
        case kPINFTAG_TEMP:       return "kPINFTAG_TEMP(30)";
        case kPINFTAG_ERRHISTORY: return "kPINFTAG_ERRHISTORY(40)";
        case kPINFTAG_TONECURVE:  return "kPINFTAG_TONECURVE(50)";
        case kPINFTAG_M19_INFO:   return "kPINFTAG_M19_INFO(85)";
        default:                  return "UNKNOWN_TAG";
    }
}

void* CHC_API CHCUSB_init(const char* dllpath) {
    LoadConfig(dllpath);
    LOG_API("CHCUSB_init(dllpath='" + std::string(dllpath ? dllpath : "<null>") + "')");

    if (!g_printersLoggedOnce) {
        g_printersLoggedOnce = true;
        LogInstalledPrintersSummary();
    }

    return &g_context;
}

void CHC_API CHCUSB_term(void* handle) {
    LOG_API("CHCUSB_term()");
}

int CHC_API CHCUSB_open(void* handle, const char* dllpath, unsigned short* rResult) {
    LOG_API("CHCUSB_open(dllpath='" + std::string(dllpath ? dllpath : "<null>") + "')");
    if (!handle) return 0;
    auto* ctx = (PrinterContext*)handle;
    std::lock_guard<std::mutex> lock(ctx->mutex);
    ctx->isOpen = true;
    if (rResult) *rResult = 0;
    return 1;
}

void CHC_API CHCUSB_close(void* handle) {
    LOG_API("CHCUSB_close()");
    if (!handle) return;
    auto* ctx = (PrinterContext*)handle;
    std::lock_guard<std::mutex> lock(ctx->mutex);
    ctx->isOpen = false;
}

unsigned short CHC_API CHCUSB_listupPrinter(void* handle, unsigned char* rIdArray) {
    LOG_API("CHCUSB_listupPrinter() -> 1 printer (ID=0)");
    if (rIdArray) rIdArray[0] = 0;
    return 1;
}

unsigned char CHC_API CHCUSB_selectPrinter(void* handle, unsigned char printerId, unsigned short* rResult) {
    LOG_API("CHCUSB_selectPrinter(printerId=" + std::to_string(printerId) + ")");
    if (rResult) *rResult = 0;
    return 0;
}

unsigned short CHC_API CHCUSB_listupPrinterSN(void* handle, SerialNumRec* rSerialArray) {
    const std::string& sn = GetConfig().serialNumber;
    LOG_API("CHCUSB_listupPrinterSN() -> returning 1 emulated printer (SN='" + sn + "')");
    if (rSerialArray) {
        memset(&rSerialArray[0], 0, sizeof(SerialNumRec));
        for (int i = 0; i < 8 && i < (int)sn.length(); i++) {
            ((uint8_t*)&rSerialArray[0])[i] = (uint8_t)sn[i];
        }
    }
    return 1;
}

void* CHC_API CHCUSB_selectPrinterSN(void* handle, SerialNumRec* printerSN, unsigned short* rResult) {
    LOG_API("CHCUSB_selectPrinterSN()");
    if (rResult) *rResult = 0;
    return handle ? handle : &g_context;
}

int CHC_API CHCUSB_status(void* handle, unsigned short* rResult) {
    LOG_TRACE("CHCUSB_status() -> kRESULT_STATUS_READY (0)");
    if (rResult) *rResult = kRESULT_STATUS_READY;
    return 1;
}

int CHC_API CHCUSB_statusAll(void* handle, const unsigned char* idArray, unsigned short* rResultArray) {
    LOG_TRACE("CHCUSB_statusAll() -> kRESULT_STATUS_READY (0)");
    if (rResultArray) rResultArray[0] = kRESULT_STATUS_READY;
    return 1;
}

int CHC_API CHCUSB_setPrintStandby(void* handle, unsigned short position, unsigned short* rResult) {
    LOG_API("CHCUSB_setPrintStandby(position=" + std::to_string(position) + ")");
    if (!handle) return 0;
    auto* ctx = (PrinterContext*)handle;
    std::lock_guard<std::mutex> lock(ctx->mutex);
    ctx->currentStandbyPos = position;
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_getCardRfidTID(void* handle, unsigned char* rCardTID, unsigned short* rResult) {
    LOG_API("CHCUSB_getCardRfidTID() -> returning configured 12-byte CardTID & kRESULT_CARDRFID_ReadA (2405)");
    if (rCardTID) {
        memcpy(rCardTID, GetConfig().cardTID, 12);
    }
    // Return 2405 (kRESULT_CARDRFID_ReadA) which verifies genuine blank card stock
    if (rResult) *rResult = kRESULT_CARDRFID_ReadA;
    return 1;
}

int CHC_API CHCUSB_commCardRfidReader(void* handle, const unsigned char* sendData, unsigned char* rRecvData, unsigned int sendSize, unsigned int* rRecvSize, unsigned short* rResult) {
    unsigned int cmdByte = (sendData && sendSize >= 1) ? sendData[0] : 0;
    LOG_API("CHCUSB_commCardRfidReader(cmd=0x" +
            ([](unsigned int v){ char b[8]; snprintf(b, sizeof(b), "%02X", v); return std::string(b); })(cmdByte) +
            ", sendSize=" + std::to_string(sendSize) + ")");
    if (rRecvData && sendData) {
        memset(rRecvData, 0, 32);
        // Command 0x42 (66) queries reader firmware version
        if (sendSize >= 1 && sendData[0] == 66) {
            rRecvData[0] = 66;
            rRecvData[3] = 0xFF; // High firmware version (0xFF >= any file version) prevents unnecessary RFID firmware reflash
        }
    }
    if (rRecvSize) *rRecvSize = 32;
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_updateCardRfidReader(void* handle, const unsigned char* data, unsigned int size, unsigned short* rResult) {
    LOG_API("CHCUSB_updateCardRfidReader(size=" + std::to_string(size) + ") -> stubbed OK");
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_setPrinterInfo(void* handle, unsigned short tagNumber, unsigned char* rBuffer, unsigned int* rLen, unsigned short* rResult) {
    unsigned int lenVal = rLen ? *rLen : 0;
    LOG_API("CHCUSB_setPrinterInfo(tag=" + std::string(GetTagName(tagNumber)) + ", len=" + std::to_string(lenVal) + ")");
    if (handle && rBuffer && rLen) {
        auto* ctx = (PrinterContext*)handle;
        std::lock_guard<std::mutex> lock(ctx->mutex);
        if (tagNumber == kPINFTAG_PAPER && *rLen > 0) {
            memcpy(ctx->paperInfo, rBuffer, (std::min)((size_t)*rLen, sizeof(ctx->paperInfo)));
        } else if (tagNumber == kPINFTAG_PRINTMODE && *rLen > 0) {
            memcpy(ctx->polishMode, rBuffer, (std::min)((size_t)*rLen, sizeof(ctx->polishMode)));
            LOG_DEBUG("  Set polish/holo mode byte[0]=" + std::to_string(ctx->polishMode[0]));
        }
    }
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_getPrinterInfo(void* handle, unsigned short tagNumber, unsigned char* rBuffer, unsigned int* rLen) {
    unsigned int lenVal = rLen ? *rLen : 0;
    LOG_API("CHCUSB_getPrinterInfo(tag=" + std::string(GetTagName(tagNumber)) + ", rLen=" + std::to_string(lenVal) + ")");
    if (!rBuffer || !rLen) return 0;

    switch (tagNumber) {
        case kPINFTAG_PAPER: { // Tag 0: Paper & Holo Ribbon Status (PrintLib.procHoloRibbonStatus checks rBuffer[1] != 0)
            memset(rBuffer, 0, *rLen);
            if (*rLen >= 1) rBuffer[0] = 2; // Media code 2 (standard CR80 card stock)
            if (*rLen >= 2) rBuffer[1] = 1; // Non-zero = Holo ribbon present (prevents Error 2308 / IsEmptyHoloRibbon)
            return 1;
        }
        case kPINFTAG_TONER: { // Tag 1: Ink ribbon remaining percentage
            memset(rBuffer, 100, *rLen);
            return 1;
        }
        case kPINFTAG_SERIALINFO: { // Tag 26: Serial number (8 bytes)
            const std::string& sn = GetConfig().serialNumber;
            memset(rBuffer, 0, *rLen);
            memcpy(rBuffer, sn.c_str(), (std::min)((size_t)*rLen, sn.length()));
            return 1;
        }
        case kPINFTAG_ENGID: { // Tag 3: Firmware Table (153 bytes)
            memset(rBuffer, 0, *rLen);
            if (*rLen >= 153) {
                // Boot firmware at offset 35-36 (0x01, 0x00)
                rBuffer[35] = 0x01;
                rBuffer[36] = 0x00;
                // Main firmware at offset 73-74 (0x01, 0x00)
                rBuffer[73] = 0x01;
                rBuffer[74] = 0x00;
                // Parameter table at offset 111-112 (0x01, 0x00)
                rBuffer[111] = 0x01;
                rBuffer[112] = 0x00;
            }
            return 1;
        }
        case kPINFTAG_PRINTCNT2: { // Tag 5: Counters (40 bytes)
            memset(rBuffer, 0, *rLen);
            if (*rLen >= 40 && handle) {
                auto* ctx = (PrinterContext*)handle;
                std::lock_guard<std::mutex> lock(ctx->mutex);
                // printCounter0 at 0..3, printCounter1 at 4..7
                uint32_t cnt = ctx->printCounter;
                memcpy(rBuffer + 0, &cnt, 4);
                memcpy(rBuffer + 4, &cnt, 4);
            }
            return 1;
        }
        default:
            memset(rBuffer, 0, *rLen);
            return 1;
    }
}

int CHC_API CHCUSB_imageformat(void* handle, unsigned short format, unsigned short ncomp, unsigned short depth, unsigned short width, unsigned short height, const unsigned char* inputImage, unsigned short* rResult) {
    LOG_API("CHCUSB_imageformat(format=" + std::to_string(format) +
            ", ncomp=" + std::to_string(ncomp) +
            ", depth=" + std::to_string(depth) +
            ", width=" + std::to_string(width) +
            ", height=" + std::to_string(height) + ")");
    if (!handle) return 0;
    auto* ctx = (PrinterContext*)handle;
    std::lock_guard<std::mutex> lock(ctx->mutex);
    ctx->format = format;
    ctx->ncomp = (ncomp > 0) ? ncomp : 3;
    ctx->depth = depth;
    ctx->width = (width > 0) ? width : kWIDTH;
    ctx->height = (height > 0) ? height : kHEIGHT;
    ctx->colorBuffer.assign((size_t)ctx->width * ctx->height * ctx->ncomp, 0);
    ctx->holoBuffer.assign((size_t)ctx->width * ctx->height, 0);
    ctx->hasHolo = false;
    if (rResult) *rResult = 0;
    return 1;
}

void CHC_API CHCUSB_setmtf(void* handle, const int* mtf) {
    LOG_API("CHCUSB_setmtf()");
}

void CHC_API CHCUSB_makeGamma(void* handle, unsigned short k, unsigned char* intoneR, unsigned char* intoneG, unsigned char* intoneB) {
    LOG_API("CHCUSB_makeGamma(k=" + std::to_string(k) + ")");
    if (intoneR && intoneG && intoneB) {
        for (int i = 0; i < 256; i++) {
            intoneR[i] = (unsigned char)i;
            intoneG[i] = (unsigned char)i;
            intoneB[i] = (unsigned char)i;
        }
    }
}

int CHC_API CHCUSB_getGamma(void* handle, const char* filename, unsigned char* r, unsigned char* g, unsigned char* b, unsigned short* rResult) {
    LOG_API("CHCUSB_getGamma(filename='" + std::string(filename ? filename : "") + "')");
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_getMtf(void* handle, const char* filename, int64_t* mtf, unsigned short* rResult) {
    LOG_API("CHCUSB_getMtf(filename='" + std::string(filename ? filename : "") + "')");
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_setIcctable(void* handle, unsigned short renderingIntents, const unsigned char* intoneR, const unsigned char* intoneG, const unsigned char* intoneB, const unsigned char* outtoneR, const unsigned char* outtoneG, const unsigned char* outtoneB, unsigned short* rResult) {
    LOG_API("CHCUSB_setIcctable(renderingIntents=" + std::to_string(renderingIntents) + ")");
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_setIcctableProfile(void* handle, const char* inProfileName, const char* outProfileName, unsigned short renderingIntents, unsigned short* rResult) {
    LOG_API("CHCUSB_setIcctableProfile(in='" + std::string(inProfileName ? inProfileName : "") +
            "', out='" + std::string(outProfileName ? outProfileName : "") + "')");
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_copies(void* handle, unsigned short copies, unsigned short* rResult) {
    LOG_API("CHCUSB_copies(copies=" + std::to_string(copies) + ")");
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_cancelCopies(void* handle, unsigned short pageId, unsigned short* rResult) {
    LOG_API("CHCUSB_cancelCopies(pageId=" + std::to_string(pageId) + ")");
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_startpage(void* handle, unsigned short postCardState, unsigned short* pageId, unsigned short* rResult) {
    if (!handle) return 0;
    auto* ctx = (PrinterContext*)handle;
    std::lock_guard<std::mutex> lock(ctx->mutex);
    ctx->isPageOpen = true;
    ctx->currentPageId++;
    if (ctx->currentPageId == 0) ctx->currentPageId = 1;
    if (pageId) *pageId = ctx->currentPageId;
    if (rResult) *rResult = 0;
    LOG_API("CHCUSB_startpage(postCardState=" + std::to_string(postCardState) +
            ") -> assigned pageId=" + std::to_string(ctx->currentPageId));
    return 1;
}

int CHC_API CHCUSB_write(void* handle, const unsigned char* data, unsigned int offset, unsigned int* writeSize, unsigned short* rResult) {
    if (!handle || !data || !writeSize) return 0;
    auto* ctx = (PrinterContext*)handle;
    std::lock_guard<std::mutex> lock(ctx->mutex);

    unsigned int bytesToWrite = *writeSize;
    LOG_API("CHCUSB_write(offset=" + std::to_string(offset) + ", bytes=" + std::to_string(bytesToWrite) + ")");
    if ((size_t)offset + bytesToWrite > ctx->colorBuffer.size()) {
        ctx->colorBuffer.resize((size_t)offset + bytesToWrite, 0);
    }
    memcpy(ctx->colorBuffer.data() + offset, data, bytesToWrite);

    // CRITICAL: Must return the number of bytes written in *writeSize
    *writeSize = bytesToWrite;
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_writeLaminate(void* handle, const unsigned char* data, unsigned int offset, unsigned int* writeSize, unsigned short* rResult) {
    unsigned int bytes = writeSize ? *writeSize : 0;
    LOG_API("CHCUSB_writeLaminate(offset=" + std::to_string(offset) + ", bytes=" + std::to_string(bytes) + ")");
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_writeHolo(void* handle, const unsigned char* data, unsigned int offset, unsigned int* writeSize, unsigned short* rResult) {
    if (!handle || !data || !writeSize) return 0;
    auto* ctx = (PrinterContext*)handle;
    std::lock_guard<std::mutex> lock(ctx->mutex);

    unsigned int bytesToWrite = *writeSize;
    LOG_API("CHCUSB_writeHolo(offset=" + std::to_string(offset) + ", bytes=" + std::to_string(bytesToWrite) + ")");
    if ((size_t)offset + bytesToWrite > ctx->holoBuffer.size()) {
        ctx->holoBuffer.resize((size_t)offset + bytesToWrite, 0);
    }
    memcpy(ctx->holoBuffer.data() + offset, data, bytesToWrite);
    ctx->hasHolo = true;

    // CRITICAL: Must return the number of bytes written in *writeSize
    *writeSize = bytesToWrite;
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_endpage(void* handle, unsigned short* rResult) {
    LOG_API("CHCUSB_endpage()");
    if (!handle) return 0;
    auto* ctx = (PrinterContext*)handle;
    std::lock_guard<std::mutex> lock(ctx->mutex);

    ctx->isPageOpen = false;
    ctx->isPrinting = true;
    ctx->pageEndTime = std::chrono::steady_clock::now();
    ctx->printCounter++;
    ctx->headCounter++;
    ctx->cutterCounter++;

    int w = (ctx->width > 0) ? ctx->width : kWIDTH;
    int h = (ctx->height > 0) ? ctx->height : kHEIGHT;
    size_t requiredColorBytes = (size_t)w * h * 3;
    size_t requiredHoloBytes  = (size_t)w * h;

    // Copy buffers for asynchronous output and guarantee safe buffer dimensions
    std::vector<uint8_t> colorCopy = ctx->colorBuffer;
    if (colorCopy.size() < requiredColorBytes) {
        LOG_WARN("CHCUSB_endpage: colorBuffer size (" + std::to_string(colorCopy.size()) +
                 ") was smaller than required (" + std::to_string(requiredColorBytes) + "); zero-padding buffer.");
        colorCopy.resize(requiredColorBytes, 0);
    }

    std::vector<uint8_t> holoCopy = ctx->holoBuffer;
    bool hasHolo = ctx->hasHolo;
    if (hasHolo && holoCopy.size() < requiredHoloBytes) {
        holoCopy.resize(requiredHoloBytes, 0);
    }

    uint32_t jobNumber = ctx->printCounter;

    // Dispatch output in an exception-safe background task
    std::thread([colorCopy = std::move(colorCopy), holoCopy = std::move(holoCopy), hasHolo, w, h, jobNumber]() {
        try {
            const AppConfig& cfg = GetConfig();
            std::string timestamp = GetTimestampString();
            std::string jobName = "Card_" + timestamp + "_" + std::to_string(jobNumber);

            LOG_INFO("Processing print job '" + jobName + "' (" + std::to_string(w) + "x" + std::to_string(h) +
                     ", hasHolo=" + std::to_string(hasHolo) + ")");

            bool wantsPrinter = (cfg.outputMode == OUTPUT_PRINTER_ONLY || cfg.outputMode == OUTPUT_BOTH);
            bool wantsFile    = (cfg.outputMode == OUTPUT_FILE_ONLY    || cfg.outputMode == OUTPUT_BOTH);

                std::string printerReason;
                bool physicalPrinterReady = false;
                if (wantsPrinter) {
                    physicalPrinterReady = IsPhysicalPrinterAvailable(cfg.printerName, &printerReason);
                    if (!physicalPrinterReady && (cfg.stubIfNoPrinter || cfg.forceStubMode)) {
                        LOG_STUB("No physical printer available (" + printerReason +
                                 "). Stub mode activated for job '" + jobName +
                                 "': saving card to pointed folder '" + cfg.outputDir + "' and skipping Win32 print.");
                        wantsFile = true;
                        wantsPrinter = false;
                    }
                } else {
                    LOG_STUB("OutputMode=0 (File Only): saving card '" + jobName +
                             "' directly to pointed folder '" + cfg.outputDir + "'.");
                }

                // Load card back buffer if enabled
                std::vector<uint8_t> backRgb;
                bool backLoaded = false;
                if (cfg.cardBack.enabled) {
                    std::string loadedBackPath;
                    backLoaded = LoadCardBackRgb(w, h, backRgb, &loadedBackPath);
                    if (backLoaded) {
                        LOG_INFO("Card back prepared for job '" + jobName + "' from: " + loadedBackPath);
                    } else {
                        LOG_WARN("Card back requested for job '" + jobName + "' but could not be loaded.");
                    }
                }

                // 1. File Export (when configured OR when stubbing due to no connected printer)
                if (wantsFile) {
                    if (!EnsureDirectoryExists(cfg.outputDir)) {
                        LOG_ERR("Failed to create or access OutputDir: '" + cfg.outputDir + "'");
                    } else {
                        // Apply bleed to export images if enabled
                        std::vector<uint8_t> frontExportRgb;
                        int exportW = w;
                        int exportH = h;
                        if (cfg.bleed.enabled && cfg.bleed.applyToFileExport) {
                            ApplyCardBleed(colorCopy.data(), w, h, frontExportRgb, exportW, exportH, cfg.bleed);
                        } else {
                            frontExportRgb = colorCopy;
                        }

                        std::vector<uint8_t> backExportRgb;
                        if (backLoaded) {
                            if (cfg.bleed.enabled && cfg.bleed.applyToFileExport) {
                                int bW = 0, bH = 0;
                                ApplyCardBleed(backRgb.data(), w, h, backExportRgb, bW, bH, cfg.bleed);
                            } else {
                                backExportRgb = backRgb;
                            }
                        }

                        std::vector<uint8_t> holoExportMask;
                        int holoW = w;
                        int holoH = h;
                        if (hasHolo) {
                            if (cfg.bleed.enabled && cfg.bleed.applyToFileExport) {
                                ApplyHoloMaskBleed(holoCopy.data(), w, h, holoExportMask, holoW, holoH, cfg.bleed);
                            } else {
                                holoExportMask = holoCopy;
                            }
                        }

                        std::string ext = (cfg.exportFormat == FORMAT_BMP) ? ".bmp" : ".png";
                        std::string mainFile = cfg.outputDir + "/" + jobName + ext;
                        ExportCardImage(mainFile, frontExportRgb.data(), exportW, exportH, cfg.exportFormat, cfg.fixColors, cfg.fileDPI);

                        if (backLoaded && cfg.cardBack.saveCardBack) {
                            std::string backFile = cfg.outputDir + "/" + jobName + "_back" + ext;
                            ExportCardImage(backFile, backExportRgb.data(), exportW, exportH, cfg.exportFormat, cfg.fixColors, cfg.fileDPI);
                        }

                        if (backLoaded && cfg.cardBack.saveCardSpread) {
                            std::string spreadFile = cfg.outputDir + "/" + jobName + "_spread" + ext;
                            ExportCardSpreadImage(spreadFile, frontExportRgb.data(), backExportRgb.data(), exportW, exportH, cfg.exportFormat, cfg.fixColors, cfg.fileDPI);
                        }

                        if (hasHolo && cfg.saveHoloMask) {
                            std::string holoFile = cfg.outputDir + "/" + jobName + "_holo" + ext;
                            ExportHoloImage(holoFile, holoExportMask.data(), holoW, holoH, cfg.exportFormat, cfg.fileDPI);
                        }

                        if (hasHolo && cfg.combineHolo) {
                            std::string compositeFile = cfg.outputDir + "/" + jobName + "_composite" + ext;
                            ExportHoloCompositeImage(compositeFile, frontExportRgb.data(), holoExportMask.data(), exportW, exportH, cfg.exportFormat, cfg.fixColors, cfg.fileDPI);
                        }
                    }
                }

                // 2. Win32 Physical Printer (only when a usable physical printer is present or StubIfNoPrinter=0)
                if (wantsPrinter) {
                    std::vector<uint8_t> frontPrintRgb;
                    int printW = w;
                    int printH = h;
                    if (cfg.bleed.enabled && cfg.bleed.applyToPhysicalPrint) {
                        ApplyCardBleed(colorCopy.data(), w, h, frontPrintRgb, printW, printH, cfg.bleed);
                    } else {
                        frontPrintRgb = colorCopy;
                    }

                    std::vector<uint8_t> backPrintRgb;
                    if (backLoaded) {
                        if (cfg.bleed.enabled && cfg.bleed.applyToPhysicalPrint) {
                            int bW = 0, bH = 0;
                            ApplyCardBleed(backRgb.data(), w, h, backPrintRgb, bW, bH, cfg.bleed);
                        } else {
                            backPrintRgb = backRgb;
                        }
                    }

                    PrintToWin32Printer(
                        frontPrintRgb.data(),
                        backLoaded ? backPrintRgb.data() : nullptr,
                        printW, printH,
                        cfg.printerName,
                        cfg.cardBack.printMode
                    );
                }
        } catch (const std::exception& ex) {
            LOG_ERR(std::string("Unhandled C++ exception in CHCUSB_endpage worker thread: ") + ex.what());
        } catch (...) {
            LOG_ERR("Unhandled unknown exception in CHCUSB_endpage worker thread");
        }
    }).detach();

    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_getPrintIDStatus(void* handle, unsigned short pageId, unsigned char* rBuffer, unsigned short* rResult) {
    if (!handle || !rBuffer) return 0;
    auto* ctx = (PrinterContext*)handle;
    std::lock_guard<std::mutex> lock(ctx->mutex);

    memset(rBuffer, 0, 8);
    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - ctx->pageEndTime).count();

    unsigned short statusWord = kRESULT_STATUS_PrinttingComplete; // 2212
    if (ctx->isPrinting && elapsedMs < GetConfig().simulateDelayMs) {
        statusWord = kRESULT_STATUS_Y_Printtng; // 2203
    } else {
        if (ctx->isPrinting) {
            LOG_API("CHCUSB_getPrintIDStatus(pageId=" + std::to_string(pageId) +
                    ") -> transitioned to kRESULT_STATUS_PrinttingComplete (2212)");
        }
        ctx->isPrinting = false;
        statusWord = kRESULT_STATUS_PrinttingComplete; // 2212 (0x08A4)
    }

    // CRITICAL: Word at offset 6 indicates status
    rBuffer[6] = (unsigned char)(statusWord & 0xFF);
    rBuffer[7] = (unsigned char)((statusWord >> 8) & 0xFF);

    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_getErrorStatus(void* handle, unsigned short* rBuffer) {
    LOG_TRACE("CHCUSB_getErrorStatus() -> 0 active errors");
    if (rBuffer) {
        memset(rBuffer, 0, sizeof(unsigned short) * 66);
    }
    return 0; // 0 indicates no active errors
}

int CHC_API CHCUSB_getErrorLog(void* handle, unsigned short index, unsigned char* rData, unsigned short* rResult) {
    LOG_API("CHCUSB_getErrorLog(index=" + std::to_string(index) + ") -> 0 records");
    if (rData) memset(rData, 0, 16);
    if (rResult) *rResult = 0;
    return 0; // 0 indicates no error log records
}

int CHC_API CHCUSB_resetPrinter(void* handle, unsigned short* rResult) {
    LOG_API("CHCUSB_resetPrinter()");
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_blinkLED(void* handle, unsigned short* rResult) {
    LOG_API("CHCUSB_blinkLED()");
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_testCardFeed(void* handle, unsigned short mode, unsigned short times, unsigned short* rResult) {
    LOG_API("CHCUSB_testCardFeed(mode=" + std::to_string(mode) + ", times=" + std::to_string(times) + ")");
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_exitCard(void* handle, unsigned short* rResult) {
    LOG_API("CHCUSB_exitCard()");
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_setPrinterToneCurve(void* handle, unsigned short type, unsigned short number, unsigned short data, unsigned short* rResult) {
    LOG_API("CHCUSB_setPrinterToneCurve(type=" + std::to_string(type) + ", number=" + std::to_string(number) + ")");
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_getPrinterToneCurve(void* handle, unsigned short type, unsigned short number, unsigned short* data, unsigned short* rResult) {
    LOG_API("CHCUSB_getPrinterToneCurve(type=" + std::to_string(type) + ", number=" + std::to_string(number) + ")");
    if (data) *data = 0;
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_MakeThread(void* handle, unsigned short maxCount) {
    LOG_API("CHCUSB_MakeThread(maxCount=" + std::to_string(maxCount) + ")");
    return 1;
}

int CHC_API CHCUSB_ReleaseThread(void* handle, unsigned short* rResult) {
    LOG_API("CHCUSB_ReleaseThread()");
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CHCUSB_AttachThreadCount(void* handle, unsigned short* rCount, unsigned short* rMaxCount) {
    if (rCount) *rCount = 0;
    if (rMaxCount) *rMaxCount = 0;
    return 1;
}

