#pragma once

#include <windows.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHC_API __stdcall

#pragma pack(push, 1)
typedef struct {
    uint8_t n0;
    uint8_t n1;
    uint8_t n2;
    uint8_t n3;
    uint8_t n4;
    uint8_t n5;
    uint8_t n6;
    uint8_t n7;
} SerialNumRec;
#pragma pack(pop)

// Status & Error Constants
#define kRESULT_STATUS_READY              0
#define kRESULT_STATUS_BUSY               2100
#define kRESULT_STATUS_Y_Printtng         2203
#define kRESULT_STATUS_M_Printtng         2204
#define kRESULT_STATUS_C_Printtng         2205
#define kRESULT_STATUS_Cutting            2208
#define kRESULT_STATUS_CardEjection       2209
#define kRESULT_STATUS_PrinttingComplete  2212
#define kRESULT_STATUS_Holo_Printing      2215
#define kRESULT_CARDRFID_ReadA            2405

// Tag Constants
#define kPINFTAG_PAPER                    0
#define kPINFTAG_TONER                    1
#define kPINFTAG_ENGID                    3
#define kPINFTAG_PRINTCNT                 4
#define kPINFTAG_PRINTCNT2                5
#define kPINFTAG_PRINTSTANDBY             8
#define kPINFTAG_PRINTSTAND               10
#define kPINFTAG_MEMORY                   15
#define kPINFTAG_PRINTMODE                20
#define kPINFTAG_CARDINFO                 25
#define kPINFTAG_SERIALINFO               26
#define kPINFTAG_TEMP                     30
#define kPINFTAG_ERRHISTORY               40
#define kPINFTAG_TONECURVE                50
#define kPINFTAG_M19_INFO                 85

// Image & Polish Constants
#define kWIDTH                            768
#define kHEIGHT                           1052
#define kCOLOR_BUFFER_SIZE                2423808
#define kHOLO_BUFFER_SIZE                 807936
#define kPOLISHTYPE_GLOSSY                18
#define kPOLISHTYPE_GLOSSY_HOLO           21

// CHCUSB API Declarations
void* CHC_API CHCUSB_init(const char* dllpath);
void  CHC_API CHCUSB_term(void* handle);
int   CHC_API CHCUSB_open(void* handle, const char* dllpath, unsigned short* rResult);
void  CHC_API CHCUSB_close(void* handle);
unsigned short CHC_API CHCUSB_listupPrinter(void* handle, unsigned char* rIdArray);
unsigned char  CHC_API CHCUSB_selectPrinter(void* handle, unsigned char printerId, unsigned short* rResult);
unsigned short CHC_API CHCUSB_listupPrinterSN(void* handle, SerialNumRec* rSerialArray);
void* CHC_API CHCUSB_selectPrinterSN(void* handle, SerialNumRec* printerSN, unsigned short* rResult);
int   CHC_API CHCUSB_status(void* handle, unsigned short* rResult);
int   CHC_API CHCUSB_statusAll(void* handle, const unsigned char* idArray, unsigned short* rResultArray);
int   CHC_API CHCUSB_setPrintStandby(void* handle, unsigned short position, unsigned short* rResult);
int   CHC_API CHCUSB_getCardRfidTID(void* handle, unsigned char* rCardTID, unsigned short* rResult);
int   CHC_API CHCUSB_commCardRfidReader(void* handle, const unsigned char* sendData, unsigned char* rRecvData, unsigned int sendSize, unsigned int* rRecvSize, unsigned short* rResult);
int   CHC_API CHCUSB_updateCardRfidReader(void* handle, const unsigned char* data, unsigned int size, unsigned short* rResult);
int   CHC_API CHCUSB_setPrinterInfo(void* handle, unsigned short tagNumber, unsigned char* rBuffer, unsigned int* rLen, unsigned short* rResult);
int   CHC_API CHCUSB_getPrinterInfo(void* handle, unsigned short tagNumber, unsigned char* rBuffer, unsigned int* rLen);
int   CHC_API CHCUSB_imageformat(void* handle, unsigned short format, unsigned short ncomp, unsigned short depth, unsigned short width, unsigned short height, const unsigned char* inputImage, unsigned short* rResult);
void  CHC_API CHCUSB_setmtf(void* handle, const int* mtf);
void  CHC_API CHCUSB_makeGamma(void* handle, unsigned short k, unsigned char* intoneR, unsigned char* intoneG, unsigned char* intoneB);
int   CHC_API CHCUSB_getGamma(void* handle, const char* filename, unsigned char* r, unsigned char* g, unsigned char* b, unsigned short* rResult);
int   CHC_API CHCUSB_getMtf(void* handle, const char* filename, int64_t* mtf, unsigned short* rResult);
int   CHC_API CHCUSB_setIcctable(void* handle, unsigned short renderingIntents, const unsigned char* intoneR, const unsigned char* intoneG, const unsigned char* intoneB, const unsigned char* outtoneR, const unsigned char* outtoneG, const unsigned char* outtoneB, unsigned short* rResult);
int   CHC_API CHCUSB_setIcctableProfile(void* handle, const char* inProfileName, const char* outProfileName, unsigned short renderingIntents, unsigned short* rResult);
int   CHC_API CHCUSB_copies(void* handle, unsigned short copies, unsigned short* rResult);
int   CHC_API CHCUSB_cancelCopies(void* handle, unsigned short pageId, unsigned short* rResult);
int   CHC_API CHCUSB_startpage(void* handle, unsigned short postCardState, unsigned short* pageId, unsigned short* rResult);
int   CHC_API CHCUSB_write(void* handle, const unsigned char* data, unsigned int offset, unsigned int* writeSize, unsigned short* rResult);
int   CHC_API CHCUSB_writeLaminate(void* handle, const unsigned char* data, unsigned int offset, unsigned int* writeSize, unsigned short* rResult);
int   CHC_API CHCUSB_writeHolo(void* handle, const unsigned char* data, unsigned int offset, unsigned int* writeSize, unsigned short* rResult);
int   CHC_API CHCUSB_endpage(void* handle, unsigned short* rResult);
int   CHC_API CHCUSB_getPrintIDStatus(void* handle, unsigned short pageId, unsigned char* rBuffer, unsigned short* rResult);
int   CHC_API CHCUSB_getErrorStatus(void* handle, unsigned short* rBuffer);
int   CHC_API CHCUSB_getErrorLog(void* handle, unsigned short index, unsigned char* rData, unsigned short* rResult);
int   CHC_API CHCUSB_resetPrinter(void* handle, unsigned short* rResult);
int   CHC_API CHCUSB_blinkLED(void* handle, unsigned short* rResult);
int   CHC_API CHCUSB_testCardFeed(void* handle, unsigned short mode, unsigned short times, unsigned short* rResult);
int   CHC_API CHCUSB_exitCard(void* handle, unsigned short* rResult);
int   CHC_API CHCUSB_setPrinterToneCurve(void* handle, unsigned short type, unsigned short number, unsigned short data, unsigned short* rResult);
int   CHC_API CHCUSB_getPrinterToneCurve(void* handle, unsigned short type, unsigned short number, unsigned short* data, unsigned short* rResult);
int   CHC_API CHCUSB_MakeThread(void* handle, unsigned short maxCount);
int   CHC_API CHCUSB_ReleaseThread(void* handle, unsigned short* rResult);
int   CHC_API CHCUSB_AttachThreadCount(void* handle, unsigned short* rCount, unsigned short* rMaxCount);

// CFW Firmware API Declarations
void* CHC_API CFW_init(const char* dllpath);
void  CHC_API CFW_term(void* handle);
int   CHC_API CFW_open(void* handle, const char* dllpath, unsigned short* rResult);
void  CHC_API CFW_close(void* handle);
unsigned short CHC_API CFW_listupPrinter(void* handle, unsigned char* rIdArray);
unsigned short CHC_API CFW_listupPrinterSN(void* handle, SerialNumRec* rSerialArray);
unsigned char  CHC_API CFW_selectPrinter(void* handle, unsigned char printerId, unsigned short* rResult);
void* CHC_API CFW_selectPrinterSN(void* handle, SerialNumRec* printerSN, unsigned short* rResult);
int   CHC_API CFW_getPrinterInfo(void* handle, unsigned short tagNumber, unsigned char* rBuffer, unsigned int* rLen);
int   CHC_API CFW_status(void* handle, unsigned short* rResult);
int   CHC_API CFW_statusAll(void* handle, unsigned char* rIdArray, unsigned short* rResultArray);
int   CHC_API CFW_resetPrinter(void* handle, unsigned short* rResult);
int   CHC_API CFW_updateFirmware(void* handle, unsigned char update, const char* filename, unsigned short* rResult);
int   CHC_API CFW_getFirmwareInfo(void* handle, unsigned char update, const char* filename, unsigned char* rBuffer, unsigned int* rLen, unsigned short* rResult);

#ifdef __cplusplus
}
#endif
