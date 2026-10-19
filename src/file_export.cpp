#include "../include/file_export.h"
#include "../include/logger.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

static bool g_gdiplusInitialized = false;
static ULONG_PTR g_gdiplusToken = 0;

static void EnsureGdiplusInitialized() {
    if (!g_gdiplusInitialized) {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
        g_gdiplusInitialized = true;
    }
}

static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    std::vector<uint8_t> memory(size);
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)memory.data();
    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            return j;
        }
    }
    return -1;
}

bool EnsureDirectoryExists(const std::string& dirPath) {
    if (dirPath.empty()) return false;
    DWORD attr = GetFileAttributesA(dirPath.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }
    size_t pos = 0;
    while ((pos = dirPath.find_first_of("\\/", pos)) != std::string::npos) {
        std::string subDir = dirPath.substr(0, pos);
        if (!subDir.empty() && subDir.back() != ':' && subDir != "." && subDir != "..") {
            CreateDirectoryA(subDir.c_str(), NULL);
        }
        pos++;
    }
    return CreateDirectoryA(dirPath.c_str(), NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

std::string GetTimestampString() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return std::string(buf);
}

bool ExportCardImage(const std::string& filepath, const uint8_t* rgbBuffer, int width, int height, ExportFormat format, bool fixColors, int fileDpi) {
    if (!rgbBuffer || width <= 0 || height <= 0) return false;
    EnsureGdiplusInitialized();

    const WCHAR* mimeType = (format == FORMAT_BMP) ? L"image/bmp" : L"image/png";
    CLSID encoderClsid;
    if (GetEncoderClsid(mimeType, &encoderClsid) < 0) {
        LOG_ERR("Failed to get encoder CLSID for format");
        return false;
    }

    // Windows GDI+ Bitmap scanlines MUST be DWORD aligned (multiples of 4 bytes)
    int stride = ((width * 3 + 3) / 4) * 4;
    std::vector<uint8_t> processedBuffer;
    const uint8_t* sourceToBitmap = rgbBuffer;

    if (fixColors || stride != width * 3) {
        // Sega Card Maker Unity engine outputs RGB24 (Byte 0=R, 1=G, 2=B).
        // Windows GDI+ PixelFormat24bppRGB expects little-endian BGR in memory (Byte 0=B, 1=G, 2=R).
        // Swapping R and B aligns the color channels correctly so skin and clothing render with true natural colors.
        processedBuffer.assign(stride * height, 0);
        for (int y = 0; y < height; y++) {
            const uint8_t* srcRow = rgbBuffer + y * width * 3;
            uint8_t* dstRow = processedBuffer.data() + y * stride;
            for (int x = 0; x < width; x++) {
                if (fixColors) {
                    dstRow[x * 3 + 0] = srcRow[x * 3 + 2]; // Blue
                    dstRow[x * 3 + 1] = srcRow[x * 3 + 1]; // Green
                    dstRow[x * 3 + 2] = srcRow[x * 3 + 0]; // Red
                } else {
                    dstRow[x * 3 + 0] = srcRow[x * 3 + 0];
                    dstRow[x * 3 + 1] = srcRow[x * 3 + 1];
                    dstRow[x * 3 + 2] = srcRow[x * 3 + 2];
                }
            }
        }
        sourceToBitmap = processedBuffer.data();
    }

    Bitmap bitmap(width, height, stride, PixelFormat24bppRGB, (BYTE*)sourceToBitmap);
    if (fileDpi > 0) {
        bitmap.SetResolution((REAL)fileDpi, (REAL)fileDpi);
    }

    wchar_t wFilepath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, filepath.c_str(), -1, wFilepath, MAX_PATH);

    Status status = bitmap.Save(wFilepath, &encoderClsid, NULL);
    if (status != Ok) {
        LOG_ERR("GDI+ failed to save image to: " + filepath);
        return false;
    }

    std::string dpiStr = (fileDpi > 0) ? (", DPI=" + std::to_string(fileDpi)) : "";
    LOG_INFO("Saved card image (" + std::string(format == FORMAT_BMP ? "BMP" : "PNG") + 
             ", FixColors=" + std::to_string(fixColors) + dpiStr + ") to: " + filepath);
    return true;
}

bool ExportCardSpreadImage(const std::string& filepath, const uint8_t* frontRgb, const uint8_t* backRgb, int width, int height, ExportFormat format, bool fixColors, int fileDpi) {
    if (!frontRgb || !backRgb || width <= 0 || height <= 0) return false;
    EnsureGdiplusInitialized();

    const WCHAR* mimeType = (format == FORMAT_BMP) ? L"image/bmp" : L"image/png";
    CLSID encoderClsid;
    if (GetEncoderClsid(mimeType, &encoderClsid) < 0) {
        LOG_ERR("Failed to get encoder CLSID for spread image format");
        return false;
    }

    int spreadWidth = width * 2;
    int stride = ((spreadWidth * 3 + 3) / 4) * 4;
    std::vector<uint8_t> spreadBuffer((size_t)stride * height, 0);

    for (int y = 0; y < height; y++) {
        const uint8_t* srcFrontRow = frontRgb + (size_t)y * width * 3;
        const uint8_t* srcBackRow  = backRgb  + (size_t)y * width * 3;
        uint8_t* dstRow = spreadBuffer.data() + (size_t)y * stride;

        // Left half: Front
        for (int x = 0; x < width; x++) {
            if (fixColors) {
                dstRow[x * 3 + 0] = srcFrontRow[x * 3 + 2]; // B
                dstRow[x * 3 + 1] = srcFrontRow[x * 3 + 1]; // G
                dstRow[x * 3 + 2] = srcFrontRow[x * 3 + 0]; // R
            } else {
                dstRow[x * 3 + 0] = srcFrontRow[x * 3 + 0];
                dstRow[x * 3 + 1] = srcFrontRow[x * 3 + 1];
                dstRow[x * 3 + 2] = srcFrontRow[x * 3 + 2];
            }
        }

        // Right half: Back
        int offset = width * 3;
        for (int x = 0; x < width; x++) {
            if (fixColors) {
                dstRow[offset + x * 3 + 0] = srcBackRow[x * 3 + 2]; // B
                dstRow[offset + x * 3 + 1] = srcBackRow[x * 3 + 1]; // G
                dstRow[offset + x * 3 + 2] = srcBackRow[x * 3 + 0]; // R
            } else {
                dstRow[offset + x * 3 + 0] = srcBackRow[x * 3 + 0];
                dstRow[offset + x * 3 + 1] = srcBackRow[x * 3 + 1];
                dstRow[offset + x * 3 + 2] = srcBackRow[x * 3 + 2];
            }
        }
    }

    Bitmap bitmap(spreadWidth, height, stride, PixelFormat24bppRGB, (BYTE*)spreadBuffer.data());
    if (fileDpi > 0) {
        bitmap.SetResolution((REAL)fileDpi, (REAL)fileDpi);
    }

    wchar_t wFilepath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, filepath.c_str(), -1, wFilepath, MAX_PATH);

    Status status = bitmap.Save(wFilepath, &encoderClsid, NULL);
    if (status != Ok) {
        LOG_ERR("GDI+ failed to save spread image to: " + filepath);
        return false;
    }

    std::string dpiStr = (fileDpi > 0) ? (", DPI=" + std::to_string(fileDpi)) : "";
    LOG_INFO("Saved card spread image (" + std::to_string(spreadWidth) + "x" + std::to_string(height) + " " +
             std::string(format == FORMAT_BMP ? "BMP" : "PNG") +
             ", FixColors=" + std::to_string(fixColors) + dpiStr + ") to: " + filepath);
    return true;
}

bool ExportHoloImage(const std::string& filepath, const uint8_t* grayBuffer, int width, int height, ExportFormat format, int fileDpi) {
    if (!grayBuffer || width <= 0 || height <= 0) return false;
    EnsureGdiplusInitialized();

    const WCHAR* mimeType = (format == FORMAT_BMP) ? L"image/bmp" : L"image/png";
    CLSID encoderClsid;
    if (GetEncoderClsid(mimeType, &encoderClsid) < 0) {
        LOG_ERR("Failed to get encoder CLSID");
        return false;
    }

    int stride = ((width * 3 + 3) / 4) * 4;
    std::vector<uint8_t> rgb(stride * height, 0);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t val = grayBuffer[y * width + x];
            int dstIdx = y * stride + x * 3;
            rgb[dstIdx + 0] = val;
            rgb[dstIdx + 1] = val;
            rgb[dstIdx + 2] = val;
        }
    }

    Bitmap bitmap(width, height, stride, PixelFormat24bppRGB, rgb.data());
    if (fileDpi > 0) {
        bitmap.SetResolution((REAL)fileDpi, (REAL)fileDpi);
    }

    wchar_t wFilepath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, filepath.c_str(), -1, wFilepath, MAX_PATH);

    Status status = bitmap.Save(wFilepath, &encoderClsid, NULL);
    if (status != Ok) {
        LOG_ERR("GDI+ failed to save holo image to: " + filepath);
        return false;
    }

    std::string dpiStr = (fileDpi > 0) ? (", DPI=" + std::to_string(fileDpi)) : "";
    LOG_INFO("Saved holo mask (" + std::string(format == FORMAT_BMP ? "BMP" : "PNG") + dpiStr + ") to: " + filepath);
    return true;
}

bool ExportHoloCompositeImage(const std::string& filepath, const uint8_t* rgbBuffer, const uint8_t* grayBuffer, int width, int height, ExportFormat format, bool fixColors, int fileDpi) {
    if (!rgbBuffer || !grayBuffer || width <= 0 || height <= 0) return false;
    EnsureGdiplusInitialized();

    const WCHAR* mimeType = (format == FORMAT_BMP) ? L"image/bmp" : L"image/png";
    CLSID encoderClsid;
    if (GetEncoderClsid(mimeType, &encoderClsid) < 0) {
        LOG_ERR("Failed to get encoder CLSID");
        return false;
    }

    const double PI = 3.14159265358979323846;
    int stride = ((width * 3 + 3) / 4) * 4;
    std::vector<uint8_t> composite(stride * height, 0);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            int rgbIdx = idx * 3;
            int dstIdx = y * stride + x * 3;
            
            // Raw Unity input is [R, G, B]
            uint8_t r = rgbBuffer[rgbIdx + 0];
            uint8_t g = rgbBuffer[rgbIdx + 1];
            uint8_t b = rgbBuffer[rgbIdx + 2];
            uint8_t mask = grayBuffer[idx];

            double outR = (double)r;
            double outG = (double)g;
            double outB = (double)b;

            if (mask > 0) {
                // Iridescent holographic sheen across card diagonal
                double angle = ((double)x * 1.2 + (double)y * 0.8) / 120.0 * PI;
                
                // Spectral foil colors
                double specR = 128.0 + 127.0 * std::sin(angle);
                double specG = 128.0 + 127.0 * std::sin(angle + 2.0 * PI / 3.0);
                double specB = 128.0 + 127.0 * std::sin(angle + 4.0 * PI / 3.0);
                
                // Mask opacity factor (0.0 to 1.0)
                double foilAlpha = ((double)mask / 255.0) * 0.50;
                
                // Screen / additive blend with specular highlight
                outR += specR * foilAlpha;
                outG += specG * foilAlpha;
                outB += specB * foilAlpha;
            }

            uint8_t clampedR = (uint8_t)(outR > 255.0 ? 255 : outR);
            uint8_t clampedG = (uint8_t)(outG > 255.0 ? 255 : outG);
            uint8_t clampedB = (uint8_t)(outB > 255.0 ? 255 : outB);

            if (fixColors) {
                // Store in BGR order for GDI+ PixelFormat24bppRGB
                composite[dstIdx + 0] = clampedB;
                composite[dstIdx + 1] = clampedG;
                composite[dstIdx + 2] = clampedR;
            } else {
                // Legacy unswapped buffer
                composite[dstIdx + 0] = clampedR;
                composite[dstIdx + 1] = clampedG;
                composite[dstIdx + 2] = clampedB;
            }
        }
    }

    Bitmap bitmap(width, height, stride, PixelFormat24bppRGB, composite.data());
    if (fileDpi > 0) {
        bitmap.SetResolution((REAL)fileDpi, (REAL)fileDpi);
    }

    wchar_t wFilepath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, filepath.c_str(), -1, wFilepath, MAX_PATH);

    Status status = bitmap.Save(wFilepath, &encoderClsid, NULL);
    if (status != Ok) {
        LOG_ERR("GDI+ failed to save composite holo image to: " + filepath);
        return false;
    }

    std::string dpiStr = (fileDpi > 0) ? (", DPI=" + std::to_string(fileDpi)) : "";
    LOG_INFO("Saved combined holo composite (" + std::string(format == FORMAT_BMP ? "BMP" : "PNG") + 
             ", FixColors=" + std::to_string(fixColors) + dpiStr + ") to: " + filepath);
    return true;
}

// PNG Wrappers
bool ExportToPNG(const std::string& filepath, const uint8_t* rgbBuffer, int width, int height, bool fixColors, int fileDpi) {
    return ExportCardImage(filepath, rgbBuffer, width, height, FORMAT_PNG, fixColors, fileDpi);
}

bool ExportHoloToPNG(const std::string& filepath, const uint8_t* grayBuffer, int width, int height, int fileDpi) {
    return ExportHoloImage(filepath, grayBuffer, width, height, FORMAT_PNG, fileDpi);
}

bool ExportHoloCompositeToPNG(const std::string& filepath, const uint8_t* rgbBuffer, const uint8_t* grayBuffer, int width, int height, bool fixColors, int fileDpi) {
    return ExportHoloCompositeImage(filepath, rgbBuffer, grayBuffer, width, height, FORMAT_PNG, fixColors, fileDpi);
}

// BMP Wrappers
bool ExportToBMP(const std::string& filepath, const uint8_t* rgbBuffer, int width, int height, bool fixColors, int fileDpi) {
    return ExportCardImage(filepath, rgbBuffer, width, height, FORMAT_BMP, fixColors, fileDpi);
}

bool ExportHoloToBMP(const std::string& filepath, const uint8_t* grayBuffer, int width, int height, int fileDpi) {
    return ExportHoloImage(filepath, grayBuffer, width, height, FORMAT_BMP, fileDpi);
}

bool ExportHoloCompositeToBMP(const std::string& filepath, const uint8_t* rgbBuffer, const uint8_t* grayBuffer, int width, int height, bool fixColors, int fileDpi) {
    return ExportHoloCompositeImage(filepath, rgbBuffer, grayBuffer, width, height, FORMAT_BMP, fixColors, fileDpi);
}
