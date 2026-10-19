#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include "config.h"

class CardBackManager {
public:
    static CardBackManager& Instance();

    // Resolves and loads the card back RGB24 buffer scaled to targetWidth x targetHeight.
    // Fills outRgbBuffer with 24-bit RGB (Byte 0=R, Byte 1=G, Byte 2=B) matching game engine format.
    // If image files are missing and autoGenerateTemplate is enabled, generates a procedural template.
    bool LoadCardBackRgb(int targetWidth, int targetHeight, std::vector<uint8_t>& outRgbBuffer, std::string* outLoadedPath = nullptr);

    // Resolves candidate paths for card back image file
    std::string ResolveCardBackFilePath(const std::string& customPath, std::vector<std::string>* outSearchedPaths = nullptr);

    // Procedurally generates an official-styled Card Maker card back in RGB24
    bool GenerateProceduralCardBack(int width, int height, std::vector<uint8_t>& outRgbBuffer);

    // Clears cached RGB buffer
    void ClearCache();

private:
    CardBackManager() = default;
    ~CardBackManager() = default;

    std::vector<uint8_t> m_cachedRgb;
    int m_cachedWidth = 0;
    int m_cachedHeight = 0;
    std::string m_cachedSourcePath;
};

// Global helper wrapper
bool LoadCardBackRgb(int targetWidth, int targetHeight, std::vector<uint8_t>& outRgbBuffer, std::string* outLoadedPath = nullptr);
