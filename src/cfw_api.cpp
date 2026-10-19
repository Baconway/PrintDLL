#include "../include/chcusb.h"
#include "../include/logger.h"
#include "../include/config.h"
#include <cstring>
#include <algorithm>

static int g_cfwDummy = 1;

void* CHC_API CFW_init(const char* dllpath) {
    LOG_API("CFW_init(dllpath='" + std::string(dllpath ? dllpath : "<null>") + "')");
    return &g_cfwDummy;
}

void CHC_API CFW_term(void* handle) {
    LOG_API("CFW_term()");
}

int CHC_API CFW_open(void* handle, const char* dllpath, unsigned short* rResult) {
    LOG_API("CFW_open(dllpath='" + std::string(dllpath ? dllpath : "<null>") + "')");
    if (rResult) *rResult = 0;
    return 1;
}

void CHC_API CFW_close(void* handle) {
    LOG_API("CFW_close()");
}

unsigned short CHC_API CFW_listupPrinter(void* handle, unsigned char* rIdArray) {
    LOG_API("CFW_listupPrinter() -> 1 printer");
    if (rIdArray) rIdArray[0] = 0;
    return 1;
}

unsigned short CHC_API CFW_listupPrinterSN(void* handle, SerialNumRec* rSerialArray) {
    const std::string& sn = GetConfig().serialNumber;
    LOG_API("CFW_listupPrinterSN() -> returning SN='" + sn + "'");
    if (rSerialArray) {
        memset(&rSerialArray[0], 0, sizeof(SerialNumRec));
        for (int i = 0; i < 8 && i < (int)sn.length(); i++) {
            ((uint8_t*)&rSerialArray[0])[i] = (uint8_t)sn[i];
        }
    }
    return 1;
}

unsigned char CHC_API CFW_selectPrinter(void* handle, unsigned char printerId, unsigned short* rResult) {
    LOG_API("CFW_selectPrinter(printerId=" + std::to_string(printerId) + ")");
    if (rResult) *rResult = 0;
    return 0;
}

void* CHC_API CFW_selectPrinterSN(void* handle, SerialNumRec* printerSN, unsigned short* rResult) {
    LOG_API("CFW_selectPrinterSN()");
    if (rResult) *rResult = 0;
    return handle ? handle : &g_cfwDummy;
}

int CHC_API CFW_getPrinterInfo(void* handle, unsigned short tagNumber, unsigned char* rBuffer, unsigned int* rLen) {
    unsigned int lenVal = rLen ? *rLen : 0;
    LOG_API("CFW_getPrinterInfo(tag=" + std::to_string(tagNumber) + ", rLen=" + std::to_string(lenVal) + ")");
    if (!rBuffer || !rLen) return 0;
    memset(rBuffer, 0, *rLen);

    if (tagNumber == kPINFTAG_PAPER) {
        if (*rLen >= 1) rBuffer[0] = 2; // Media code 2
        if (*rLen >= 2) rBuffer[1] = 1; // Non-zero = Holo ribbon present
        return 1;
    }
    if (tagNumber == kPINFTAG_SERIALINFO) {
        const std::string& sn = GetConfig().serialNumber;
        memcpy(rBuffer, sn.c_str(), (std::min)((size_t)*rLen, sn.length()));
        return 1;
    }
    if (tagNumber == kPINFTAG_ENGID && *rLen >= 153) {
        // Boot firmware version at offset 35, 36
        rBuffer[35] = 0x01;
        rBuffer[36] = 0x00;
        // Main firmware version at offset 39 + 34 = 73, 74
        rBuffer[73] = 0x01;
        rBuffer[74] = 0x00;
        // Param firmware version at offset 77 + 34 = 111, 112
        rBuffer[111] = 0x01;
        rBuffer[112] = 0x00;
        return 1;
    }
    return 1;
}

int CHC_API CFW_status(void* handle, unsigned short* rResult) {
    LOG_TRACE("CFW_status() -> 0 (Ready)");
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CFW_statusAll(void* handle, unsigned char* rIdArray, unsigned short* rResultArray) {
    LOG_TRACE("CFW_statusAll() -> 0 (Ready)");
    if (rResultArray) rResultArray[0] = 0;
    return 1;
}

int CHC_API CFW_resetPrinter(void* handle, unsigned short* rResult) {
    LOG_API("CFW_resetPrinter()");
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CFW_updateFirmware(void* handle, unsigned char update, const char* filename, unsigned short* rResult) {
    LOG_API("CFW_updateFirmware(update=" + std::to_string(update) + ", file='" + std::string(filename ? filename : "") + "') -> stubbed OK");
    if (rResult) *rResult = 0;
    return 1;
}

int CHC_API CFW_getFirmwareInfo(void* handle, unsigned char update, const char* filename, unsigned char* rBuffer, unsigned int* rLen, unsigned short* rResult) {
    LOG_API("CFW_getFirmwareInfo(update=" + std::to_string(update) + ", file='" + std::string(filename ? filename : "") + "')");
    if (rBuffer && rLen && *rLen >= 38) {
        memset(rBuffer, 0, *rLen);
        rBuffer[34] = 0x01;
        rBuffer[35] = 0x00;
    }
    if (rResult) *rResult = 0;
    return 1;
}

