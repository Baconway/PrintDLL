#include "../include/bleed_helper.h"
#include "../include/logger.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <cmath>
#include <algorithm>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

static bool g_bleedGdiplusInit = false;
static ULONG_PTR g_bleedGdiplusToken = 0;

static void EnsureBleedGdiplus() {
    if (!g_bleedGdiplusInit) {
        GdiplusStartupInput gsi;
        GdiplusStartup(&g_bleedGdiplusToken, &gsi, NULL);
        g_bleedGdiplusInit = true;
    }
}

bool ApplyCardBleed(const uint8_t* srcRgb, int srcW, int srcH,
                    std::vector<uint8_t>& outRgb, int& outW, int& outH,
                    const BleedConfig& cfg) {
    if (!srcRgb || srcW <= 0 || srcH <= 0) return false;

    if (!cfg.enabled || cfg.bleedMm <= 0.0) {
        outRgb.assign(srcRgb, srcRgb + (size_t)srcW * srcH * 3);
        outW = srcW;
        outH = srcH;
        return true;
    }

    double innerW_mm = cfg.finalCardWidthMm - 2.0 * cfg.bleedMm;
    double innerH_mm = cfg.finalCardHeightMm - 2.0 * cfg.bleedMm;
    if (innerW_mm <= 0.0 || innerH_mm <= 0.0) {
        LOG_ERR("Invalid bleed parameters: bleed margin exceeds card dimensions");
        return false;
    }

    EnsureBleedGdiplus();

    // Scale calculation using vertical resolution baseline
    double px_per_mm = (double)srcH / innerH_mm;
    outH = (int)std::round(cfg.finalCardHeightMm * px_per_mm);
    outW = (int)std::round(cfg.finalCardWidthMm * px_per_mm);

    int bleedY = (int)std::round(cfg.bleedMm * px_per_mm);
    int bleedX = (int)std::round(cfg.bleedMm * px_per_mm);

    int innerW = outW - 2 * bleedX;
    int innerH = outH - 2 * bleedY;
    if (innerW <= 0 || innerH <= 0) return false;

    int radius = (int)std::round(cfg.cornerRadiusMm * px_per_mm);
    if (radius < 1) radius = 1;
    if (radius * 2 > innerW) radius = innerW / 2;
    if (radius * 2 > innerH) radius = innerH / 2;

    // Convert srcRgb to BGR scanline stride for GDI+ Bitmap
    int srcStride = ((srcW * 3 + 3) / 4) * 4;
    std::vector<uint8_t> srcBgr((size_t)srcStride * srcH, 0);
    for (int y = 0; y < srcH; y++) {
        const uint8_t* s = srcRgb + (size_t)y * srcW * 3;
        uint8_t* d = srcBgr.data() + (size_t)y * srcStride;
        for (int x = 0; x < srcW; x++) {
            d[x * 3 + 0] = s[x * 3 + 2]; // Blue
            d[x * 3 + 1] = s[x * 3 + 1]; // Green
            d[x * 3 + 2] = s[x * 3 + 0]; // Red
        }
    }
    Bitmap srcBmp(srcW, srcH, srcStride, PixelFormat24bppRGB, srcBgr.data());

    // Target bitmap with bleed border
    Bitmap targetBmp(outW, outH, PixelFormat24bppRGB);
    Graphics g(&targetBmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);

    // 1. Fill entire canvas with the bleed border color (Black by default)
    SolidBrush bgBrush(Color(255, cfg.bleedColorR, cfg.bleedColorG, cfg.bleedColorB));
    g.FillRectangle(&bgBrush, 0, 0, outW, outH);

    // 2. Build rounded rectangle path for the inner card area
    GraphicsPath path;
    int d = radius * 2;
    path.AddArc(bleedX, bleedY, d, d, 180, 90);
    path.AddArc(bleedX + innerW - d, bleedY, d, d, 270, 90);
    path.AddArc(bleedX + innerW - d, bleedY + innerH - d, d, d, 0, 90);
    path.AddArc(bleedX, bleedY + innerH - d, d, d, 90, 90);
    path.CloseFigure();

    // 3. Clip to the rounded card path and draw card image inside
    g.SetClip(&path);
    g.DrawImage(&srcBmp, bleedX, bleedY, innerW, innerH);
    g.ResetClip();

    // 4. Extract 24-bit RGB result
    Rect rect(0, 0, outW, outH);
    BitmapData bData;
    if (targetBmp.LockBits(&rect, ImageLockModeRead, PixelFormat24bppRGB, &bData) != Ok) {
        LOG_ERR("Failed to lock bits for bleed bitmap");
        return false;
    }

    outRgb.assign((size_t)outW * outH * 3, 0);
    for (int y = 0; y < outH; y++) {
        const uint8_t* rowSrc = (const uint8_t*)bData.Scan0 + (size_t)y * bData.Stride;
        uint8_t* rowDst = outRgb.data() + (size_t)y * outW * 3;
        for (int x = 0; x < outW; x++) {
            rowDst[x * 3 + 0] = rowSrc[x * 3 + 2]; // R
            rowDst[x * 3 + 1] = rowSrc[x * 3 + 1]; // G
            rowDst[x * 3 + 2] = rowSrc[x * 3 + 0]; // B
        }
    }
    targetBmp.UnlockBits(&bData);

    LOG_DEBUG("Applied bleed (" + std::to_string(cfg.bleedMm) + "mm, R=" + std::to_string(cfg.cornerRadiusMm) +
              "mm) -> Final size: " + std::to_string(outW) + "x" + std::to_string(outH) +
              " px (" + std::to_string(cfg.finalCardWidthMm) + "x" + std::to_string(cfg.finalCardHeightMm) + " mm)");
    return true;
}

bool ApplyHoloMaskBleed(const uint8_t* srcGray, int srcW, int srcH,
                        std::vector<uint8_t>& outGray, int& outW, int& outH,
                        const BleedConfig& cfg) {
    if (!srcGray || srcW <= 0 || srcH <= 0) return false;

    if (!cfg.enabled || cfg.bleedMm <= 0.0) {
        outGray.assign(srcGray, srcGray + (size_t)srcW * srcH);
        outW = srcW;
        outH = srcH;
        return true;
    }

    // Convert grayscale to RGB24
    std::vector<uint8_t> grayRgb((size_t)srcW * srcH * 3, 0);
    for (size_t i = 0; i < (size_t)srcW * srcH; i++) {
        uint8_t val = srcGray[i];
        grayRgb[i * 3 + 0] = val;
        grayRgb[i * 3 + 1] = val;
        grayRgb[i * 3 + 2] = val;
    }

    BleedConfig maskCfg = cfg;
    maskCfg.bleedColorR = 0;
    maskCfg.bleedColorG = 0;
    maskCfg.bleedColorB = 0;

    std::vector<uint8_t> outRgb;
    if (!ApplyCardBleed(grayRgb.data(), srcW, srcH, outRgb, outW, outH, maskCfg)) {
        return false;
    }

    // Convert back to grayscale
    outGray.assign((size_t)outW * outH, 0);
    for (size_t i = 0; i < (size_t)outW * outH; i++) {
        outGray[i] = outRgb[i * 3 + 0];
    }
    return true;
}
