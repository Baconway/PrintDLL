#pragma once

#include <stdint.h>
#include <string>
#include "config.h"

// Unified image export functions supporting PNG and BMP formats with color fix option and custom DPI
bool ExportCardImage(const std::string& filepath, const uint8_t* rgbBuffer, int width, int height, ExportFormat format, bool fixColors, int fileDpi = 0);
bool ExportCardSpreadImage(const std::string& filepath, const uint8_t* frontRgb, const uint8_t* backRgb, int width, int height, ExportFormat format, bool fixColors, int fileDpi = 0);
bool ExportHoloImage(const std::string& filepath, const uint8_t* grayBuffer, int width, int height, ExportFormat format, int fileDpi = 0);
bool ExportHoloCompositeImage(const std::string& filepath, const uint8_t* rgbBuffer, const uint8_t* grayBuffer, int width, int height, ExportFormat format, bool fixColors, int fileDpi = 0);

// PNG export wrappers
bool ExportToPNG(const std::string& filepath, const uint8_t* rgbBuffer, int width, int height, bool fixColors = true, int fileDpi = 0);
bool ExportHoloToPNG(const std::string& filepath, const uint8_t* grayBuffer, int width, int height, int fileDpi = 0);
bool ExportHoloCompositeToPNG(const std::string& filepath, const uint8_t* rgbBuffer, const uint8_t* grayBuffer, int width, int height, bool fixColors = true, int fileDpi = 0);

// BMP export wrappers
bool ExportToBMP(const std::string& filepath, const uint8_t* rgbBuffer, int width, int height, bool fixColors = true, int fileDpi = 0);
bool ExportHoloToBMP(const std::string& filepath, const uint8_t* grayBuffer, int width, int height, int fileDpi = 0);
bool ExportHoloCompositeToBMP(const std::string& filepath, const uint8_t* rgbBuffer, const uint8_t* grayBuffer, int width, int height, bool fixColors = true, int fileDpi = 0);

// Helper to ensure target directory exists
bool EnsureDirectoryExists(const std::string& dirPath);

// Generate timestamp string YYYYMMDD_HHMMSS
std::string GetTimestampString();
