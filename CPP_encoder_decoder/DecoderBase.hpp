#pragma once

#include "globalDefines.hpp"

#include <cstdint>
#include <memory>
#include <vector>

#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef INCLUDE_OPENCL
#    define CL_TARGET_OPENCL_VERSION 200
#    include "OpenCL_sources/opencl_platforms.hpp"
#    include <CL/cl.h>
#    include <emmintrin.h>   // For SIMD operations
#    include <immintrin.h>   // For SIMD operations

#endif

#ifdef TIMING_EN
#    include <chrono>
#endif

void createMissingDirectory(const char* folder_out);

typedef uint32_t STATUS_t;
// Define a macro to check the return value of a function
#define RETURN_ON_FAILURE(func)      \
    do {                             \
        STATUS_t returnValue = func; \
        if(returnValue > 0) {        \
            return returnValue;      \
        }                            \
    } while(0);

enum
{
    BASE_SUCCESS                      = 0,
    BASE_ERROR                        = 1,
    BASE_DIVISION_ERROR               = 2,
    BASE_ERROR_ALL_BYTES_ALREADY_READ = 3,
    BASE_CANNOT_OPEN_INPUT_FILE       = 4,
    BASE_CANNOT_OPEN_OUTPUT_FILE      = 5,
    BASE_OUTPUT_BUFFER_FALSE_SIZE     = 6,
    BASE_ERROR_HEADER_NOT_ALIGNED     = 10,
    BASE_ERROR_HEADER_DATA_INVALID    = 11,
    BASE_OPENCL_ERROR                 = 12,
};

/*
* Struct that manages bitstream reading. It keeps internal state of bitstream pointer.
*/
struct Reader {
    // const std::vector<std::uint8_t>& m_bitStream;
    const std::uint8_t* m_bitStream = nullptr;
    std::size_t m_bitStreamSize     = 0;
    std::size_t m_bitsReadFromByte  = 0;
    std::size_t m_byteIdx           = 0;
    std::uint8_t m_byte             = 0;

    alignas(16) std::uint32_t m_bits_single_bfr[32][4]  = {0};
    alignas(16) std::uint32_t m_bits_shifted_bfr[32][4] = {0};
    /*
    * Reader constructor.
    */
    Reader(const std::uint8_t* bitStream, const std::size_t bitStreamSize);

    /*
    * Fetches single bit.
    */
    static std::uint32_t fetchBit_impl(
       const std::uint8_t* bitStream,
       std::size_t bitStream_size,
       std::size_t& bitsReadFromByte,
       std::size_t& byteIdx,
       std::uint8_t& byte);

    /*
    * Fetches 8 bytes from bitstream[byteOffset:byteOffset+8].
    */
    static std::uint64_t fetch8bytes(   //
       const std::uint8_t* bitStream,
       std::size_t byteOffset);

    /*
    * Loads first byte from bitstream to m_byte and increments bitstream pointer by 1 byte.
    */
    void loadFirstByte(const std::uint8_t* bitStream, std::size_t& byteIdx, std::uint8_t& byte);

    /**
     * Loads first 4 bytes from bitstream and increments bitstream pointer by 4 bytes.
     */
    void loadFirstFourBytes(
       const std::uint8_t* bitStream,
       std::size_t& byteIdx,
       std::uint32_t (*bits_single_bfr)[4],
       std::uint32_t (*bits_shifted_bfr)[4]);

    /*
    * Loads first byte from bitstream member object.
    */
    void loadFirstByte()
    {
        loadFirstByte(m_bitStream, m_byteIdx, m_byte);
    }

    /**
     * Loads first 4 bytes from bitstream member object.
     */
    void loadFirstFourBytes()
    {
        loadFirstFourBytes(m_bitStream, m_byteIdx, m_bits_single_bfr, m_bits_shifted_bfr);
    }

    std::uint32_t fetchBit()
    {
        return Reader::fetchBit_impl(m_bitStream, m_bitStreamSize, m_bitsReadFromByte, m_byteIdx, m_byte);
    }

    /*
    * Gets timestamp from bitstream[0:7]
    */
    static std::uint64_t getTimestamp(const std::uint8_t* bitStream)
    {
        return Reader::fetch8bytes(bitStream, 0);
    }

    /*
    * Gets roi header from bitstream[8:15]
    */
    static std::uint64_t getRoiHeader(const std::uint8_t* bitStream)
    {
        return Reader::fetch8bytes(bitStream, 8);
    }

    /*
    * Gets roi header from bitstream[8:15]
    */
    static std::uint64_t getOmlsHeader(const std::uint8_t* bitStream)
    {
#ifdef ROI_NOT_INCLUDED
        return Reader::fetch8bytes(bitStream, 8);
#else
        return Reader::fetch8bytes(bitStream, 16);
#endif
    }

    /*
    * Gets timestamp from bitstream[0:7] and header from bitstream[8:15]
    */
    static STATUS_t getTimestampAndCompressionInfoFromHeader(
       const std::uint8_t* bitStream,
       std::uint64_t& timestamp,
       std::uint64_t& roi,
       std::uint16_t& width,
       std::uint16_t& height,
       std::uint8_t& unaryMaxWidth,
       std::uint8_t& bpp,
       std::uint8_t& lossyBits,
       std::uint8_t& reserved);
};

/**
     * Translates to MSB first and shifts to get actual value. This function will not be needed if remainders are encoded in MSB first.
     */
inline std::uint32_t getMSBfirst(std::uint32_t lsb, std::size_t validBits)
{
    std::uint32_t msb = 0;
    for(std::size_t i = 0; i < validBits; i++) {
        msb |= ((lsb >> i) & 1) << (validBits - 1 - i);
    }
    // msb >>= (16 - validBits);
    return msb;
}

/**
     * Inverts byte order. Used to calculate how much time could be saved if byte order is changed on FPGA.
     */
inline void reverseByteOrder_16bytes(const std::uint8_t* bitStream, std::uint8_t* dest);

struct DecoderBase {

    /**
 * @param width_a and @param height_a are full image width and height.
 * BayerCFA image.
 */
    template<typename T>
    static STATUS_t decodeBitstreamParallel_actual(
       std::size_t width_a,
       std::size_t height_a,
       std::size_t lossyBits_a,
       std::size_t unaryMaxWidth_a,
       std::size_t bpp_a,
       const std::uint8_t* bitStream,
       const std::size_t bitStreamSize,
       T* bayerGB,
       std::size_t bayerGBSize)
    {
        std::uint32_t N_threshold = 8;
        std::uint32_t A_init      = 32;

        Reader reader{bitStream, bitStreamSize};

        std::size_t width;
        std::size_t height;
        std::size_t lossyBits;
        std::size_t unaryMaxWidth;
        unaryMaxWidth        = unaryMaxWidth_a;
        width                = width_a / 2;
        height               = height_a / 2;
        lossyBits            = lossyBits_a;
        unaryMaxWidth        = unaryMaxWidth_a;
        std::uint32_t k_seed = bpp_a + 3;   // max 12 BPP + 3 = 15

        // Check if full decompressed image buffer is large enough. Multply by 4 because there are Gb,B,R & Gr channels.
        if(2 * width * 2 * height != bayerGBSize) {
            fprintf(
               stdout,
               "DecoderBase: expected size of output buffer: %zu, actual size: %zu (bpp: %zu)\n",
               2 * width * 2 * height,
               bayerGBSize,
               bpp_a);
            return BASE_OUTPUT_BUFFER_FALSE_SIZE;
        }

        reader.loadFirstByte();

        std::uint32_t A[]        = {A_init, A_init, A_init, A_init};
        std::uint32_t N          = N_START;
        std::int16_t YCCC[]      = {0, 0, 0, 0};
        std::int16_t YCCC_prev[] = {0, 0, 0, 0};
        std::int16_t YCCC_up[]   = {0, 0, 0, 0};

        // 1.) decode 4 seed pixels
        // Seed pixel
        std::uint16_t posValue[] = {0, 0, 0, 0};
        for(std::size_t ch = 0; ch < 4; ch++) {
            // for(std::uint32_t n = k_SEED + 1; n > 0; n--) { // MSB first
            //     std::uint32_t lastBit = fetchBit(reader);
            //     posValue[ch]          = (posValue[ch] << 1) | lastBit;
            // }
            (void)reader.fetchBit();   // read delimiter
            for(std::uint32_t n = 0; n < k_seed; n++) {   // LSB first
                std::uint32_t lastBit = reader.fetchBit();
                posValue[ch]          = posValue[ch] | ((std::uint16_t)lastBit << n);
            };
            YCCC[ch] = DecoderBase::fromAbs(posValue[ch]);
        }

        for(std::size_t ch = 0; ch < 4; ch++) {   //
            A[ch] += YCCC[ch] > 0 ? YCCC[ch] : -YCCC[ch];
        }

        RETURN_ON_FAILURE(DecoderBase::YCCC_to_BayerGB<T>(
           YCCC[0],
           YCCC[1],
           YCCC[2],
           YCCC[3],
           bayerGB[0],
           bayerGB[1],
           bayerGB[2 * width],
           bayerGB[2 * width + 1],
           lossyBits))

        memcpy(YCCC_up, YCCC, sizeof(YCCC));
        memcpy(YCCC_prev, YCCC, sizeof(YCCC));

        for(std::size_t idx = 1; idx < height * width; idx++) {

            // 2.) AGOR
            std::uint16_t quotient[]  = {0, 0, 0, 0};
            std::uint16_t remainder[] = {0, 0, 0, 0};
            std::uint16_t k[]         = {0, 0, 0, 0};
            std::int16_t dpcm_curr[]  = {0, 0, 0, 0};

            for(std::size_t ch = 0; ch < 4; ch++) {
                // for(std::size_t it = 0; it < 10; it++) {
                for(std::size_t it = 0; it < (bpp_a + 2); it++) {
                    if((N << k[ch]) < A[ch]) {
                        k[ch] = k[ch] + 1;
                    } else {
                        k[ch] = k[ch];
                    }
                }
                std::uint32_t lastBit;

                std::uint16_t absVal = 0;
                do {   // decode quotient: unary coding
                    lastBit = reader.fetchBit();
                    if(lastBit == 1) {   //
                        quotient[ch]++;
                    }
                } while(lastBit == 1 && quotient[ch] < unaryMaxWidth);

                /* m_unaryMaxWidth * '1' -> indicates binary coding of positive value */
                if(quotient[ch] >= unaryMaxWidth) {
                    // for(std::uint32_t n = 0; n < k_MAX; n++) {   //decode remainder
                    for(std::uint32_t n = 0; n < k_seed; n++) {   //decode remainder
                        lastBit = reader.fetchBit();
                        absVal  = absVal | ((std::uint16_t)lastBit << n); /* LSB first */
                        // absVal  = (absVal << 1) | lastBit; /* MSB first*/
                    };

                } else {
                    for(std::uint32_t n = 0; n < k[ch]; n++) {   //decode remainder
                        lastBit       = reader.fetchBit();
                        remainder[ch] = remainder[ch] | ((std::uint16_t)lastBit << n); /* LSB first*/
                        // remainder[ch] = (remainder[ch] << 1) | lastBit; /* MSB first*/
                    };
                    absVal        = (quotient[ch] << k[ch]) + remainder[ch];
                }
                dpcm_curr[ch] = DecoderBase::fromAbs(absVal);

                A[ch] += dpcm_curr[ch] > 0 ? dpcm_curr[ch] : -dpcm_curr[ch];
                YCCC[ch] = YCCC_prev[ch] + dpcm_curr[ch];
                if(idx % width == 0) {   // when first column pixel, use pixel one row up as a reference instead
                    YCCC[ch]    = YCCC_up[ch] + dpcm_curr[ch];
                    YCCC_up[ch] = YCCC[ch];
                }
                YCCC_prev[ch] = YCCC[ch];   // save pixel to be used as a reference for the next pixel
            }

            std::size_t idxGB = (idx / width) * width * 4 + 2 * (idx % width);
            RETURN_ON_FAILURE(DecoderBase::YCCC_to_BayerGB<T>(
                                 YCCC[0],   //
                                 YCCC[1],
                                 YCCC[2],
                                 YCCC[3],
                                 bayerGB[idxGB],
                                 bayerGB[idxGB + 1],
                                 bayerGB[idxGB + 2 * width],
                                 bayerGB[idxGB + 2 * width + 1],
                                 lossyBits);)
            N += 1;
            if(N >= N_threshold) {
                N >>= 1;
                A[0] >>= 1;
                A[1] >>= 1;
                A[2] >>= 1;
                A[3] >>= 1;
            }
            A[0] = A[0] < A_MIN ? A_MIN : A[0];
            A[1] = A[1] < A_MIN ? A_MIN : A[1];
            A[2] = A[2] < A_MIN ? A_MIN : A[2];
            A[3] = A[3] < A_MIN ? A_MIN : A[3];
        }
        return BASE_SUCCESS;
    }

    /**
 * @param width_a and @param height_a are related to channel size which is one half of the actual BayerCFA image.
 * BayerCFA image.
 */
    STATUS_t decodeBitstreamParallel_pseudo_gpu(
       std::size_t width_a,
       std::size_t height_a,
       std::size_t lossyBits_a,
       std::size_t unaryMaxWidth_a,
       std::size_t bpp_a,
       const std::uint8_t* bitStream,
       const std::size_t bitStreamSize,
       std::uint8_t* bayerGB,
       std::size_t bayerGBSize)
    {

        printf("Pseudo GPU decoding started!\n");

        if(bpp_a != 8) {
            fprintf(
               stdout,
               "DecoderBase: bpp is not 8, but %zu. GPU decompression implemented only for 8 BPP.\n",
               bpp_a);
            return BASE_ERROR;
        }

        std::uint32_t N_threshold = 8;
        std::uint32_t A_init      = 32;

        Reader reader{bitStream, bitStreamSize};

        std::size_t width;
        std::size_t height;
        std::size_t lossyBits;
        std::size_t unaryMaxWidth;
        unaryMaxWidth        = unaryMaxWidth_a;
        width                = width_a / 2;   // half size of the actual BayerCFA image
        height               = height_a / 2;
        lossyBits            = lossyBits_a;
        unaryMaxWidth        = unaryMaxWidth_a;
        std::uint32_t k_seed = bpp_a + 3;   // max 12 BPP + 3 = 15

        // Check if full decompressed image buffer is large enough. Multply by 4 because there are Gb,B,R & Gr channels.
        if(2 * width * 2 * height != bayerGBSize) {
            fprintf(
               stdout,
               "DecoderBase: expected size of output buffer: %zu, actual size: %zu (bpp: %zu)\n",
               2 * width * 2 * height,
               bayerGBSize,
               bpp_a);
            return BASE_OUTPUT_BUFFER_FALSE_SIZE;
        }

        reader.loadFirstByte();

#ifdef TIMING_EN
        std::chrono::steady_clock::time_point begin         = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point begin_parsing = std::chrono::steady_clock::now();
#endif

        std::uint32_t A[] = {A_init, A_init, A_init, A_init};
        std::uint32_t N   = N_START;
        //std::int16_t YCCC[]      = {0, 0, 0, 0};
        std::vector<std::int16_t> YCCC(4 * (height * width));
        std::vector<std::int16_t> YCCC_prev_debug(4 * (height * width));
        std::int16_t YCCC_prev[] = {0, 0, 0, 0};
        //std::int16_t YCCC_up[]   = {0, 0, 0, 0};

        // 1.) decode 4 seed pixels
        // Seed pixel
        std::uint16_t posValue[] = {0, 0, 0, 0};
        for(std::size_t ch = 0; ch < 4; ch++) {
            // for(std::uint32_t n = k_SEED + 1; n > 0; n--) { // MSB first
            //     std::uint32_t lastBit = fetchBit(reader);
            //     posValue[ch]          = (posValue[ch] << 1) | lastBit;
            // }
            (void)reader.fetchBit();   // read delimiter
            for(std::uint32_t n = 0; n < k_seed; n++) {   // LSB first
                std::uint32_t lastBit = reader.fetchBit();
                posValue[ch]          = posValue[ch] | ((std::uint16_t)lastBit << n);
            };
            YCCC[0 + ch] = DecoderBase::fromAbs(posValue[ch]);
        }

        for(std::size_t ch = 0; ch < 4; ch++) {   //
            A[ch] += YCCC[0 + ch] > 0 ? YCCC[0 + ch] : -YCCC[0 + ch];
        }

        RETURN_ON_FAILURE(DecoderBase::YCCC_to_BayerGB(
           YCCC[0 + 0],
           YCCC[0 + 1],
           YCCC[0 + 2],
           YCCC[0 + 3],
           bayerGB[0],
           bayerGB[1],
           bayerGB[2 * width],
           bayerGB[2 * width + 1],
           lossyBits))

        std::vector<std::int16_t> YCCC_dpcm(4 * (height * width));
        //memcpy(YCCC_up, YCCC.data(), sizeof(YCCC_up));
        memcpy(YCCC_prev, YCCC.data(), sizeof(YCCC_prev));

        YCCC_dpcm[0] = YCCC[0];
        YCCC_dpcm[1] = YCCC[1];
        YCCC_dpcm[2] = YCCC[2];
        YCCC_dpcm[3] = YCCC[3];

        // Get DPCM
        for(std::size_t idx = 1; idx < height * width; idx++) {

            // 2.) AGOR
            std::uint16_t quotient[]  = {0, 0, 0, 0};
            std::uint16_t remainder[] = {0, 0, 0, 0};
            std::uint16_t k[]         = {0, 0, 0, 0};
            //std::int16_t dpcm_curr[]  = {0, 0, 0, 0};

            for(std::size_t ch = 0; ch < 4; ch++) {
                for(std::size_t it = 0; it < (bpp_a + 2); it++) {
                    if((N << k[ch]) < A[ch]) {
                        k[ch] = k[ch] + 1;
                    } else {
                        k[ch] = k[ch];
                    }
                }
                std::uint32_t lastBit;

                std::uint16_t absVal = 0;
                do {   // decode quotient: unary coding
                    lastBit = reader.fetchBit();
                    if(lastBit == 1) {   //
                        quotient[ch]++;
                    }
                } while(lastBit == 1 && quotient[ch] < unaryMaxWidth);

                /* m_unaryMaxWidth * '1' -> indicates binary coding of positive value */
                if(quotient[ch] >= unaryMaxWidth) {
                    for(std::uint32_t n = 0; n < k_seed; n++) {   //decode remainder
                        lastBit = reader.fetchBit();
                        absVal  = absVal | ((std::uint16_t)lastBit << n); /* LSB first */
                        // absVal  = (absVal << 1) | lastBit; /* MSB first*/
                    };
                } else {
                    for(std::uint32_t n = 0; n < k[ch]; n++) {   //decode remainder
                        lastBit       = reader.fetchBit();
                        remainder[ch] = remainder[ch] | ((std::uint16_t)lastBit << n); /* LSB first*/
                        // remainder[ch] = (remainder[ch] << 1) | lastBit; /* MSB first*/
                    };
                    absVal = quotient[ch] * (1 << k[ch]) + remainder[ch];
                }

                // TODO: OPPORTUNITY: unsigned absVal to signed dpcm can also be done in parallel
                YCCC_dpcm[4 * idx + ch] = DecoderBase::fromAbs(absVal);
            }

            A[0] += YCCC_dpcm[4 * idx + 0] > 0 ? YCCC_dpcm[4 * idx + 0] : -YCCC_dpcm[4 * idx + 0];
            A[1] += YCCC_dpcm[4 * idx + 1] > 0 ? YCCC_dpcm[4 * idx + 1] : -YCCC_dpcm[4 * idx + 1];
            A[2] += YCCC_dpcm[4 * idx + 2] > 0 ? YCCC_dpcm[4 * idx + 2] : -YCCC_dpcm[4 * idx + 2];
            A[3] += YCCC_dpcm[4 * idx + 3] > 0 ? YCCC_dpcm[4 * idx + 3] : -YCCC_dpcm[4 * idx + 3];

            N += 1;
            if(N >= N_threshold) {
                N >>= 1;
                A[0] >>= 1;
                A[1] >>= 1;
                A[2] >>= 1;
                A[3] >>= 1;
            }
            A[0] = A[0] < A_MIN ? A_MIN : A[0];
            A[1] = A[1] < A_MIN ? A_MIN : A[1];
            A[2] = A[2] < A_MIN ? A_MIN : A[2];
            A[3] = A[3] < A_MIN ? A_MIN : A[3];
        }

#ifdef TIMING_EN
        std::chrono::steady_clock::time_point end_parsing = std::chrono::steady_clock::now();
#endif

        // calculate all pixels in first column
        // CALCULATE ALL YCCC from DPCM
        for(std::size_t row = 1; row < height; row++) {
            auto idx_curr =
               row *
               (4 *
                width);   // 4*width (because there are 4 channels of each width (which is half of the actual image width))
            auto idx_prev = (row - 1) * (4 * width);

            YCCC_prev_debug[idx_curr + 0] = YCCC[idx_prev + 0];
            YCCC_prev_debug[idx_curr + 1] = YCCC[idx_prev + 1];
            YCCC_prev_debug[idx_curr + 2] = YCCC[idx_prev + 2];
            YCCC_prev_debug[idx_curr + 3] = YCCC[idx_prev + 3];
            YCCC[idx_curr + 0]            = YCCC[idx_prev + 0] + YCCC_dpcm[idx_curr + 0];
            YCCC[idx_curr + 1]            = YCCC[idx_prev + 1] + YCCC_dpcm[idx_curr + 1];
            YCCC[idx_curr + 2]            = YCCC[idx_prev + 2] + YCCC_dpcm[idx_curr + 2];
            YCCC[idx_curr + 3]            = YCCC[idx_prev + 3] + YCCC_dpcm[idx_curr + 3];
        }

#ifdef TIMING_EN
        std::chrono::steady_clock::time_point begin_dpcm_to_yccc = std::chrono::steady_clock::now();
#endif

        // calculate all pixels in each row
        for(std::size_t row = 0; row < height; row++) {

            auto row_offset = row * (4 * width);
            for(std::size_t col = 1; col < width; col++) {
                auto idx_curr                 = row_offset + 4 * col;
                auto idx_prev                 = row_offset + 4 * (col - 1);
                YCCC_prev_debug[idx_curr + 0] = YCCC[idx_prev + 0];
                YCCC_prev_debug[idx_curr + 1] = YCCC[idx_prev + 1];
                YCCC_prev_debug[idx_curr + 2] = YCCC[idx_prev + 2];
                YCCC_prev_debug[idx_curr + 3] = YCCC[idx_prev + 3];
                YCCC[idx_curr + 0]            = YCCC[idx_prev + 0] + YCCC_dpcm[idx_curr + 0];
                YCCC[idx_curr + 1]            = YCCC[idx_prev + 1] + YCCC_dpcm[idx_curr + 1];
                YCCC[idx_curr + 2]            = YCCC[idx_prev + 2] + YCCC_dpcm[idx_curr + 2];
                YCCC[idx_curr + 3]            = YCCC[idx_prev + 3] + YCCC_dpcm[idx_curr + 3];
            }
        }

#ifdef TIMING_EN
        std::chrono::steady_clock::time_point end_dpcm_to_yccc    = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point begin_yccc_to_bayer = std::chrono::steady_clock::now();
#endif

        // TODO: OPPORTUNITY: can be done in parallel
        for(std::size_t idx = 1; idx < height * width; idx++) {
            std::size_t idxGB = (idx / width) * width * 4 + 2 * (idx % width);
            RETURN_ON_FAILURE(DecoderBase::YCCC_to_BayerGB(
                                 YCCC[4 * idx + 0],   //
                                 YCCC[4 * idx + 1],
                                 YCCC[4 * idx + 2],
                                 YCCC[4 * idx + 3],
                                 bayerGB[idxGB],
                                 bayerGB[idxGB + 1],
                                 bayerGB[idxGB + 2 * width],
                                 bayerGB[idxGB + 2 * width + 1],
                                 lossyBits);)
        }

#ifdef TIMING_EN
        std::chrono::steady_clock::time_point end_yccc_to_bayer = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point end               = std::chrono::steady_clock::now();
        std::cout << "Pseudo GPU: Parallel decoding time = "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;
        std::cout << "Pseudo GPU: Parsing time = "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end_parsing - begin_parsing).count()
                  << "[ms]" << std::endl;
        std::cout
           << "Pseudo GPU: DPCM to YCCC time = "
           << std::chrono::duration_cast<std::chrono::milliseconds>(end_dpcm_to_yccc - begin_dpcm_to_yccc).count()
           << "[ms]" << std::endl;
        std::cout
           << "Pseudo GPU: YCCC to Bayer time = "
           << std::chrono::duration_cast<std::chrono::milliseconds>(end_yccc_to_bayer - begin_yccc_to_bayer).count()
           << "[ms]" << std::endl;
        std::cout << "Pseudo GPU: Total decoding time = "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;
        std::cout << "Pseudo GPU decoding done!" << std::endl;
        std::cout << std::endl;
#endif

        printf("Pseudo GPU decoding done!\n");
        return BASE_SUCCESS;
    }

    // template<typename T>
    static STATUS_t exportImage(
       const char* fileName,
       //    std::uint16_t* data,
       std::uint8_t* data,
       std::size_t data_size_bytes,
       std::uint64_t header,
       std::uint64_t roi,
       std::uint64_t timestamp)
    {
        std::ofstream wf(fileName, std::ios::out | std::ios::binary);
        if(!wf) {
            return BASE_CANNOT_OPEN_OUTPUT_FILE;
        }

        if(timestamp) {
            wf.write((char*)&timestamp, 8); /* Little endian order */
        }

        if(roi) {
            wf.write((char*)&roi, 8); /* Little endian order */
        }

        if(header) {
            wf.write((char*)&header, 8); /* Little endian order */
        }

        /* Little endian order*/
        wf.write((char*)data, data_size_bytes);

        wf.close();

        return BASE_SUCCESS;
    }

    // template<typename T>
    static STATUS_t exportImage(
       const char* folderName,
       const char* fileName,
       std::uint8_t* data,
       std::size_t data_size_bytes,
       std::uint64_t header,
       std::uint64_t roi,
       std::uint64_t timestamp)
    {
        createMissingDirectory(folderName);
        char path[500];
        sprintf_s(path, "%s/%s", folderName, fileName);

        return DecoderBase::exportImage(path, data, data_size_bytes, header, roi, timestamp);
    }

    static STATUS_t importBitstream(const char* fileName, std::unique_ptr<std::vector<std::uint8_t>>& outData);

    /**
   * Get DPCM value from Absolute value.
   */
    static std::int16_t fromAbs(std::uint16_t absValue);

    /**
     * Transform from YCdCmCo to Bayer GB color space
     *
     * Y  | Cd =>  Gb | Bl
     * ---+--- =>  ---+---
     * Cm | Co =>  Rd | Gr
     *
     */
    template<typename T>
    static STATUS_t YCCC_to_BayerGB(
       const std::int16_t y,
       const std::int16_t cd,
       const std::int16_t cm,
       const std::int16_t co,
       T& gb,
       T& b,
       T& r,
       T& gr,
       std::size_t lossyBits)
    {
        // clang-format off
    //std::int16_t gr_temp = (2 * y +  2 * cd +  4 * cm +  2 * co);
    //std::int16_t r_temp =  (2 * y +  2 * cd + -4 * cm +  2 * co);  
    //std::int16_t b_temp =  (2 * y +  2 * cd + -4 * cm + -6 * co);  
    //std::int16_t gb_temp = (2 * y + -6 * cd +  4 * cm +  2 * co); 
    //
    //std::uint16_t mask = 7 >> lossyBits;
    //if((gr_temp & mask) != 0 ){
    //    return BASE_DIVISION_ERROR;
    //}
    //if((b_temp & mask) != 0 ){
    //    return BASE_DIVISION_ERROR;
    //}
    //if((r_temp & mask) != 0 ){
    //    return BASE_DIVISION_ERROR;
    //}
    //if((gb_temp & mask) != 0 ){
    //    return BASE_DIVISION_ERROR;
    //}

    gr = (T)((std::int16_t)(2 * y +  2 * cd +  4 * cm +  2 * co) >> 3) << lossyBits;   // divide by 8
    r  = (T)((std::int16_t)(2 * y +  2 * cd + -4 * cm +  2 * co) >> 3) << lossyBits;   // divide by 8
    b  = (T)((std::int16_t)(2 * y +  2 * cd + -4 * cm + -6 * co) >> 3) << lossyBits;   // divide by 8
    gb = (T)((std::int16_t)(2 * y + -6 * cd +  4 * cm +  2 * co) >> 3) << lossyBits;   // divide by 8
        // clang-format on
        return BASE_SUCCESS;
    }

    static STATUS_t handleReturnValue(STATUS_t status);
};
