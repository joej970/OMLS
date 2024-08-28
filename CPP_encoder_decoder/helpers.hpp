
#pragma once

#include "Image.hpp"
#include "ImageYCCC.hpp"
#include "globalDefines.hpp"
#include <cstdint>

class Helpers
{
  public:
    static pImage read_image(const char* input, std::size_t header_bytes, size_t width, size_t height);

    static pImageYCCC bayer_to_YCCC(const Image* img, std::size_t lossyBits);

    static int transformColorGB(
       const std::uint16_t gb,   //
       const std::uint16_t b,   //
       const std::uint16_t r,   //
       const std::uint16_t gr,   //
       std::int16_t* y_o,   //
       std::int16_t* cd_o,   //
       std::int16_t* cm_o,   //
       std::int16_t* co_o,
       std::size_t lossyBits);

    static bool isLittleEndian();

    static void dump16pp(const char* outputFile, const Image* pImg);
    static void dump8pp(const char* outputFile, const Image* pImg);
    static void dumpBayer(const char* outputFile, const Image* pImg);

    static void dumpToFile(const char* fileName, const sQuadChannelCS* quadCh, std::size_t width, std::size_t height);
    static void dumpToFile(const char* fileName, const uQuadChannelCS* quadCh, std::size_t width, std::size_t height);
};
