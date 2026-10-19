#include "../include/card_back.h"
#include "../include/logger.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <vector>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

static bool g_cardBackGdiplusInit = false;
static ULONG_PTR g_cardBackGdiplusToken = 0;

static void EnsureCardBackGdiplus() {
    if (!g_cardBackGdiplusInit) {
        GdiplusStartupInput gsi;
        GdiplusStartup(&g_cardBackGdiplusToken, &gsi, NULL);
        g_cardBackGdiplusInit = true;
    }
}

static bool FileExists(const std::string& path) {
    if (path.empty()) return false;
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static std::string NormalizePath(const std::string& path) {
    if (path.empty()) return "";
    char buf[MAX_PATH] = {0};
    if (GetFullPathNameA(path.c_str(), MAX_PATH, buf, NULL) != 0) {
        return std::string(buf);
    }
    return path;
}

static void AddPathCandidate(std::vector<std::string>& list, const std::string& p) {
    if (p.empty()) return;
    std::string norm = NormalizePath(p);
    for (const auto& item : list) {
        if (_stricmp(item.c_str(), norm.c_str()) == 0) return;
    }
    list.push_back(norm);
}

CardBackManager& CardBackManager::Instance() {
    static CardBackManager s_mgr;
    return s_mgr;
}

void CardBackManager::ClearCache() {
    m_cachedRgb.clear();
    m_cachedWidth = 0;
    m_cachedHeight = 0;
    m_cachedSourcePath.clear();
}

std::string CardBackManager::ResolveCardBackFilePath(const std::string& customPath, std::vector<std::string>* outSearchedPaths) {
    std::vector<std::string> candidates;
    const AppConfig& cfg = GetConfig();

    std::string dllDir = GetPrintDllDirectory();
    std::string exeDir = GetPrintExeDirectory();
    std::string iniDir = cfg.loadedIniDir;

    // 1. If customPath is given, prioritize it
    if (!customPath.empty()) {
        AddPathCandidate(candidates, customPath);
        if (!iniDir.empty()) AddPathCandidate(candidates, iniDir + customPath);
        if (!dllDir.empty()) AddPathCandidate(candidates, dllDir + customPath);
        if (!exeDir.empty()) AddPathCandidate(candidates, exeDir + customPath);
    }

    // 2. Standard base search directories
    std::vector<std::string> baseDirs;
    if (!iniDir.empty()) baseDirs.push_back(iniDir);
    if (!dllDir.empty()) baseDirs.push_back(dllDir);
    if (!exeDir.empty()) baseDirs.push_back(exeDir);
    baseDirs.push_back(""); // Working directory
    baseDirs.push_back("UserData/");
    baseDirs.push_back(exeDir + "UserData/");

    // 3. Known standard filenames for card back
    const char* fileNames[] = {
        "card_backs/UI_CMU_GachaScene_100000.png",
        "card_backs/card_back.png",
        "card_backs/default_back.png",
        "card_backs/card_back.jpg",
        "card_backs/card_back.bmp",
        "UI_CMU_GachaScene_100000.png",
        "card_back.png"
    };

    for (const auto& dir : baseDirs) {
        for (const char* name : fileNames) {
            AddPathCandidate(candidates, dir + name);
        }
    }

    // 4. Also check explicit known user picture asset path if on this host
    AddPathCandidate(candidates, "C:\\Users\\Baconway\\Pictures\\1.44 cm\\Assets\\Texture2D\\UI_CMU_GachaScene_100000.png");

    if (outSearchedPaths) {
        *outSearchedPaths = candidates;
    }

    for (const auto& path : candidates) {
        if (FileExists(path)) {
            return path;
        }
    }

    return "";
}

bool CardBackManager::GenerateProceduralCardBack(int width, int height, std::vector<uint8_t>& outRgbBuffer) {
    if (width <= 0 || height <= 0) return false;
    EnsureCardBackGdiplus();

    Bitmap bmp(width, height, PixelFormat24bppRGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    // Dark carbon background
    SolidBrush bgBrush(Color(255, 26, 26, 28));
    g.FillRectangle(&bgBrush, 0, 0, width, height);

    // Subtle geometric pattern
    Pen gridPen(Color(255, 38, 40, 44), 1.0f);
    int step = 32;
    for (int y = -height; y < height * 2; y += step) {
        g.DrawLine(&gridPen, 0, y, width, y + width);
        g.DrawLine(&gridPen, 0, y + width, width, y);
    }

    // White outer border
    Pen outerPen(Color(255, 240, 240, 245), 6.0f);
    g.DrawRectangle(&outerPen, 18, 18, width - 36, height - 36);

    // Inner thin border
    Pen innerPen(Color(255, 180, 180, 190), 2.0f);
    g.DrawRectangle(&innerPen, 28, 28, width - 56, height - 56);

    // Corner diagonal triangles
    SolidBrush accentBrush(Color(255, 230, 230, 235));
    Point cornerTopLeft[] = { Point(18, 18), Point(90, 18), Point(18, 90) };
    g.FillPolygon(&accentBrush, cornerTopLeft, 3);
    Point cornerBottomRight[] = { Point(width - 18, height - 18), Point(width - 90, height - 18), Point(width - 18, height - 90) };
    g.FillPolygon(&accentBrush, cornerBottomRight, 3);

    // Center Logo Box
    int boxW = 460;
    int boxH = 180;
    int boxX = (width - boxW) / 2;
    int boxY = (height - boxH) / 2;
    SolidBrush boxBg(Color(230, 20, 20, 22));
    g.FillRectangle(&boxBg, boxX, boxY, boxW, boxH);
    Pen boxBorder(Color(255, 220, 220, 230), 3.0f);
    g.DrawRectangle(&boxBorder, boxX, boxY, boxW, boxH);

    // Center Logo Emblem
    SolidBrush redEmblem(Color(255, 218, 37, 28));
    Point emblemPoints[] = {
        Point(width / 2, boxY + 25),
        Point(width / 2 + 40, boxY + 65),
        Point(width / 2, boxY + 105),
        Point(width / 2 - 40, boxY + 65)
    };
    g.FillPolygon(&redEmblem, emblemPoints, 4);

    // Text: SEGA at top right
    FontFamily fontFamily(L"Arial");
    Font fontSega(&fontFamily, 24, FontStyleBold, UnitPixel);
    SolidBrush blueSega(Color(255, 0, 102, 204));
    StringFormat fmtRight;
    fmtRight.SetAlignment(StringAlignmentFar);
    RectF segaRect((REAL)(width - 240), 40.0f, 200.0f, 40.0f);
    g.DrawString(L"SEGA", -1, &fontSega, segaRect, &fmtRight, &blueSega);

    // Text: CARD MAKER in center
    Font fontTitle(&fontFamily, 28, FontStyleBold, UnitPixel);
    SolidBrush whiteText(Color(255, 255, 255, 255));
    StringFormat fmtCenter;
    fmtCenter.SetAlignment(StringAlignmentCenter);
    RectF titleRect((REAL)boxX, (REAL)(boxY + 120), (REAL)boxW, 40.0f);
    g.DrawString(L"CARD MAKER", -1, &fontTitle, titleRect, &fmtCenter, &whiteText);

    // Text: (C)SEGA at bottom left
    Font fontCopy(&fontFamily, 16, FontStyleRegular, UnitPixel);
    SolidBrush copyText(Color(255, 200, 200, 200));
    StringFormat fmtLeft;
    fmtLeft.SetAlignment(StringAlignmentNear);
    RectF copyRect(40.0f, (REAL)(height - 60), 200.0f, 30.0f);
    g.DrawString(L"(C)SEGA", -1, &fontCopy, copyRect, &fmtLeft, &copyText);

    // Lock bits to copy into RGB24 buffer
    Rect rect(0, 0, width, height);
    BitmapData bData;
    if (bmp.LockBits(&rect, ImageLockModeRead, PixelFormat24bppRGB, &bData) != Ok) {
        LOG_ERR("Failed to lock bits on procedural card back bitmap");
        return false;
    }

    outRgbBuffer.assign((size_t)width * height * 3, 0);
    for (int y = 0; y < height; y++) {
        const uint8_t* srcRow = (const uint8_t*)bData.Scan0 + y * bData.Stride;
        uint8_t* dstRow = outRgbBuffer.data() + y * width * 3;
        for (int x = 0; x < width; x++) {
            // GDI+ PixelFormat24bppRGB is BGR in memory -> convert to RGB24
            dstRow[x * 3 + 0] = srcRow[x * 3 + 2]; // Red
            dstRow[x * 3 + 1] = srcRow[x * 3 + 1]; // Green
            dstRow[x * 3 + 2] = srcRow[x * 3 + 0]; // Blue
        }
    }
    bmp.UnlockBits(&bData);

    LOG_INFO("Procedurally generated Card Maker card back (" + std::to_string(width) + "x" + std::to_string(height) + ")");
    return true;
}

bool CardBackManager::LoadCardBackRgb(int targetWidth, int targetHeight, std::vector<uint8_t>& outRgbBuffer, std::string* outLoadedPath) {
    if (targetWidth <= 0 || targetHeight <= 0) return false;

    const AppConfig& cfg = GetConfig();

    // Check cached buffer first if dimensions match and source hasn't changed
    std::string customPath = cfg.cardBack.customImagePath;
    if (!m_cachedRgb.empty() && m_cachedWidth == targetWidth && m_cachedHeight == targetHeight) {
        if (!m_cachedSourcePath.empty() && FileExists(m_cachedSourcePath)) {
            outRgbBuffer = m_cachedRgb;
            if (outLoadedPath) *outLoadedPath = m_cachedSourcePath;
            return true;
        }
    }

    std::vector<std::string> searched;
    std::string resolvedPath = ResolveCardBackFilePath(customPath, &searched);

    if (!resolvedPath.empty()) {
        EnsureCardBackGdiplus();
        wchar_t wPath[MAX_PATH] = {0};
        MultiByteToWideChar(CP_UTF8, 0, resolvedPath.c_str(), -1, wPath, MAX_PATH);

        Bitmap sourceBmp(wPath);
        if (sourceBmp.GetLastStatus() == Ok && sourceBmp.GetWidth() > 0 && sourceBmp.GetHeight() > 0) {
            int srcW = sourceBmp.GetWidth();
            int srcH = sourceBmp.GetHeight();

            Bitmap* pTargetBmp = nullptr;
            std::unique_ptr<Bitmap> scaledBmpHolder;

            if (srcW == targetWidth && srcH == targetHeight) {
                pTargetBmp = &sourceBmp;
            } else {
                LOG_INFO("Scaling card back image from " + std::to_string(srcW) + "x" + std::to_string(srcH) +
                         " to target " + std::to_string(targetWidth) + "x" + std::to_string(targetHeight));
                scaledBmpHolder = std::make_unique<Bitmap>(targetWidth, targetHeight, PixelFormat24bppRGB);
                Graphics g(scaledBmpHolder.get());
                g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
                g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
                g.SetSmoothingMode(SmoothingModeHighQuality);
                g.DrawImage(&sourceBmp, 0, 0, targetWidth, targetHeight);
                pTargetBmp = scaledBmpHolder.get();
            }

            Rect rect(0, 0, targetWidth, targetHeight);
            BitmapData bData;
            if (pTargetBmp->LockBits(&rect, ImageLockModeRead, PixelFormat24bppRGB, &bData) == Ok) {
                outRgbBuffer.assign((size_t)targetWidth * targetHeight * 3, 0);
                for (int y = 0; y < targetHeight; y++) {
                    const uint8_t* srcRow = (const uint8_t*)bData.Scan0 + y * bData.Stride;
                    uint8_t* dstRow = outRgbBuffer.data() + y * targetWidth * 3;
                    for (int x = 0; x < targetWidth; x++) {
                        // Convert GDI+ BGR scanline to standard RGB24
                        dstRow[x * 3 + 0] = srcRow[x * 3 + 2]; // R
                        dstRow[x * 3 + 1] = srcRow[x * 3 + 1]; // G
                        dstRow[x * 3 + 2] = srcRow[x * 3 + 0]; // B
                    }
                }
                pTargetBmp->UnlockBits(&bData);

                // Update cache
                m_cachedRgb = outRgbBuffer;
                m_cachedWidth = targetWidth;
                m_cachedHeight = targetHeight;
                m_cachedSourcePath = resolvedPath;

                if (outLoadedPath) *outLoadedPath = resolvedPath;
                LOG_INFO("Loaded card back image from: '" + resolvedPath + "' (" +
                         std::to_string(targetWidth) + "x" + std::to_string(targetHeight) + " RGB24)");
                return true;
            }
        }
        LOG_WARN("Failed to load or lock bitmap from image file: '" + resolvedPath + "'");
    } else {
        LOG_WARN("Could not find card back image file on disk (searched " +
                 std::to_string(searched.size()) + " candidate locations).");
    }

    // Fallback: procedural template generation
    if (cfg.cardBack.autoGenerateTemplate) {
        LOG_INFO("Generating procedural card back template fallback.");
        bool ok = GenerateProceduralCardBack(targetWidth, targetHeight, outRgbBuffer);
        if (ok) {
            m_cachedRgb = outRgbBuffer;
            m_cachedWidth = targetWidth;
            m_cachedHeight = targetHeight;
            m_cachedSourcePath = "<ProceduralTemplate>";
            if (outLoadedPath) *outLoadedPath = "<ProceduralTemplate>";
            return true;
        }
    }

    LOG_ERR("Failed to obtain card back image buffer.");
    return false;
}

bool LoadCardBackRgb(int targetWidth, int targetHeight, std::vector<uint8_t>& outRgbBuffer, std::string* outLoadedPath) {
    return CardBackManager::Instance().LoadCardBackRgb(targetWidth, targetHeight, outRgbBuffer, outLoadedPath);
}
