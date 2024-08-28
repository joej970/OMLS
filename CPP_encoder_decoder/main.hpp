
#pragma once

#include <cstdint>

// https://stackoverflow.com/questions/8526598/how-does-stdforward-work

struct Params {
    char fileName[100];
    const char* folder_in;
    const char* folder_out;
    std::size_t imgIdx_min;
    std::size_t imgIdx_max;
    std::size_t lossyBits;
    std::size_t unaryMaxWidth;
    std::size_t header_bytes;
    std::uint16_t width;
    std::uint16_t height;
    std::uint8_t bpp;
    bool compress;
    bool decompress;
    bool ideal_compress;
    bool use_gpu;
};

void printHelp();
Params parseArguments(int argc, char* argv[]);

std::uint64_t getCurrentTimeMicros();

int main(int argc, char* argv[]);

// void compressImageRangeAGOR(char* folder, std::size_t imgIdx_min, std::size_t imgIdx_max);
void compressImageRangeAGOR(
   const char* fileName,
   const char* folder_in,
   const char* folder_out,
   std::size_t imgIdx_min,
   std::size_t imgIdx_max,
   std::size_t unaryMaxWidth,
   std::uint8_t bpp,
   std::vector<std::uint32_t>* N,
   std::vector<std::uint32_t>* A_init,
   std::size_t lossyBits,
   std::vector<std::size_t>* imageSizes,
   std::size_t headerBytes);
void compressImageRangeIdeal(
   const char* fileName,
   const char* folder_in,
   const char* folder_out,
   std::size_t imgIdx_min,
   std::size_t imgIdx_max,
   std::size_t unaryMaxWidth,
   std::uint8_t bpp,
   std::size_t lossyBits,
   std::vector<std::size_t>* imageSizes,
   std::size_t headerBytes);
void decompressImageRangeAGOR(
   const char* fileName,
   const char* folder_in,
   const char* folder_out,
   std::size_t imgIdx_min,
   std::size_t imgIdx_max,
   std::size_t unaryMaxWidth,
   std::uint8_t bpp,
   std::vector<std::uint32_t>* N,
   std::vector<std::uint32_t>* A_init,
   std::size_t lossyBits,
   std::vector<std::size_t>* imageSizes,
   std::size_t headerBytes,
   bool use_gpu);

void runTests();
void createMissingDirectories(const char* folder_out);
void translateBinaryToASCII_hex(char* fileNameIn);
void translateBinaryToASCII_bin(char* fileNameIn);