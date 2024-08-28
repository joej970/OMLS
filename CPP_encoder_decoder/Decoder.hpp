#pragma once

#include "Channels.hpp"
#include "DecoderBase.hpp"
#include "globalDefines.hpp"
#include "helpers.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>

struct headerData_t {
    std::uint64_t timestamp;
    std::uint64_t roi;
    std::uint16_t width;
    std::uint16_t height;
    std::uint8_t unaryMaxWidth;
    std::uint8_t bpp;
    std::uint8_t lossyBits;
    std::uint8_t reserved;
};

class Decoder : public DecoderBase
{
  private:
    std::size_t m_width        = 0;
    std::size_t m_height       = 0;
    std::size_t m_pixelAmount  = 0;
    std::size_t m_width_bayer  = 0;
    std::size_t m_height_bayer = 0;
    std::uint16_t m_k_min      = 0;
    std::size_t m_N_threshold  = 0;
    std::size_t m_A_init       = 0;

    std::unique_ptr<std::vector<std::uint8_t>> m_pFileData;
    std::unique_ptr<sQuadChannelCS> m_pQuotients;
    std::unique_ptr<sQuadChannelCS> m_pRemainders;
    std::unique_ptr<sQuadChannelCS> m_pkValues;
    std::unique_ptr<sQuadChannelCS> m_pAbs;
    std::unique_ptr<sQuadChannelCS> m_pDpcm;
    std::unique_ptr<sQuadChannelCS> m_pFull;
    std::unique_ptr<std::vector<std::uint8_t>> m_pBayer_8bit;
    std::unique_ptr<std::vector<std::uint16_t>> m_pBayer_16bit;

  public:
    Decoder();
    Decoder(
       const char* fileName,
       std::size_t width,
       std::size_t height,
       std::uint32_t A_init,
       std::uint32_t N_threshold);
    Decoder(const char* fileName, std::uint32_t A_init, std::uint32_t N_threshold);

    void decodeSequentially(std::size_t lossyBits);
    headerData_t decodeParallel();
    headerData_t decodeParallelGPU();
    std::size_t decodeBitstream(
       Reader& reader,
       std::uint32_t N_threshold,
       std::uint32_t A_init,
       std::size_t length,
       std::int16_t* quotients,
       std::int16_t* remainders,
       std::int16_t* kValues,
       std::int16_t* dpcm);

    void exportBayerImage(const char* fileName, std::uint64_t addHeader, std::uint64_t roi, std::uint64_t timestamp);
    void exportBayerImage(const char* fileName, std::uint8_t* data, std::size_t yccc_width, std::size_t yccc_height);
    void toBayerGB(std::size_t lossyBits);
    static void YCCC_to_BayerGB(
       const std::int16_t y,
       const std::int16_t cd,
       const std::int16_t cm,
       const std::int16_t co,
       std::uint8_t& gb,
       std::uint8_t& b,
       std::uint8_t& r,
       std::uint8_t& gr,
       std::size_t lossyBits);
    void toFullAll();
    void toFull(const std::int16_t* src, std::int16_t* dst, std::size_t height, std::size_t width);
    void decodeBitstreamAll(std::uint32_t N_threshold, std::uint32_t A_init);
    void importBitstream(const char* fileName);
    void readBitstreamOnChannel(Reader reader, uQuadChannelCS::Channel ch);

    std::size_t getWidth() const;
    std::size_t getHeight() const;
    std::size_t getLength() const;
    std::size_t getWidthBayer() const;
    std::size_t getHeightBayer() const;
};