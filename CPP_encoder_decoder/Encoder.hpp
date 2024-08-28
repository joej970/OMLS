#pragma once

#include "ImageYCCC.hpp"
#include "globalDefines.hpp"
#include "helpers.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>

struct Writter_s {
    std::ofstream* m_pWf;
    std::uint32_t* m_pBfr;
    std::size_t* m_pBitCnt;
    std::size_t* m_pBytesCnt;

    explicit Writter_s();
    explicit Writter_s(std::ofstream* pWf, std::uint32_t* pBfr, std::size_t* pBitCnt, std::size_t* pBytes);
};

class Encoder
{
  private:
    const Image* m_pImg         = nullptr;
    const ImageYCCC* m_pImgYCCC = nullptr;
    std::size_t m_width, m_height, m_length;
    sQuadChannelCS m_kValues;
    sQuadChannelCS m_dpcm;   // max value 2*max(dpcm) = 1020 - 0 = 1020 (Y channel), 255 - -255 = 510 (others)
    sQuadChannelCS m_abs;   // max value 2*max(dpcm) = 2*1020 = 2040 (max val for int16 = 32768)
    sQuadChannelCS m_quotient;   // max(m_data)/2**k @k=1 => 2040/2=1020
    sQuadChannelCS m_remainder;   // max = 2^k;
    const char* m_folderOut;
    std::size_t m_imgIdx;
    std::uint32_t m_A_init      = 32;
    std::uint32_t m_N_threshold = 8;
    std::size_t m_lossyBits     = 0;
    std::size_t m_unaryMaxWidth = C_MAX_UNARY_LENGTH;
    std::uint8_t m_bpp = 8;
    std::size_t m_header_bytes  = 0;
    const char* m_fileName       = nullptr;
    std::size_t m_k_seed        = 0;
    std::uint16_t m_k_min       = 0;
    std::uint16_t m_k_max       = 0;
    std::uint64_t m_roi         = 0;
    std::size_t m_fileSize      = 0;
    std::size_t m_idealRule     = 0;
#ifdef DUMP_VERIFICATION
    std::size_t m_row                 = 0;
    std::size_t m_col                 = 0;
    std::ofstream m_wf_bitstream      = 0;
    std::size_t m_ASCII_bitstream_cnt = 0;
    std::size_t m_charsInRow          = 0;
#endif

  public:
    enum class algorithm
    {
        singleSeedInTwos,
        diffUp,
        ideal,
        end
    };
    enum method
    {
        singleSeedInTwos,
        ideal,
        parallel_standard,
        parallel_limited,
        end
    };
    Encoder(
       const Image* pImg,
       std::size_t width,
       std::size_t height,
       const char* folderOut,
       std::size_t imgIdx,
       std::uint32_t A_init,
       std::uint32_t N_threshold,
       std::size_t lossyBits,
       std::size_t unaryMaxWidth,
       std::uint8_t bpp,
       std::size_t header_bytes,
       const char* fileName);
    Encoder(
       const ImageYCCC* pImgYCCC,
       const char* folderOut,
       std::size_t imgIdx,
       std::size_t lossyBits,
       std::size_t header_bytes);

    std::unique_ptr<std::vector<std::size_t>> encodeUsingMethod(Encoder::method method);

    const sQuadChannelCS* getAbsChannelsConst() const;
    const sQuadChannelCS* getQuotientChannelsConst() const;
    const sQuadChannelCS* getRemainderChannelsConst() const;

    sQuadChannelCS* getDpcmChannels();
    const sQuadChannelCS* getDpcmChannelsConst() const;

    std::unique_ptr<std::vector<std::size_t>> runParallelCompression();
    std::size_t encodeParallel(
       std::uint16_t gb,   //
       std::uint16_t b,
       std::uint16_t r,
       std::uint16_t gr);
    void encodeParallelOneQuadrupleSeedPixel(
       std::uint16_t gb,   //
       std::uint16_t b,
       std::uint16_t r,
       std::uint16_t gr,
       std::int16_t* YCCC_prev,
       std::uint32_t* A,
       Writter_s writter);
    void encodeParallelOneQuadruple(
       std::uint16_t gb,
       std::uint16_t b,
       std::uint16_t r,
       std::uint16_t gr,
       std::int16_t* YCCC_prev,
       std::uint32_t* A,
       std::uint32_t& N,
       std::uint32_t N_threshold,
       std::uint8_t last,
       Writter_s writter);

    void toAbs(const std::int16_t* src, std::int16_t* dst, std::size_t length);
    std::int16_t toAbsSingle(const std::int16_t src);
    void toAbsAll();
    void toAbsAll(const sQuadChannelCS* pIn);

    void golombRiceCalcAll(void);
    void golombRiceOnChannel(sQuadChannelCS::Channel ch);
    void adaptiveGolombRiceSingleSeedInTwos(
       const std::int16_t* dpcm,
       std::size_t length,
       std::size_t width,
       std::int16_t* q,
       std::int16_t* r,
       std::int16_t* kValues,
       std::uint32_t N_threshold,
       std::uint32_t A_init);
    void adaptiveGolombRiceAll(   //
       Encoder::algorithm algo);

    void differentiateAll(Encoder::algorithm algo);
    void differentiate(sQuadChannelCS::Channel channel);
    void differentiateDiffUp(sQuadChannelCS::Channel channel);

    std::size_t getWidth() const;
    std::size_t getHeight() const;
    std::size_t getLength() const;
    std::size_t getFileSize() const;
    const std::size_t getGolombRiceParameter_k() const;
    void setGolombRiceParameter_k(std::uint32_t new_k);

    std::unique_ptr<std::vector<std::size_t>> encodeBitstreamAll();
    std::size_t encodeBitstreamOnChannel(   //
       Writter_s writter,
       sQuadChannelCS::Channel ch);

    std::size_t encodeBitstream(
       Writter_s writter,
       const std::int16_t* q,
       const std::int16_t* r,
       const std::size_t length,
       const std::int16_t* kValues);

    void pushBit(Writter_s writter, std::uint32_t bit);
    void pushHeader(Writter_s writter, std::uint64_t header);
    void pushBit_1(Writter_s writter);
    void pushBit_0(Writter_s writter);
    void flushBitstream(Writter_s writter);

    void dumpAbsToFile(const char* fileName);
    void dumpQuotientToFile(const char* fileName);
    void dumpRemainderToFile(const char* fileName);
    void dumpDpcmToFile(const char* fileName);
};
