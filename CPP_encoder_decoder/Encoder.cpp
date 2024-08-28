
#include "Encoder.hpp"
#include "Channels.hpp"
#include <bitset>
#include <chrono>

Writter_s::Writter_s(){};
Writter_s::Writter_s(std::ofstream* pWf, std::uint32_t* pBfr, std::size_t* pBitCnt, std::size_t* pBytesCnt)
   : m_pWf(pWf)
   , m_pBfr(pBfr)
   , m_pBitCnt(pBitCnt)
   , m_pBytesCnt(pBytesCnt){};

/**
 * Parallel version. Encode data to binary array. Supply BAYER image.
 */
Encoder::Encoder(
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
   const char* fileName)
   : m_pImg(pImg)
   //    , m_width(pImg->getWidth() / 2)
   //    , m_height(pImg->getHeight() / 2)
   , m_width(width / 2)
   , m_height(height / 2)
   , m_length(m_width * m_height)
   , m_folderOut(folderOut)
   , m_imgIdx(imgIdx)
   , m_A_init(A_init)
   , m_N_threshold(N_threshold)
   , m_lossyBits(lossyBits)
   , m_unaryMaxWidth(unaryMaxWidth)
   , m_bpp(bpp)
   , m_header_bytes(header_bytes)
   , m_fileName(fileName)
   , m_k_seed(bpp + 3)
   , m_k_max(bpp + 2){};

/**
 * Sequential and ideal version. Encode data to binary array. Supply YCCC image.
 */
Encoder::Encoder(
   const ImageYCCC* pImgYCCC,
   const char* folderOut,
   std::size_t imgIdx,
   std::size_t lossyBits,
   std::size_t header_bytes)
   : m_pImgYCCC(pImgYCCC)
   , m_width(pImgYCCC->getWidth())
   , m_height(pImgYCCC->getHeight())
   , m_length(m_width * m_height)
   , m_kValues(m_length)
   , m_dpcm(m_length)
   , m_abs(m_length)
   , m_quotient(m_length)
   , m_remainder(m_length)
   , m_folderOut(folderOut)
   , m_imgIdx(imgIdx)
   , m_lossyBits(lossyBits)
   , m_header_bytes(header_bytes){};

/**
 * 
*/
std::unique_ptr<std::vector<std::size_t>> Encoder::encodeUsingMethod(Encoder::method method)
{
    std::cout << "Encoding: " << unsigned(m_width) << " x " << unsigned(m_height) << std::endl;
    switch(method) {
        /* Encode by calculating dpcm using diffUp method, then encode the single seed in two's complement.
        * Sequental encoding (CH1, CH1, CH1,... CH2, CH2, CH2,... CH3, CH3, CH3,... , CH4, CH4, CH4,...)
        * Reset statistics (A and N) when going to new row.*/
        case Encoder::method::singleSeedInTwos:
            if(m_pImgYCCC) {
                differentiateAll(Encoder::algorithm::diffUp);
                adaptiveGolombRiceAll(Encoder::algorithm::singleSeedInTwos);
                return encodeBitstreamAll();
            } else {
                throw std::runtime_error("encodeUsingMethod(singleSeedInTwos): m_pImgYCCC was not initilised through "
                                         "proper Encoder constructor.");
            }

        /* Uses ideal rule for k calculation. BPP close to entropy limit. Reconstruction not possible.*/
        case Encoder::method::ideal:
            if(m_pImgYCCC) {
                m_idealRule = 1;
                differentiateAll(Encoder::algorithm::diffUp);
                adaptiveGolombRiceAll(Encoder::algorithm::ideal);
                return encodeBitstreamAll();
            } else {
                throw std::runtime_error(
                   "encodeUsingMethod(ideal): m_pImgYCCC was not initilised through proper Encoder constructor.");
            }

        /* Encode all 4 channels simultaneously with single seed (diff up method)*/
        case Encoder::method::parallel_standard:
            if(m_unaryMaxWidth != C_MAX_UNARY_LENGTH_FULL) {
                throw std::runtime_error(
                   "encodeUsingMethod(parallel_standard): You wanted to use standard unary encoding but it seems that "
                   "you have set the unaryMaxWidth parameter.");
            }
            if(m_pImg) {
                return runParallelCompression();
            } else {
                throw std::runtime_error(
                   "encodeUsingMethod(): m_pImg was not initilised through proper Encoder constructor.");
            }
        /* Encode all 4 channels simultaneously with single seed (diff up method). Limit maximal allowed length of compressed data.*/
        case Encoder::method::parallel_limited:
            if(m_unaryMaxWidth == C_MAX_UNARY_LENGTH_FULL) {
                throw std::runtime_error(
                   "encodeUsingMethod(parallel_limited): You wanted to use limited unary encoding but it seems that "
                   "you have set the unaryMaxWidth parameter to maximum value (no limiting).");
            }
            if(m_pImg) {
                return runParallelCompression();
            } else {
                throw std::runtime_error(
                   "encodeUsingMethod(): m_pImg was not initilised through proper Encoder constructor.");
            }
        default:
            throw std::runtime_error("Invalid method!");
            break;
    }
}

/**
 * Simulates receiving pixels from image sensor. Keeps track of current pixel. 
 * Immediately encodes it and writes it to a bitstream.
*/
std::unique_ptr<std::vector<std::size_t>> Encoder::runParallelCompression()
{
    char txt[200];
    sprintf(
       txt,
       "Parallel encoding with params: imgIdx: %zu, unaryMaxWidth: %zu, A_init: %u, N_threshold: %u\n",
       m_imgIdx,
       m_unaryMaxWidth,
       m_A_init,
       m_N_threshold);
    std::cout << txt;

#ifdef DUMP_VERIFICATION
    char outputFile[200];
    sprintf(
       outputFile,
       "%s/dump/%s%02zu_BAYER.txt",
       m_folderOut,
       m_fileName,
       m_imgIdx);
    std::ofstream wf(outputFile, std::ios::out);
    if(!wf) {
        char msg[200];
        sprintf(msg, "Cannot open specified file: %s", outputFile);
        throw std::runtime_error(msg);
    }
    sprintf(txt, "%zu %zu\n", m_width, m_height);
    wf << txt;
#endif

    auto imageData = m_pImg->getDataView();
    for(std::size_t i = 0; i < m_pImg->getHeight() / 2; i++) {
        for(std::size_t j = 0; j < m_pImg->getWidth() / 2; j++) {
            std::size_t idxGB = 2 * i * m_pImg->getWidth() + 2 * j;
            std::size_t idxB  = 2 * i * m_pImg->getWidth() + 2 * j + 1;
            std::size_t idxR  = (2 * i + 1) * m_pImg->getWidth() + 2 * j;
            std::size_t idxGR = (2 * i + 1) * m_pImg->getWidth() + 2 * j + 1;
#ifdef DUMP_VERIFICATION
            m_row = i;
            m_col = j;
#endif
            encodeParallel(imageData[idxGB], imageData[idxB], imageData[idxR], imageData[idxGR]);
#ifdef DUMP_VERIFICATION
            sprintf(
               txt,
               "%4zu %4zu : %3u %3u %3u %3u\n",
               i,
               j,
               imageData[idxGB],
               imageData[idxB],
               imageData[idxR],
               imageData[idxGR]);
            wf << txt;
#endif
        }
    }

#ifdef DUMP_VERIFICATION
    wf.close();
#endif
    std::vector<std::size_t> bytesWritten(1);
    bytesWritten[0] = getFileSize();
    return std::make_unique<std::vector<std::size_t>>(std::forward<std::vector<std::size_t>>(bytesWritten));
}

/**
 * Encodes all 4 channels in parallel (CH1,CH2,CH3,CH4,CH1,CH2,CH3,CH4,CH1,CH2,CH3,CH4,...).
 * Instead of sequental encoding (CH1, CH1, CH1,... CH2, CH2, CH2,... CH3, CH3, CH3,... , CH4, CH4, CH4,...)
 * Input: BayerCFA
 * Output: Encoded bitstream to data file.
 * for each pair of rows
 * 1.) Buffer last 2 rows to calculate YCCC squares.
 *  2a.) First square is seed for all 4 channels. 
 *  2b.) Save pixelUp for diffUp encoding of first elements in a row.
 *  2c.) Calculate difference (dpcm) from previous neighbours.
 *  3.) Calculate positive odd/even value.
 *  4.) Calculate quotients and remainders using AGOR.
 *  5.) add abs(dpcm) to A of respective channel. Increase N++ of respective channel.
 *  6.) Encode q & r for each channel in parallel
 * 7.) fill last byte bitstream with 1s
 * 
*/
std::size_t Encoder::encodeParallel(std::uint16_t gb, std::uint16_t b, std::uint16_t r, std::uint16_t gr)
{
    static std::size_t idx = 0;
    static std::uint32_t bfr;
    static std::size_t bitCnt;
    static std::size_t bytesCnt;
    static std::ofstream wf;
    static Writter_s writter;
    static int16_t YCCC_prev[4];
    static int16_t YCCC_up[4];
    static std::uint32_t A[4];
    static std::uint32_t N;

    if(idx == 0) {
        // open new file
        bfr        = 0;
        bitCnt     = 0;
        bytesCnt   = 0;
        m_fileSize = 0;
        N          = N_START;

        char path[200];
        sprintf(path, "%s/compressed/%s%02zu.bin", m_folderOut, m_fileName, m_imgIdx);
        wf.open((const char*)path, std::ios::out | std::ios::binary);
        if(!wf) {
            char msg[200];
            sprintf(msg, "Cannot open specified file: %s", path);
            throw std::runtime_error(msg);
        }

        writter.m_pWf       = &wf;
        writter.m_pBfr      = &bfr;
        writter.m_pBitCnt   = &bitCnt;
        writter.m_pBytesCnt = &bytesCnt;

        // if header_bytes = 8, then write 8 byte timestamp to the beginning
        if(m_header_bytes == 8) {

            /* Timestamp: */
            // MSB                                                                          LSB
            // 64                 48                  32                  16                   0
            // | 8 bytes ' 8 bytes | 8 bytes ' 8 bytes | 8 bytes ' 8 bytes | 8 bytes ' 8 bytes |
            // address : content
            //       0 : timestampLSB
            //       7 : timestampMSB

            // std::uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            //                              std::chrono::system_clock::now().time_since_epoch())
            //                              .count();

            std::uint8_t reservedBits      = 0;
            std::uint64_t compression_info =   //
               0LLU |   //
               ((std::uint64_t)((std::uint8_t)reservedBits)) << 56 |   //
               ((std::uint64_t)((std::uint8_t)m_lossyBits)) << 48 |   //
               ((std::uint64_t)((std::uint8_t)m_bpp)) << 40 |   //
               ((std::uint64_t)((std::uint8_t)m_unaryMaxWidth)) << 32 |   //
               ((std::uint64_t)((std::uint16_t)2 * m_height)) << 16 |   //
               ((std::uint64_t)((std::uint16_t)2 * m_width)) << 0;
            // clang-format on
            for(std::size_t s = 0; s < 64; s += 8) {
                printf("Byte %zu: %02X\n", s / 8, (uint8_t)(compression_info >> s));
            }
            printf("Compression data: 0x%08llX\n", compression_info);

            pushHeader(writter, compression_info);

        } else if(m_header_bytes == 16) {
            std::uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::system_clock::now().time_since_epoch())
                                         .count();

            pushHeader(writter, timestamp);

            std::cout << "Compression Image timestamp: " << timestamp << std::endl;

            /* Compression info: */
            //  MSB                                                                          LSB
            // 64                 48                  32                  16                   0
            // | 8 bytes ' 8 bytes | 8 bytes ' 8 bytes | 8 bytes ' 8 bytes | 8 bytes ' 8 bytes |
            // | Reservd ' losyBts | unary H ' unary L | heigh H ' heigh L | width H ' width L |
            // address : content
            //       0 : width L
            //       1 : width H
            //       2 : height L
            //       3 : height H
            //       4 : unary L
            //       5 : unary H
            //       6 : lossy bits
            //       7 : reserved

            std::uint8_t reservedBits = 0;
            // clang-format off
            std::uint64_t compression_info =   //
               0LLU                                                    |   //
               ((std::uint64_t)((std::uint8_t)reservedBits))     << 56 |   //
               ((std::uint64_t)((std::uint8_t)m_lossyBits))      << 48 |   //
               ((std::uint64_t)((std::uint8_t)m_bpp))            << 40 |   //
               ((std::uint64_t)((std::uint8_t)m_unaryMaxWidth))  << 32 |   //
               ((std::uint64_t)((std::uint16_t)2*m_height))      << 16 |   //
               ((std::uint64_t)((std::uint16_t)2*m_width))       << 0;
            // clang-format on
            for(std::size_t s = 0; s < 64; s += 8) {
                printf("Byte %zu: %02X\n", s / 8, (uint8_t)(compression_info >> s));
            }
            printf("Compression data: 0x%08llX\n", compression_info);

            pushHeader(writter, compression_info);
            // wf.write((const char*)&compression_info, sizeof(compression_info));
            // (*writter.m_pBytesCnt) += sizeof(compression_info);
        } else if(m_header_bytes == 24) {
            std::uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::system_clock::now().time_since_epoch())
                                         .count();

            pushHeader(writter, timestamp);

            std::cout << "Compression Image timestamp: " << timestamp << std::endl;
            std::uint64_t roi      = 0;
            std::uint16_t offset_y = 0;
            std::uint16_t offset_x = 0;
            roi = (2 * m_height & 0xFFFF) << 48 | (2 * m_width & 0xFFFF) << 32 | offset_y << 16 | offset_x;
            pushHeader(writter, roi);

            /* Compression info: */
            //  MSB                                                                          LSB
            // 64                 48                  32                  16                   0
            // | 8 bytes ' 8 bytes | 8 bytes ' 8 bytes | 8 bytes ' 8 bytes | 8 bytes ' 8 bytes |
            // | Reservd ' losyBts | unary H ' unary L | heigh H ' heigh L | width H ' width L |
            // address : content
            //       0 : width L
            //       1 : width H
            //       2 : height L
            //       3 : height H
            //       4 : unary L
            //       5 : unary H
            //       6 : lossy bits
            //       7 : reserved

            std::uint8_t reservedBits = 0;
            // clang-format off
            std::uint64_t compression_info =   //
               0LLU                                                    |   //
               ((std::uint64_t)((std::uint8_t)reservedBits))     << 56 |   //
               ((std::uint64_t)((std::uint8_t)m_lossyBits))      << 48 |   //
               ((std::uint64_t)((std::uint8_t)m_bpp))            << 40 |   //
               ((std::uint64_t)((std::uint8_t)m_unaryMaxWidth))  << 32 |   //
               ((std::uint64_t)((std::uint16_t)2*m_height))      << 16 |   //
               ((std::uint64_t)((std::uint16_t)2*m_width))       << 0;
            // clang-format on
            for(std::size_t s = 0; s < 64; s += 8) {
                printf("Byte %zu: %02X\n", s / 8, (uint8_t)(compression_info >> s));
            }
            printf("Compression data: 0x%08llX\n", compression_info);

            pushHeader(writter, compression_info);

        }

        else {
            printf("No header will be added to the compressed file.\n");
        }

        for(std::size_t ch = 0; ch < 4; ch++) {
            YCCC_prev[ch] = 0;
            A[ch]         = m_A_init;
        }
        encodeParallelOneQuadrupleSeedPixel(gb, b, r, gr, YCCC_prev, A, writter);
        memcpy(YCCC_up, YCCC_prev, sizeof(YCCC_prev));   // Copy first pixel of a row to YCCC_prev.
        idx++;

    } else if(idx % m_width == 0) {   // new row
        // provide YCCC_up pixel value. YCCC_up is then updated with new value.
        encodeParallelOneQuadruple(gb, b, r, gr, YCCC_up, A, N, m_N_threshold, 0, writter);
        memcpy(YCCC_prev, YCCC_up, sizeof(YCCC_prev));   // Copy first pixel of a row to YCCC_prev.
        idx++;
    } else if(idx == (m_length - 4)) {   // fourth-to-last pixel
        encodeParallelOneQuadruple(gb, b, r, gr, YCCC_prev, A, N, m_N_threshold, 1, writter);
        idx++;
    } else if(idx == (m_length - 3)) {   // third-to-last pixel
        encodeParallelOneQuadruple(gb, b, r, gr, YCCC_prev, A, N, m_N_threshold, 1, writter);
        idx++;
    } else if(idx == (m_length - 2)) {   // second-to-last pixel
        encodeParallelOneQuadruple(gb, b, r, gr, YCCC_prev, A, N, m_N_threshold, 1, writter);
        idx++;
    } else if(idx == (m_length - 1)) {   // last pixel
        encodeParallelOneQuadruple(gb, b, r, gr, YCCC_prev, A, N, m_N_threshold, 1, writter);
        flushBitstream(writter);
        wf.close();
        idx        = 0;
        m_fileSize = *writter.m_pBytesCnt;
    } else {
        encodeParallelOneQuadruple(gb, b, r, gr, YCCC_prev, A, N, m_N_threshold, 0, writter);
        idx++;
    }

    return bytesCnt;
}

/**
 * YCCC_prev is updated with current YCCC values.
*/
void Encoder::encodeParallelOneQuadrupleSeedPixel(
   std::uint16_t gb,
   std::uint16_t b,
   std::uint16_t r,
   std::uint16_t gr,
   std::int16_t* YCCC_prev,
   std::uint32_t* A,
   Writter_s writter)
{
    std::int16_t YCCC[] = {0, 0, 0, 0};

    // 1.) BayerCFA to YCCC
    Helpers::transformColorGB(
       gb,   //
       b,   //
       r,   //
       gr,   //
       &YCCC[0],   //
       &YCCC[1],   //
       &YCCC[2],   //
       &YCCC[3],
       m_lossyBits);   //

#ifdef DUMP_VERIFICATION
    {
        char outputFile[200];
        sprintf(
           outputFile,
           "%s/dump/%s%02zu_dpcm.txt",
           m_folderOut,
           m_fileName,
           m_imgIdx);
        std::ofstream wf(outputFile, std::ios::out);
        if(!wf) {
            char msg[200];
            std::sprintf(msg, "Cannot open specified file: %s", outputFile);
            throw std::runtime_error(msg);
        }
        char txt[200];
        std::sprintf(txt, "%zu %zu pred YCCC, curr YCCC, dpcm\n", m_width, m_height);
        wf << txt;
        std::sprintf(
           txt,
           "%4zu %4zu : %.4hX %.4hX %.4hX %.4hX : %.4hX %.4hX %.4hX %.4hX : %.4hX %.4hX %.4hX %.4hX\n",
           m_row,
           m_col,
           0,
           0,
           0,
           0,
           (YCCC[0] & 0x03FF),
           (YCCC[1] & 0x01FF),
           (YCCC[2] & 0x01FF),
           (YCCC[3] & 0x01FF),
           (YCCC[0] & 0x07FF),
           (YCCC[1] & 0x03FF),
           (YCCC[2] & 0x03FF),
           (YCCC[3] & 0x03FF));
        wf << txt;
        wf.close();
    }
#endif
    // std::uint16_t k[] = {k_SEED, k_SEED, k_SEED, k_SEED};
    std::uint16_t k[] = {(uint16_t)m_k_seed, (uint16_t)m_k_seed, (uint16_t)m_k_seed, (uint16_t)m_k_seed};
#ifdef DUMP_VERIFICATION
    {
        char outputFile[200];
        sprintf(
           outputFile,
           "%s/dump/%s%02zu_calc.txt",
           m_folderOut,
           m_fileName,
           m_imgIdx);
        std::ofstream wf(outputFile, std::ios::out);
        if(!wf) {
            char msg[200];
            std::sprintf(msg, "Cannot open specified file: %s", outputFile);
            throw std::runtime_error(msg);
        }
        char txt[200];
        std::sprintf(txt, "%zu %zu average A, k\n", m_width, m_height);
        wf << txt;
        std::sprintf(
           txt,
           //    "%4zu %4zu : %.4hX %.4hX %.4hX %.4hX : %.4hX %.4hX %.4hX %.4hX\n",
           "%4zu %4zu : %4hu %4hu %4hu %4hu : %3hu %3hu %3hu %3hu\n",
           m_row,
           m_col,
           32,
           32,
           32,
           32,
           k[0],
           k[1],
           k[2],
           k[3]);
        wf << txt;
        wf.close();
    }
#endif
    // 4.) AGOR
    std::uint16_t quotient[]  = {0, 0, 0, 0};
    std::uint16_t remainder[] = {0, 0, 0, 0};
#ifdef DUMP_VERIFICATION
    // 3.) To positive value
    std::uint16_t posValue[] = {0, 0, 0, 0};
    for(std::size_t ch = 0; ch < 4; ch++) {   //
        posValue[ch] = (std::uint16_t)toAbsSingle(YCCC[ch]);
    }

    for(std::size_t ch = 0; ch < 4; ch++) {
        quotient[ch]  = posValue[ch] >> k[ch];
        remainder[ch] = posValue[ch] & (std::uint16_t)((1 << k[ch]) - 1);   // modulus op = take last k bits
    }
    {
        char outputFile[200];
        sprintf(
           outputFile,
           "%s/dump/%s%02zu_qr.txt",
           m_folderOut,
           m_fileName,
           m_imgIdx);
        std::ofstream wf(outputFile, std::ios::out);
        if(!wf) {
            char msg[200];
            std::sprintf(msg, "Cannot open specified file: %s", outputFile);
            throw std::runtime_error(msg);
        }
        char txt[200];
        std::sprintf(txt, "%zu %zu posValue, quotient, remainder\n", m_width, m_height);
        wf << txt;
        std::sprintf(
           txt,
           "%4zu %4zu : %4hu %4hu %4hu %4hu : %4hu %4hu %4hu %4hu : %4hu %4hu %4hu %4hu\n",
           m_row,
           m_col,
           posValue[0],
           posValue[1],
           posValue[2],
           posValue[3],
           quotient[0],
           quotient[1],
           quotient[2],
           quotient[3],
           remainder[0],
           remainder[1],
           remainder[2],
           remainder[3]);
        wf << txt;
        wf.close();
    }
    {
        char outputFile[200];
        sprintf(
           outputFile,
           "%s/dump/%s%02zu_qrK.txt",
           m_folderOut,
           m_fileName,
           m_imgIdx);
        std::ofstream wf(outputFile, std::ios::out);
        if(!wf) {
            char msg[200];
            std::sprintf(msg, "Cannot open specified file: %s", outputFile);
            throw std::runtime_error(msg);
        }
        char txt[200];
        std::sprintf(txt, "%zu %zu quotient, remainder, k\n", m_width, m_height);
        wf << txt;
        std::sprintf(
           txt,
           "%4zu %4zu : %4hu %4hu %4hu %4hu : %4hu %4hu %4hu %4hu : %2hu %2hu %2hu %2hu : %1hu\n",
           m_row,
           m_col,
           quotient[0],
           quotient[1],
           quotient[2],
           quotient[3],
           remainder[0],
           remainder[1],
           remainder[2],
           remainder[3],
           k[0],
           k[1],
           k[2],
           k[3],
           0);
        wf << txt;
        wf.close();
    }
    char outputFile[200];
    sprintf(
       outputFile,
       "%s/dump/%s%02zu_codes.txt",
       m_folderOut,
       m_fileName,
       m_imgIdx);
    std::ofstream wf_codes(outputFile, std::ios::out);
    if(!wf_codes) {
        char msg[200];
        std::sprintf(msg, "Cannot open specified file: %s", outputFile);
        throw std::runtime_error(msg);
    }
    char txt[200];
    std::sprintf(txt, "%zu %zu  quotient : remai : k  : len    code\n", m_width, m_height);
    wf_codes << txt;
#endif
    // 6.) Encode
    for(std::size_t ch = 0; ch < 4; ch++) {
#ifdef DUMP_VERIFICATION
        std::size_t code_len =
           ((std::size_t)(quotient[ch] + 1 + k[ch]) < (m_k_seed + m_unaryMaxWidth) ? quotient[ch] + 1 + k[ch]
                                                                                   : (m_k_seed + m_unaryMaxWidth));
        std::sprintf(
           txt,
           "%4zu %4zu : %4hu  : %4hu  : %2hu : %3zu ",
           m_row,
           m_col,
           quotient[ch],
           remainder[ch],
           k[ch],
           code_len);
        wf_codes << txt;
#endif

        // unary coding of quotient
        for(std::uint16_t n = 0; n < quotient[ch]; n++) {   // big endian
            pushBit_1(writter);
#ifdef DUMP_VERIFICATION
            wf_codes << '1';
#endif
        }
        pushBit_0(writter);
#ifdef DUMP_VERIFICATION
        wf_codes << '0';
#endif

        // // remainder coding
        // for(std::int16_t n = k[ch]; n > 0; n--) {   // MSB first
        //     std::uint32_t bit = (remainder[ch] >> (n - 1)) & (std::uint32_t)1;
        //     pushBit(writter, bit);
        // }
        // remainder coding
        for(std::int16_t n = 0; n < k[ch]; n++) {   // LSB first
            std::uint32_t bit = (remainder[ch] >> n) & (std::uint32_t)1;
            pushBit(writter, bit);
#ifdef DUMP_VERIFICATION
            wf_codes << (bit ? '1' : '0');
#endif
        }
#ifdef DUMP_VERIFICATION
        wf_codes << '\n';
#endif
    }
#ifdef DUMP_VERIFICATION
    wf_codes.close();
#endif

    // prepare data for next pixel
    memcpy(YCCC_prev, YCCC, sizeof(YCCC));

    // 2.) to dpcm: same as YCCC; & add abs(dpcm) to A
    for(std::size_t ch = 0; ch < 4; ch++) {   //
        A[ch] += (YCCC[ch] >= 0 ? YCCC[ch] : -YCCC[ch]);
    }
}
/**
 * It is up to the caller to supply correct YCCC_prev values
 * (e.g. when going to new row or when pixel is seed pixel).
 * YCCC_prev is updated with current YCCC values.
*/
void Encoder::encodeParallelOneQuadruple(
   std::uint16_t gb,
   std::uint16_t b,
   std::uint16_t r,
   std::uint16_t gr,
   std::int16_t* YCCC_prev,
   std::uint32_t* A,
   std::uint32_t& N,
   std::uint32_t N_threshold,
   std::uint8_t last,
   Writter_s writter)
{
    std::int16_t YCCC[] = {0, 0, 0, 0};

    // 1.) BayerCFA to YCCC
    Helpers::transformColorGB(
       gb,   //
       b,   //
       r,   //
       gr,   //
       &YCCC[0],   //
       &YCCC[1],   //
       &YCCC[2],   //
       &YCCC[3],
       m_lossyBits);   //

    // 2.) to dpcm: difference between prev pixel and current pixel.
    std::int16_t dpcm[] = {0, 0, 0, 0};
    for(std::size_t ch = 0; ch < 4; ch++) {
        dpcm[ch] = YCCC[ch] - YCCC_prev[ch];   // dX(n) = X(n) - X(n-1)
    }

#ifdef DUMP_VERIFICATION
    {
        static std::ofstream wf_dpcm;
        if((m_row == 0) & (m_col == 1)) {
            char outputFile[200];
            std::sprintf(
               outputFile,
               "%s/dump/%s%02zu_dpcm.txt",
               m_folderOut,
               m_fileName,
               m_imgIdx);
            wf_dpcm.open(outputFile, std::ios::app);
            if(!wf_dpcm) {
                char msg[200];
                std::sprintf(msg, "Cannot open specified file: %s", outputFile);
                throw std::runtime_error(msg);
            }
        }
        char txt[200];

        int16_t mask_Y      = (1 << (m_bpp + 2)) - 1;   // 0x03FF
        int16_t mask_C      = (1 << (m_bpp + 1)) - 1;   // 0x01FF;
        int16_t mask_Y_dpcm = (1 << (m_bpp + 3)) - 1;   // 0x07FF;
        int16_t mask_C_dpcm = (1 << (m_bpp + 2)) - 1;   // 0x03FF;

        std::sprintf(
           txt,
           "%4zu %4zu : %.4hX %.4hX %.4hX %.4hX : %.4hX %.4hX %.4hX %.4hX : %.4hX %.4hX %.4hX %.4hX\n",
           m_row,
           m_col,
           (YCCC_prev[0] & mask_Y),
           (YCCC_prev[1] & mask_C),
           (YCCC_prev[2] & mask_C),
           (YCCC_prev[3] & mask_C),
           (YCCC[0] & mask_Y),
           (YCCC[1] & mask_C),
           (YCCC[2] & mask_C),
           (YCCC[3] & mask_C),
           (dpcm[0] & mask_Y_dpcm),
           (dpcm[1] & mask_C_dpcm),
           (dpcm[2] & mask_C_dpcm),
           (dpcm[3] & mask_C_dpcm));
        wf_dpcm << txt;
        if((m_row == m_height - 1) & (m_col == m_width - 1)) {   //
            wf_dpcm.close();
        };
    }
#endif

    // 3.) To positive value
    std::uint16_t posValue[] = {0, 0, 0, 0};
    for(std::size_t ch = 0; ch < 4; ch++) {   //
        posValue[ch] = (std::uint16_t)toAbsSingle(dpcm[ch]);
    }

    // 4.) AGOR
    std::uint16_t quotient[]  = {0, 0, 0, 0};
    std::uint16_t remainder[] = {0, 0, 0, 0};
    std::uint16_t k[]         = {m_k_min, m_k_min, m_k_min, m_k_min};

    for(std::size_t ch = 0; ch < 4; ch++) {
        for(std::size_t it = m_k_min; it < m_k_max; it++) {
            if((N << k[ch]) < A[ch]) {
                k[ch] = k[ch] + 1;
            } else {
                k[ch] = k[ch];
            }
        }
        quotient[ch]  = posValue[ch] >> k[ch];
        remainder[ch] = posValue[ch] & (std::uint16_t)((1 << k[ch]) - 1);   // modulus op = take last k bits
    }

#ifdef DUMP_VERIFICATION
    {
        static std::ofstream wf_calc;
        if((m_row == 0) & (m_col == 1)) {
            char outputFile[200];
            std::sprintf(
               outputFile,
               "%s/dump/%s%02zu_calc.txt",
               m_folderOut,
               m_fileName,
               m_imgIdx);
            wf_calc.open(outputFile, std::ios::app);
            if(!wf_calc) {
                char msg[200];
                std::sprintf(msg, "Cannot open specified file: %s", outputFile);
                throw std::runtime_error(msg);
            }
        }
        char txt[200];

        std::sprintf(
           txt,
           //    "%4zu %4zu : %.4hX %.4hX %.4hX %.4hX : %.4hX %.4hX %.4hX %.4hX\n",
           "%4zu %4zu : %4hu %4hu %4hu %4hu : %3hu %3hu %3hu %3hu\n",
           m_row,
           m_col,
           A[0],
           A[1],
           A[2],
           A[3],
           k[0],
           k[1],
           k[2],
           k[3]);
        wf_calc << txt;
        if((m_row == m_height - 1) & (m_col == m_width - 1)) {   //
            wf_calc.close();
        }
    }
#endif

#ifdef DUMP_VERIFICATION
    {
        static std::ofstream wf_qr;
        if((m_row == 0) & (m_col == 1)) {
            char outputFile[200];
            std::sprintf(outputFile, "%s/dump/%s%02zu_qr.txt", m_folderOut, m_fileName, m_imgIdx);
            wf_qr.open(outputFile, std::ios::app);
            if(!wf_qr) {
                char msg[200];
                std::sprintf(msg, "Cannot open specified file: %s", outputFile);
                throw std::runtime_error(msg);
            }
        }
        char txt[200];
        std::sprintf(
           txt,
           "%4zu %4zu : %4hu %4hu %4hu %4hu : %4hu %4hu %4hu %4hu : %4hu %4hu %4hu %4hu\n",
           m_row,
           m_col,
           posValue[0],
           posValue[1],
           posValue[2],
           posValue[3],
           quotient[0],
           quotient[1],
           quotient[2],
           quotient[3],
           remainder[0],
           remainder[1],
           remainder[2],
           remainder[3]);
        wf_qr << txt;
        if((m_row == m_height - 1) & (m_col == m_width - 1)) {   //
            wf_qr.close();
        }
    }
    {
        static std::ofstream wf_qr;
        if((m_row == 0) & (m_col == 1)) {
            char outputFile[200];
            std::sprintf(outputFile, "%s/dump/%s%02zu_qrK.txt", m_folderOut, m_fileName, m_imgIdx);
            wf_qr.open(outputFile, std::ios::app);
            if(!wf_qr) {
                char msg[200];
                std::sprintf(msg, "Cannot open specified file: %s", outputFile);
                throw std::runtime_error(msg);
            }
        }
        char txt[200];
        std::sprintf(
           txt,
           "%4zu %4zu : %4hu %4hu %4hu %4hu : %4hu %4hu %4hu %4hu : %2hu %2hu %2hu %2hu : %1hu\n",
           m_row,
           m_col,
           quotient[0],
           quotient[1],
           quotient[2],
           quotient[3],
           remainder[0],
           remainder[1],
           remainder[2],
           remainder[3],
           k[0],
           k[1],
           k[2],
           k[3],
           last);
        wf_qr << txt;
        if((m_row == m_height - 1) & (m_col == m_width - 1)) {   //
            wf_qr.close();
        }
    }
#endif

    // 5.) add abs(dpcm) to A
    for(std::size_t ch = 0; ch < 4; ch++) {   //
        A[ch] += (dpcm[ch] >= 0 ? dpcm[ch] : -dpcm[ch]);
    }

    // 5.) increas N and halve N and A
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

    // 6.) Encode
#ifdef DUMP_VERIFICATION
    static std::ofstream wf_codes;
    if((m_row == 0) & (m_col == 1)) {
        char outputFile[200];
        std::sprintf(outputFile, "%s/dump/%s%02zu_codes.txt", m_folderOut, m_fileName, m_imgIdx);
        wf_codes.open(outputFile, std::ios::app);
        if(!wf_codes) {
            char msg[200];
            std::sprintf(msg, "Cannot open specified file: %s", outputFile);
            throw std::runtime_error(msg);
        }
    }
#endif
    for(std::size_t ch = 0; ch < 4; ch++) {
#ifdef DUMP_VERIFICATION
        char txt[200];
        std::size_t code_len =
           ((std::size_t)(quotient[ch] + 1 + k[ch]) < (m_k_seed + m_unaryMaxWidth) ? quotient[ch] + 1 + k[ch]
                                                                                   : (m_k_seed + m_unaryMaxWidth));
        std::sprintf(
           txt,
           "%4zu %4zu : %4hu  : %4hu  : %2hu : %3zu ",
           m_row,
           m_col,
           quotient[ch],
           remainder[ch],
           k[ch],
           code_len);
        wf_codes << txt;
#endif
        /* check if it fits to GR encoding or should it switch to limited encoding */
        if(quotient[ch] < m_unaryMaxWidth) {
            // unary coding of quotient
            for(std::uint16_t n = 0; n < quotient[ch]; n++) { /* big endian */
                pushBit_1(writter);
#ifdef DUMP_VERIFICATION
                wf_codes << '1';
#endif
            }
            pushBit_0(writter);
#ifdef DUMP_VERIFICATION
            wf_codes << '0';
#endif
            /* remainder coding */
            // for(std::int16_t n = k[ch]; n > 0; n--) {   /* MSB first */
            //     std::uint32_t bit = (remainder[ch] >> (n - 1)) & (std::uint32_t)1;
            //     pushBit(writter, bit);
            // }
            for(std::int16_t n = 0; n < k[ch]; n++) { /* LSB first */
                std::uint32_t bit = (remainder[ch] >> n) & (std::uint32_t)1;
                pushBit(writter, bit);
#ifdef DUMP_VERIFICATION
                wf_codes << (bit ? '1' : '0');
#endif
            }
        } else {
            /* m_unaryMaxWidth * '1' -> indicates binary coding of positive value */
            for(std::uint16_t n = 0; n < m_unaryMaxWidth; n++) {   //
                pushBit_1(writter);
#ifdef DUMP_VERIFICATION
                wf_codes << '1';
#endif
            }
            for(std::uint16_t n = 0; n < m_k_seed; n++) { /* LSB first */
                std::uint32_t bit = (posValue[ch] >> n) & (std::uint32_t)1;
                pushBit(writter, bit);
#ifdef DUMP_VERIFICATION
                wf_codes << (bit ? '1' : '0');
#endif
            }
        }
#ifdef DUMP_VERIFICATION
        wf_codes << '\n';
#endif
    }
#ifdef DUMP_VERIFICATION
    if((m_row == m_height - 1) & (m_col == m_width - 1)) {   //
        wf_codes.close();
    }
#endif
    // prepare data for next pixel
    memcpy(YCCC_prev, YCCC, sizeof(YCCC));
}

/**
 * Translate negative values to odd positive ones on all channels.
 */
void Encoder::toAbsAll()
{
    toAbsAll(&m_dpcm);
};

/**
 * Translate negative values to odd positive ones on all channels.
 */
void Encoder::toAbsAll(const sQuadChannelCS* pIn)
{
    using sQC = sQuadChannelCS::Channel;

    toAbs(pIn->getChannelView(sQC::Y).data(), m_abs.getChannel(sQC::Y).data(), m_length);
    toAbs(pIn->getChannelView(sQC::Cd).data(), m_abs.getChannel(sQC::Cd).data(), m_length);
    toAbs(pIn->getChannelView(sQC::Cm).data(), m_abs.getChannel(sQC::Cm).data(), m_length);
    toAbs(pIn->getChannelView(sQC::Co).data(), m_abs.getChannel(sQC::Co).data(), m_length);

#ifdef TEST_OUT
    {
        // Test 2.1 encoder abs
        using namespace std;
        cout << '\n' << "------ Test 2.1 --------------" << '\n';
        cout << "Abs Val Y ch [1:2,1:5]" << '\n';
        cout << unsigned(m_abs.Y[0]) << " " << unsigned(m_abs.Y[1]) << " " << unsigned(m_abs.Y[2]) << " "
             << unsigned(m_abs.Y[3]) << " " << unsigned(m_abs.Y[4]) << '\n';
        cout << unsigned(m_abs.Y[0 + m_width]) << " " << unsigned(m_abs.Y[m_width + 1]) << " "
             << unsigned(m_abs.Y[m_width + 2]) << " " << unsigned(m_abs.Y[m_width + 3]) << " "
             << unsigned(m_abs.Y[m_width + 4]) << '\n';

        std::size_t i = m_height - 1;
        std::size_t j = m_width - 1;
        cout << "Abs Val Y ch [end-1:end,end-4:end]" << '\n';
        cout << unsigned(m_abs.Y[(i - 1) * m_width + j - 4]) << " " << unsigned(m_abs.Y[(i - 1) * m_width + j - 3])
             << " " << unsigned(m_abs.Y[(i - 1) * m_width + j - 2]) << " "
             << unsigned(m_abs.Y[(i - 1) * m_width + j - 1]) << " " << unsigned(m_abs.Y[(i - 1) * m_width + j]) << '\n';
        cout << unsigned(m_abs.Y[i * m_width + j - 4]) << " " << unsigned(m_abs.Y[i * m_width + j - 3]) << " "
             << unsigned(m_abs.Y[i * m_width + j - 2]) << " " << unsigned(m_abs.Y[i * m_width + j - 1]) << " "
             << unsigned(m_abs.Y[i * m_width + j]) << '\n';
    }
#endif
}

/**
 * Transforms to absolute value. Input arguments are int pointers.
*/
void Encoder::toAbs(const std::int16_t* src, std::int16_t* dst, std::size_t length)
{
    for(std::size_t i = 0; i < length; i++) {   //
        dst[i] = (src[i] < 0) ? (-2 * (std::int32_t)src[i] - 1) : (2 * (std::int32_t)src[i]);
    }
}

/**
 * Transforms to absolute value. Input arguments is int.
*/
std::int16_t Encoder::toAbsSingle(const std::int16_t src)
{
    return (src < 0) ? (-2 * (std::int32_t)src - 1) : (2 * (std::int32_t)src);
}

/**
 * Calculates difference between actual and predicted value for all channels.
 */
void Encoder::differentiateAll(Encoder::algorithm algo)
{
    using ImageChannel = sQuadChannelCS::Channel;
    switch(algo) {
        case Encoder::algorithm::diffUp:
            differentiateDiffUp(ImageChannel::Y);
            differentiateDiffUp(ImageChannel::Cd);
            differentiateDiffUp(ImageChannel::Cm);
            differentiateDiffUp(ImageChannel::Co);
            break;
        default:
            throw std::runtime_error("Invalid algorithm!");
    }

#ifdef TEST_OUT
    {
        using namespace std;
        cout << '\n' << "------ Test 1.2 --------------" << '\n';
        cout << "DPCM image[1:2,1:5]" << '\n';
        cout << signed(m_dpcm.Y[0]) << " " << signed(m_dpcm.Y[1]) << " " << signed(m_dpcm.Y[2]) << " "
             << signed(m_dpcm.Y[3]) << " " << signed(m_dpcm.Y[4]) << '\n';
        cout << signed(m_dpcm.Y[0 + m_width]) << " " << signed(m_dpcm.Y[m_width + 1]) << " "
             << signed(m_dpcm.Y[m_width + 2]) << " " << signed(m_dpcm.Y[m_width + 3]) << " "
             << signed(m_dpcm.Y[m_width + 4]) << '\n';

        uint32_t i = m_height - 1;
        uint32_t j = m_width - 1;
        cout << "m_dpcm image[end-1:end,end-4:end]" << '\n';
        cout << signed(m_dpcm.Y[(i - 1) * m_width + j - 4]) << " " << signed(m_dpcm.Y[(i - 1) * m_width + j - 3]) << " "
             << signed(m_dpcm.Y[(i - 1) * m_width + j - 2]) << " " << signed(m_dpcm.Y[(i - 1) * m_width + j - 1]) << " "
             << signed(m_dpcm.Y[(i - 1) * m_width + j]) << '\n';
        cout << signed(m_dpcm.Y[i * m_width + j - 4]) << " " << signed(m_dpcm.Y[i * m_width + j - 3]) << " "
             << signed(m_dpcm.Y[i * m_width + j - 2]) << " " << signed(m_dpcm.Y[i * m_width + j - 1]) << " "
             << signed(m_dpcm.Y[i * m_width + j]) << '\n';
    }
#endif
};

/**
 * Calculates difference between actual and predicted value.
 * Predicted value is assumed to be equal to previous pixel except for 
 * a) first pixel which is seed.
 * b) first pixel in each row which is differentiated from upper neighbor pixel.
 * Matlab test 1.2
 */
void Encoder::differentiateDiffUp(sQuadChannelCS::Channel channel)
{
    std::span<std::int16_t const> full = m_pImgYCCC->getFullChannelsConst()->getChannelView(channel);
    std::span<std::int16_t> dpcm       = m_dpcm.getChannel(channel);
    std::int16_t pixelUp               = 0;
    for(std::size_t i = 0; i < m_height; i++) {
        // Seed pixel: first element in first row is full value
        /* TODO: In proposed work, first column element is also determined by differention*/
        dpcm[i * m_width] = full[i * m_width] - pixelUp;
        pixelUp           = full[i * m_width];
        // calculate difference for other elements
        for(std::size_t j = 1; j < m_width; j++) {
            dpcm[i * m_width + j] = full[i * m_width + j] - full[i * m_width + j - 1];   // dX(n) = X(n) - X(n-1)
        }
    }
};

/**
 * Runs adaptive Golomb Rice on All channels using specific algorithm.
*/
void Encoder::adaptiveGolombRiceAll(Encoder::algorithm algo)
{
    switch(algo) {
        case Encoder::algorithm::singleSeedInTwos:
            for(auto i = 0; i < 4; i++) {
                adaptiveGolombRiceSingleSeedInTwos(
                   m_dpcm.getChannel(i).data(),
                   m_height,
                   m_width,
                   m_quotient.getChannel(i).data(),
                   m_remainder.getChannel(i).data(),
                   m_kValues.getChannel(i).data(),
                   m_N_threshold,
                   m_A_init);
            }
            break;

        default:
            throw std::runtime_error("Invalid algorithm for adaptive golomb rice q&r calc!");
    }

        // Test 5.3 Adaptive Golomb Rice
#ifdef TEST_OUT
    {
        using namespace std;
        cout << '\n'
             << "------ Test 5.3 - AGOR (LOCO-I) --- N_threshold = " << unsigned(N_threshold) << "----------" << '\n';
        cout << "quotient ch Y [1:2,1:5]" << '\n';
        cout << unsigned(m_quotient.Y[0]) << " "   //
             << unsigned(m_quotient.Y[1]) << " "   //
             << unsigned(m_quotient.Y[2]) << " "   //
             << unsigned(m_quotient.Y[3]) << " "   //
             << unsigned(m_quotient.Y[4]) << " "   //
             << unsigned(m_quotient.Y[5]) << '\n';
        cout << unsigned(m_quotient.Y[m_width + 0]) << " "   //
             << unsigned(m_quotient.Y[m_width + 1]) << " "   //
             << unsigned(m_quotient.Y[m_width + 2]) << " "   //
             << unsigned(m_quotient.Y[m_width + 3]) << " "   //
             << unsigned(m_quotient.Y[m_width + 4]) << " "   //
             << unsigned(m_quotient.Y[m_width + 5]) << '\n';
        cout << unsigned(m_quotient.Y[2 * m_width + 0]) << " "   //
             << unsigned(m_quotient.Y[2 * m_width + 1]) << " "   //
             << unsigned(m_quotient.Y[2 * m_width + 2]) << " "   //
             << unsigned(m_quotient.Y[2 * m_width + 3]) << " "   //
             << unsigned(m_quotient.Y[2 * m_width + 4]) << " "   //
             << unsigned(m_quotient.Y[2 * m_width + 5]) << '\n';

        cout << "remainder ch Y [end-1:end,end-4:end]" << '\n';
        std::size_t end = m_remainder.Y.size();
        cout << unsigned(m_remainder.Y[end - 2 * m_width - 6]) << " "   //
             << unsigned(m_remainder.Y[end - 2 * m_width - 5]) << " "   //
             << unsigned(m_remainder.Y[end - 2 * m_width - 4]) << " "   //
             << unsigned(m_remainder.Y[end - 2 * m_width - 3]) << " "   //
             << unsigned(m_remainder.Y[end - 2 * m_width - 2]) << " "   //
             << unsigned(m_remainder.Y[end - 2 * m_width - 1]) << '\n';   //
        cout << unsigned(m_remainder.Y[end - m_width - 6]) << " "   //
             << unsigned(m_remainder.Y[end - m_width - 5]) << " "   //
             << unsigned(m_remainder.Y[end - m_width - 4]) << " "   //
             << unsigned(m_remainder.Y[end - m_width - 3]) << " "   //
             << unsigned(m_remainder.Y[end - m_width - 2]) << " "   //
             << unsigned(m_remainder.Y[end - m_width - 1]) << '\n';   //
        cout << unsigned(m_remainder.Y[end - 6]) << " "   //
             << unsigned(m_remainder.Y[end - 5]) << " "   //
             << unsigned(m_remainder.Y[end - 4]) << " "   //
             << unsigned(m_remainder.Y[end - 3]) << " "   //
             << unsigned(m_remainder.Y[end - 2]) << " "   //
             << unsigned(m_remainder.Y[end - 1]) << '\n';   //

        cout << "k Values ch Y [1:2,1:5]" << '\n';
        cout << unsigned(m_kValues.Y[0]) << " "   //
             << unsigned(m_kValues.Y[1]) << " "   //
             << unsigned(m_kValues.Y[2]) << " "   //
             << unsigned(m_kValues.Y[3]) << " "   //
             << unsigned(m_kValues.Y[4]) << '\n';
        cout << unsigned(m_kValues.Y[m_width + 0]) << " "   //
             << unsigned(m_kValues.Y[m_width + 1]) << " "   //
             << unsigned(m_kValues.Y[m_width + 2]) << " "   //
             << unsigned(m_kValues.Y[m_width + 3]) << " "   //
             << unsigned(m_kValues.Y[m_width + 4]) << '\n';

        cout << "k values ch Y [end-1:end,end-4:end]" << '\n';
        end = m_kValues.Y.size();
        cout << unsigned(m_kValues.Y[end - m_width - 5]) << " "   //
             << unsigned(m_kValues.Y[end - m_width - 4]) << " "   //
             << unsigned(m_kValues.Y[end - m_width - 3]) << " "   //
             << unsigned(m_kValues.Y[end - m_width - 2]) << " "   //
             << unsigned(m_kValues.Y[end - m_width - 1]) << '\n';   //
        cout << unsigned(m_kValues.Y[end - 5]) << " "   //
             << unsigned(m_kValues.Y[end - 4]) << " "   //
             << unsigned(m_kValues.Y[end - 3]) << " "   //
             << unsigned(m_kValues.Y[end - 2]) << " "   //
             << unsigned(m_kValues.Y[end - 1]) << '\n';   //
    }
#endif
};

/**
 * Adaptive golomb rice encoding of dpcm data. Prediction for first pixel in a row
 * is one pixel up. Top left corner pixel is copied - single seed pixel.
 * Also keeps track of k parameters used on each pixel.
 * Algorithm based on LOCO-I method.
*/
void Encoder::adaptiveGolombRiceSingleSeedInTwos(
   const std::int16_t* dpcm,
   std::size_t height,
   std::size_t width,
   std::int16_t* q,
   std::int16_t* r,
   std::int16_t* kValues,
   std::uint32_t N_threshold,
   std::uint32_t A_init)
{
    std::size_t length = height * width;

    std::uint32_t A = A_init;   // floor(2^16+32)/64
    std::uint32_t N = 4;

    /* Seed pixel will be encoded using two's complement so just cast it. */
    q[0]       = (std::int16_t)dpcm[0];
    r[0]       = (std::int16_t)dpcm[0];
    kValues[0] = -1;

    for(std::size_t idx = 1; idx < length; idx++) {

        /** Worst case: loop until: N_min * 2^k_max < N_threshold*max(dpcm)
         *                             1 * 2^k_max < 8*1020
         *                                 2^13 = 8192 < 8160
         *                                 k_max = 13 */
        std::uint16_t absVal = toAbsSingle(dpcm[idx]);
        std::int16_t k       = m_k_min;
        while((N << k) < A) {   // N << k = N*(2^k)
            k++;
        }

        q[idx] = absVal / (1 << k);   // (1 << k == 2^k)
        r[idx] = absVal % (1 << k);

        N += 1;
        A += (dpcm[idx] >= 0 ? dpcm[idx] : -dpcm[idx]);

        if(N >= N_threshold) {
            N = N / 2;
            A = A / 2;
        }
        A = A < A_MIN ? A_MIN : A;

        kValues[idx] = k;
    }
    //}
}

/**
 * Get width of each channel.
*/
std::size_t Encoder::getWidth() const
{
    return m_width;
};

/**
 * Get height of each channel.
*/
std::size_t Encoder::getHeight() const
{
    return m_height;
};

/**
 * Get length of each channel.
*/
std::size_t Encoder::getLength() const
{
    return m_length;
};

/**
 * Get length of each channel.
*/
std::size_t Encoder::getFileSize() const
{
    return m_fileSize;
};

/**
 * Generate and write bitstream for all channels. Returns pointer to vector with channel sizes in bytes.
*/
std::unique_ptr<std::vector<std::size_t>> Encoder::encodeBitstreamAll()
{

    char fileName[200];
    if(m_idealRule) {
        sprintf(fileName, "%s/compressed/%s%02zu_ideal.bin", m_folderOut, m_fileName, m_imgIdx);
    } else {
        sprintf(fileName, "%s/compressed/%s%02zu.bin", m_folderOut, m_fileName, m_imgIdx);
    }

    std::uint32_t bfr    = 0;
    std::size_t bitCnt   = 0;
    std::size_t bytesCnt = 0;
    std::ofstream wf(fileName, std::ios::out | std::ios::binary);

    if(!wf) {
        char msg[200];
        sprintf(msg, "Cannot open specified file: %s", fileName);
        throw std::runtime_error(msg);
    }

    Writter_s writter{&wf, &bfr, &bitCnt, &bytesCnt};

    std::uint16_t widthHeight[2];
    widthHeight[0] = getWidth();
    widthHeight[1] = getHeight();
    wf.write((char*)widthHeight, 4); /* Little endian order */

    std::vector<std::size_t> bytesWritten(4);

    using sQC       = sQuadChannelCS::Channel;
    bytesWritten[0] = encodeBitstreamOnChannel(writter, sQC::Y);
    bytesWritten[1] = encodeBitstreamOnChannel(writter, sQC::Cd);
    bytesWritten[2] = encodeBitstreamOnChannel(writter, sQC::Cm);
    bytesWritten[3] = encodeBitstreamOnChannel(writter, sQC::Co);
    flushBitstream(writter);

    wf.close();

    return std::make_unique<std::vector<std::size_t>>(std::forward<std::vector<std::size_t>>(bytesWritten));
}

/**
 * Generate and write bitstream for specific channel. If seedInTwosComplement = true
 * then encode first value in two's complement.
*/
std::size_t Encoder::encodeBitstreamOnChannel(Writter_s writter, sQuadChannelCS::Channel ch)
{
    return encodeBitstream(
       writter,
       m_quotient.getChannelView(ch).data(),
       m_remainder.getChannelView(ch).data(),
       m_length,
       m_kValues.getChannelDataConst((sQuadChannelCS::Channel)ch));
}

/**
 * Generates bitstream of quotients and remainders. Big endian.
*/
std::size_t Encoder::encodeBitstream(
   Writter_s writter,
   const std::int16_t* q,
   const std::int16_t* r,
   const std::size_t length,
   const std::int16_t* kValues)
{
    auto bytesWritten_start = *writter.m_pBytesCnt;

    for(std::size_t i = 0; i < length; i++) {
        auto curr_q = q[i];
        auto curr_r = r[i];
        auto curr_k = kValues[i];
        (void)curr_q;
        (void)curr_r;
        (void)curr_k;
        if(kValues[i] == -1) {
            // two's complement
            for(std::int16_t n = 16; n > 0; n--) {   // big endian
                std::uint32_t bit = (q[i] >> (n - 1)) & (std::uint32_t)1;
                pushBit(writter, bit);
            }
        } else {
            // unary coding of quotient
            for(std::uint16_t n = 0; n < q[i]; n++) {   // big endian
                pushBit_1(writter);
            }
            pushBit_0(writter);

            // remainder coding
            //for(std::size_t n = 0; n < k; n++) { // little endian
            //uint32_t bit = (r[i] >> n) & (std::uint32_t)1;
            for(std::int16_t n = kValues[i]; n > 0; n--) {   // big endian
                std::uint32_t bit = (r[i] >> (n - 1)) & (std::uint32_t)1;
                pushBit(writter, bit);
            }
        }
    }

    auto bytesWritten_end = *writter.m_pBytesCnt;
    return bytesWritten_end - bytesWritten_start;
    //  *writter.m_pBytesCnt - bytesWritten_start;
}

/**
 * Puts '1' in a buffer.
 * If cnt = 8, that writes a byte to file.
 */
void Encoder::pushBit_1(Writter_s writter)
{
    pushBit(writter, 1);
}

/**
 * Puts '0' in a buffer.
 * If cnt = 8, that writes a byte to file.
 */
void Encoder::pushBit_0(Writter_s writter)
{
    pushBit(writter, 0);
}

/**
 * Puts bit in a buffer.
 * If cnt = 8, that writes a byte to file.
 */
void Encoder::pushBit(Writter_s writter, std::uint32_t bit)
{

    if(bit == 1) {
        *writter.m_pBfr = (*writter.m_pBfr << 1) | (uint32_t)1;
    } else if(bit == 0) {
        *writter.m_pBfr = (*writter.m_pBfr << 1) & ~((uint32_t)1);
    } else {
        throw std::runtime_error("Non bit value suppplied to pushBit");
    }
    (*(writter.m_pBitCnt))++;
    // Buffer full (8 bits collected), write new byte to file.
    if(*writter.m_pBitCnt == 8) {
        char byte          = *writter.m_pBfr;
        *writter.m_pBitCnt = 0;
        writter.m_pWf->write(&byte, 1);
        (*writter.m_pBytesCnt)++;
    }
}

void Encoder::pushHeader(Writter_s writter, std::uint64_t header)
{

    writter.m_pWf->write((const char*)&header, sizeof(header));
    (*writter.m_pBytesCnt) += sizeof(header);
}

/**
 * Flush last byte. Fill in '0' to missing bits and write to file.
*/
void Encoder::flushBitstream(Writter_s writter)
{
    for(auto n = 8 - *writter.m_pBitCnt; n > 0; n--) {   //
        pushBit_0(writter);
    }

    if(*writter.m_pBytesCnt % 16 != 0) {
        for(auto n = 16 - (*writter.m_pBytesCnt % 16); n > 0; n--) {   //
            for(auto n = 8; n > 0; n--) {   //
                pushBit_0(writter);
            }
        }
    }
}

/* Dump DPCM (differential relative to prev pixel) values to file.*/
void Encoder::dumpDpcmToFile(const char* fileName)
{
    Helpers::dumpToFile(fileName, &m_dpcm, getWidth(), getHeight());

    //std::cout << "Finished dumping Dpcm to file: " << fileName << std::endl;
};

/* Dump abs values to file.*/
void Encoder::dumpAbsToFile(const char* fileName)
{
    Helpers::dumpToFile(fileName, getAbsChannelsConst(), getWidth(), getHeight());
    //std::cout << "Finished dumping abs values to file: " << fileName << std::endl;
};

/* Dump quotient values to file.*/
void Encoder::dumpQuotientToFile(const char* fileName)
{
    Helpers::dumpToFile(fileName, getQuotientChannelsConst(), getWidth(), getHeight());
    //std::cout << "Finished dumping quotients to file: " << fileName << std::endl;
};

/* Dump remainder values to file.*/
void Encoder::dumpRemainderToFile(const char* fileName)
{
    Helpers::dumpToFile(fileName, getRemainderChannelsConst(), getWidth(), getHeight());
    //std::cout << "Finished dumping remainders to file: " << fileName << std::endl;
};

/* Get a reference to Abs channel.*/
const sQuadChannelCS* Encoder::getAbsChannelsConst() const
{
    return &m_abs;
};
/* Get a reference to Quotient channel.*/
const sQuadChannelCS* Encoder::getQuotientChannelsConst() const
{
    return &m_quotient;
};
/* Get a reference to Remainder channel.*/
const sQuadChannelCS* Encoder::getRemainderChannelsConst() const
{
    return &m_remainder;
};

sQuadChannelCS* Encoder::getDpcmChannels()
{
    return &m_dpcm;
};
const sQuadChannelCS* Encoder::getDpcmChannelsConst() const
{
    return &m_dpcm;
};
