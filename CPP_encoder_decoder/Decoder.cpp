#include "Decoder.hpp"
#include "helpers.hpp"
#include <span>
#include <stdexcept>
#include <vector>

#ifdef TIMING_EN
#    include <chrono>
#endif

Decoder::Decoder(){};
Decoder::Decoder(
   const char* fileName,
   std::size_t width,
   std::size_t height,
   std::uint32_t A_init,
   std::uint32_t N_threshold)
   : m_width(width / 2)
   , m_height(height / 2)
   , m_pixelAmount(width * height)
   , m_N_threshold(N_threshold)
   , m_A_init(A_init)
{
#ifdef TIMING_EN
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
#endif
    Decoder::importBitstream(fileName);
#ifdef TIMING_EN
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << "Image importing time = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()
              << "[ms]" << std::endl;
#endif
    std::cout << "\n\nImported: " << fileName << std::endl;
};

Decoder::Decoder(const char* fileName, std::uint32_t A_init, std::uint32_t N_threshold)
   : m_N_threshold(N_threshold)
   , m_A_init(A_init)
{
#ifdef TIMING_EN
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
#endif
    Decoder::importBitstream(fileName);

    // m_pixelAmount = m_width * m_height;
#ifdef TIMING_EN
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << "Image importing time = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()
              << "[ms]" << std::endl;
#endif
    std::cout << "\n\nImported: " << fileName << std::endl;
};

/**
 * This one cannot be used for decoding data with channels that are encoded in parallel.
*/
void Decoder::decodeSequentially(std::size_t lossyBits)
{
    std::cout << "\nSequential decoding: " << unsigned(m_width) << " x " << unsigned(m_height) << std::endl;
    decodeBitstreamAll(m_N_threshold, m_A_init);
    toFullAll();
    toBayerGB(lossyBits);
}

/**
 * Decodes data with channels that are encoded in parallel.
*/
headerData_t Decoder::decodeParallel()
{

    // std::vector<std::uint8_t> bayerGB(2 * width * 2 * height);
    // a) here we need info about size to allocate vector
    // b) we need to be able to accept unique_ptr to vector

    headerData_t headerData;

    auto data = m_pFileData.get();

    auto status = Reader::getTimestampAndCompressionInfoFromHeader(   //
       data->data(),
       headerData.timestamp,
       headerData.roi,
       headerData.width,
       headerData.height,
       headerData.unaryMaxWidth,
       headerData.bpp,
       headerData.lossyBits,
       headerData.reserved);

    m_width  = headerData.width / 2;
    m_height = headerData.height / 2;

    if(status) {
        throw std::runtime_error("Error while reading header.");
    }

    std::cout << "\nUsing CPU: Parallel decoding image size W x H : " << unsigned(headerData.width) << " x "
              << unsigned(headerData.height) << std::endl;

    if(headerData.bpp == 8) {
        std::vector<std::uint8_t> out_buffer(headerData.width * headerData.height);

        status = DecoderBase::decodeBitstreamParallel_actual<std::uint8_t>(
           headerData.width,
           headerData.height,
           headerData.lossyBits,
           headerData.unaryMaxWidth,
           headerData.bpp,
           data->data() + 24,
           data->size() - 24,
           out_buffer.data(),
           out_buffer.size());
        if(status) {
            handleReturnValue(status);
            // printf("Error while decoding bitstream, error code: %d.\n", status);
            throw std::runtime_error("Parallel decoding unsuccessful.");
        };

        m_pBayer_8bit =
           std::make_unique<std::vector<std::uint8_t>>(std::forward<std::vector<std::uint8_t>>(out_buffer));
        m_pBayer_16bit.reset();

    } else {
        std::vector<std::uint16_t> out_buffer(headerData.width * headerData.height);
        status = DecoderBase::decodeBitstreamParallel_actual<std::uint16_t>(
           headerData.width,
           headerData.height,
           headerData.lossyBits,
           headerData.unaryMaxWidth,
           headerData.bpp,
           data->data() + 24,
           data->size() - 24,
           out_buffer.data(),
           out_buffer.size());
        if(status) {
            handleReturnValue(status);
            // printf("Error while decoding bitstream, error code: %d.\n", status);
            throw std::runtime_error("Parallel decoding unsuccessful.");
        };

        m_pBayer_16bit =
           std::make_unique<std::vector<std::uint16_t>>(std::forward<std::vector<std::uint16_t>>(out_buffer));
        m_pBayer_8bit.reset();
    }

    return headerData;
}

/**
 * Decodes data with channels that are encoded in parallel.
*/
headerData_t Decoder::decodeParallelGPU()
{

    // std::vector<std::uint8_t> bayerGB(2 * width * 2 * height);
    // a) here we need info about size to allocate vector
    // b) we need to be able to accept unique_ptr to vector

    headerData_t headerData;

    auto data = m_pFileData.get();

    auto status = Reader::getTimestampAndCompressionInfoFromHeader(   //
       data->data(),
       headerData.timestamp,
       headerData.roi,
       headerData.width,
       headerData.height,
       headerData.unaryMaxWidth,
       headerData.bpp,
       headerData.lossyBits,
       headerData.reserved);

    m_width  = headerData.width / 2;
    m_height = headerData.height / 2;

    if(status) {
        throw std::runtime_error("Error while reading header.");
    }

    std::cout << "\nUsing GPU: Parallel decoding image size W x H : " << unsigned(headerData.width) << " x "
              << unsigned(headerData.height) << std::endl;

    if(headerData.bpp == 8) {
        std::vector<std::uint8_t> out_buffer(headerData.width * headerData.height);

        status = DecoderBase::decodeBitstreamParallel_pseudo_gpu(
           headerData.width,
           headerData.height,
           headerData.lossyBits,
           headerData.unaryMaxWidth,
           headerData.bpp,
           data->data() + 24,
           data->size() - 24,
           out_buffer.data(),
           out_buffer.size());
        if(status) {
            handleReturnValue(status);
            // printf("Error while decoding bitstream, error code: %d.\n", status);
            throw std::runtime_error("Parallel decoding unsuccessful.");
        };
       
        m_pBayer_8bit =
           std::make_unique<std::vector<std::uint8_t>>(std::forward<std::vector<std::uint8_t>>(out_buffer));
        m_pBayer_16bit.reset();

    } else {

        throw std::runtime_error("10 and 12 BPP GPU decoding not yet supported.");

        // std::vector<std::uint16_t> out_buffer(headerData.width * headerData.height);
        // status = DecoderBase::decodeBitstreamParallel_actual<std::uint16_t>(
        //    headerData.width,
        //    headerData.height,
        //    headerData.lossyBits,
        //    headerData.unaryMaxWidth,
        //    headerData.bpp,
        //    data->data() + 24,
        //    data->size() - 24,
        //    out_buffer.data(),
        //    out_buffer.size());
        // if(status) {
        //     handleReturnValue(status);
        //     // printf("Error while decoding bitstream, error code: %d.\n", status);
        //     throw std::runtime_error("Parallel decoding unsuccessful.");
        // };

        // m_pBayer_16bit =
        //    std::make_unique<std::vector<std::uint16_t>>(std::forward<std::vector<std::uint16_t>>(out_buffer));
        // m_pBayer_8bit.reset();
    }

    return headerData;
}

void Decoder::exportBayerImage(const char* fileName, std::uint64_t header, std::uint64_t roi, std::uint64_t timestamp)
{
    if(m_pBayer_8bit != nullptr) {
        DecoderBase::exportImage(
           fileName,
           m_pBayer_8bit->data(),
           (2 * getWidth() * 2 * getHeight()),
           header,
           roi,
           timestamp);
    } else if(m_pBayer_16bit != nullptr) {
        DecoderBase::exportImage(
           fileName,
           (std::uint8_t*)m_pBayer_16bit->data(),
           2 * (2 * getWidth() * 2 * getHeight()),
           header,
           roi,
           timestamp);
    } else {
        throw std::runtime_error("No bayer image data available.");
    }
}

/**
 * Translates image from YCCC to Bayer GreenBlue image (twice as large).
*/
void Decoder::toBayerGB(std::size_t lossyBits)
{
    std::vector<std::uint16_t> bayerGB(4 * m_pixelAmount);
    m_width_bayer  = 2 * getWidth();
    m_height_bayer = 2 * getHeight();

    for(std::size_t i = 0; i < m_height; i++) {
        for(std::size_t j = 0; j < m_width; j++) {
            std::size_t idx   = i * m_width + j;
            std::size_t idxGB = 2 * i * m_width_bayer + 2 * j;
            std::size_t idxB  = 2 * i * m_width_bayer + 2 * j + 1;
            std::size_t idxR  = (2 * i + 1) * m_width_bayer + 2 * j;
            std::size_t idxGR = (2 * i + 1) * m_width_bayer + 2 * j + 1;
            DecoderBase::YCCC_to_BayerGB(
               m_pFull->Y[idx],
               m_pFull->Cd[idx],
               m_pFull->Cm[idx],
               m_pFull->Co[idx],
               bayerGB[idxGB],
               bayerGB[idxB],
               bayerGB[idxR],
               bayerGB[idxGR],
               lossyBits);
        }
    }

#ifdef TEST_OUT
    // clang-format off
    std::cout << '\n' << "------ Test 5.2 --- Bayer[0:1,0:3]-------" << '\n';
    std::cout << unsigned(bayerGB[0]) << " " << unsigned(bayerGB[1]) << " "   //
              << unsigned(bayerGB[2]) << " " << unsigned(bayerGB[3]) << " "   //
              << unsigned(bayerGB[3]) << " " << unsigned(bayerGB[5]) << '\n';
    std::cout << unsigned(bayerGB[0 + m_width_bayer]) << " " << unsigned(bayerGB[1 + m_width_bayer]) << " "   //
              << unsigned(bayerGB[2 + m_width_bayer]) << " " << unsigned(bayerGB[3 + m_width_bayer]) << " "   //
              << unsigned(bayerGB[4 + m_width_bayer]) << " " << unsigned(bayerGB[5 + m_width_bayer]) << '\n';
    std::cout << '\n' << "------ Test 5.2 --- Bayer[end-1:end,end-3:end]----" << '\n';
    std::cout << unsigned(bayerGB.end()[-m_width_bayer - 6]) << " " << unsigned(bayerGB.end()[-m_width_bayer - 5]) << " "   //
              << unsigned(bayerGB.end()[-m_width_bayer - 4]) << " " << unsigned(bayerGB.end()[-m_width_bayer - 3]) << " "   //
              << unsigned(bayerGB.end()[-m_width_bayer - 2]) << " " << unsigned(bayerGB.end()[-m_width_bayer - 1]) << '\n';
    std::cout << unsigned(bayerGB.end()[-6]) << " " << unsigned(bayerGB.end()[-5]) << " "   //
              << unsigned(bayerGB.end()[-4]) << " " << unsigned(bayerGB.end()[-3]) << " "   //
              << unsigned(bayerGB.end()[-2]) << " " << unsigned(bayerGB.end()[-1]) << '\n';
// clang-format on
#endif
    m_pBayer_16bit = std::make_unique<std::vector<std::uint16_t>>(std::forward<std::vector<std::uint16_t>>(bayerGB));
};

/**
 * Translates dpcm data to full image data on all channels.
*/
void Decoder::toFullAll()
{
    sQuadChannelCS full(m_pixelAmount);

    for(std::size_t chIdx = 0; chIdx < 4; chIdx++) {
        toFull(m_pDpcm->getChannel(chIdx).data(), full.getChannel(chIdx).data(), getHeight(), getWidth());
    };

    m_pFull = std::make_unique<sQuadChannelCS>(std::forward<sQuadChannelCS>(full));

#ifdef TEST_OUT
    // clang-format off
            std::cout << '\n' << "------ Test 5.1.2 (same as 1.1.2) --- [0:3]-------" << '\n';
            std::cout << "Y [0:3]: " << signed(m_pFull->Y[0])  << " " << signed(m_pFull->Y[1])  << " " << signed(m_pFull->Y[2])  << " " << signed(m_pFull->Y[3])  <<'\n';
            std::cout << "Cd[0:3]: " << signed(m_pFull->Cd[0]) << " " << signed(m_pFull->Cd[1]) << " " << signed(m_pFull->Cd[2]) << " " << signed(m_pFull->Cd[3]) <<'\n';
            std::cout << "Cm[0:3]: " << signed(m_pFull->Cm[0]) << " " << signed(m_pFull->Cm[1]) << " " << signed(m_pFull->Cm[2]) << " " << signed(m_pFull->Cm[3]) <<'\n';
            std::cout << "Co[0:3]: " << signed(m_pFull->Co[0]) << " " << signed(m_pFull->Co[1]) << " " << signed(m_pFull->Co[2]) << " " << signed(m_pFull->Co[3]) <<'\n';
            std::cout << '\n' << "------ Test 5.1.2 (same as 1.1.2) --- [-3:end]----" << '\n';
            std::cout << "Y [-3:end]: " << signed(m_pFull-> Y.end()[-4]) << " " << signed(m_pFull-> Y.end()[-3]) << " " << signed(m_pFull-> Y.end()[-2]) << " " << signed(m_pFull-> Y.end()[-1]) <<'\n';
            std::cout << "Cd[-3:end]: " << signed(m_pFull->Cd.end()[-4]) << " " << signed(m_pFull->Cd.end()[-3]) << " " << signed(m_pFull->Cd.end()[-2]) << " " << signed(m_pFull->Cd.end()[-1]) <<'\n';
            std::cout << "Cm[-3:end]: " << signed(m_pFull->Cm.end()[-4]) << " " << signed(m_pFull->Cm.end()[-3]) << " " << signed(m_pFull->Cm.end()[-2]) << " " << signed(m_pFull->Cm.end()[-1]) <<'\n';
            std::cout << "Co[-3:end]: " << signed(m_pFull->Co.end()[-4]) << " " << signed(m_pFull->Co.end()[-3]) << " " << signed(m_pFull->Co.end()[-2]) << " " << signed(m_pFull->Co.end()[-1]) <<'\n';
    // clang-format on
#endif
}

/**
 * Translates dpcm data to full image data.
*/
void Decoder::toFull(const std::int16_t* dpcm, std::int16_t* full, std::size_t height, std::size_t width)
{
    std::int16_t pixelUp = 0;
    for(std::size_t i = 0; i < m_height; i++) {
        full[i * m_width] = dpcm[i * m_width] + pixelUp;
        pixelUp           = full[i * m_width];
        for(std::size_t j = 1; j < m_width; j++) {
            full[i * m_width + j] = full[i * m_width + j - 1] + dpcm[i * m_width + j];
        }
    }
}

/**
 * Decodes k. Decodes quotients and remainders from bitstream. Uses q and r to calculate abs value,
 * then calculates dpcm and saves dpcm data to m_pDpcm pointer.
*/
void Decoder::decodeBitstreamAll(std::uint32_t N_threshold, std::uint32_t A_init)
{
    Reader reader{m_pFileData.get()->data(), m_pFileData.get()->size()};

    sQuadChannelCS quotients(m_pixelAmount);
    sQuadChannelCS remainders(m_pixelAmount);
    sQuadChannelCS kValues(m_pixelAmount);
    sQuadChannelCS dpcm(m_pixelAmount);

    for(std::size_t chIdx = 0; chIdx < 4; chIdx++) {
        std::size_t pixelCount = Decoder::decodeBitstream(
           reader,
           N_threshold,
           A_init,
           m_pixelAmount,
           quotients.getChannel(chIdx).data(),
           remainders.getChannel(chIdx).data(),
           kValues.getChannel(chIdx).data(),
           dpcm.getChannel(chIdx).data());
        if(pixelCount != m_pixelAmount) {
            throw std::runtime_error("Number of decoded pixels does not match expected number of pixels!");
        }
    }

    // Test 2.2 GolombRice
#ifdef TEST_OUT
    using namespace std;
    cout << '\n' << "------ Test 4.2.2 (same as Test 2.2 k = 2) --------------" << '\n';
    cout << "quotient ch Y [1:2,1:5]" << '\n';
    cout << unsigned(quotients.Y[0]) << " "   //
         << unsigned(quotients.Y[1]) << " "   //
         << unsigned(quotients.Y[2]) << " "   //
         << unsigned(quotients.Y[3]) << " "   //
         << unsigned(quotients.Y[4]) << '\n';
    cout << unsigned(quotients.Y[m_width + 0]) << " "   //
         << unsigned(quotients.Y[m_width + 1]) << " "   //
         << unsigned(quotients.Y[m_width + 2]) << " "   //
         << unsigned(quotients.Y[m_width + 3]) << " "   //
         << unsigned(quotients.Y[m_width + 4]) << '\n';

    cout << "remainder ch Y [end-1:end,end-4:end]" << '\n';
    std::size_t end = remainders.Y.size();
    cout << unsigned(remainders.Y[end - m_width - 5]) << " "   //
         << unsigned(remainders.Y[end - m_width - 4]) << " "   //
         << unsigned(remainders.Y[end - m_width - 3]) << " "   //
         << unsigned(remainders.Y[end - m_width - 2]) << " "   //
         << unsigned(remainders.Y[end - m_width - 1]) << '\n';   //
    cout << unsigned(remainders.Y[end - 5]) << " "   //
         << unsigned(remainders.Y[end - 4]) << " "   //
         << unsigned(remainders.Y[end - 3]) << " "   //
         << unsigned(remainders.Y[end - 2]) << " "   //
         << unsigned(remainders.Y[end - 1]) << '\n';   //
#endif

    m_pQuotients  = std::make_unique<sQuadChannelCS>(std::forward<sQuadChannelCS>(quotients));
    m_pRemainders = std::make_unique<sQuadChannelCS>(std::forward<sQuadChannelCS>(remainders));
    m_pkValues    = std::make_unique<sQuadChannelCS>(std::forward<sQuadChannelCS>(kValues));
    m_pDpcm       = std::make_unique<sQuadChannelCS>(std::forward<sQuadChannelCS>(dpcm));
}

/**
 * Reads imported bitstream and calculates quotients, remainders and k values on a single channel, 
 * continuing from the byte with index <bytesRead>.
 * Returns number of read quotients and remainders.
*/
std::size_t Decoder::decodeBitstream(
   Reader& reader,
   std::uint32_t N_threshold,
   std::uint32_t A_init,
   std::size_t length,
   std::int16_t* quotients,
   std::int16_t* remainders,
   std::int16_t* kValues,
   std::int16_t* dpcm)
{
    std::int16_t dpcm_curr = 0;
    std::uint32_t lastBit;
    std::size_t idx = 0;

    std::uint32_t A = A_init;
    std::uint32_t N = N_START;

    // Seed pixel:
    std::uint16_t posValue = 0;
    // for(std::uint32_t n = k_SEED + 1; n > 0; n--) {
    for(std::uint32_t n = 16; n > 0; n--) {
        lastBit  = reader.fetchBit();
        posValue = (posValue << 1) | lastBit;
    }
    dpcm_curr = DecoderBase::fromAbs(posValue);

    quotients[0]  = dpcm_curr;
    remainders[0] = dpcm_curr;
    kValues[0]    = -1;
    dpcm[0]       = dpcm_curr;

    for(idx = 1; idx < length; idx++) {

        std::uint16_t absVal = 0;
        std::int16_t q       = 0;
        std::int16_t r       = 0;
        std::uint16_t k      = 0;

        while((N << k) < A) {   // N << k = N*(2^k)
            k++;
        }

        // decode quotient: unary coding
        q = 0;
        do {
            lastBit = reader.fetchBit();
            if(lastBit == 1) {   //
                q++;
            }
        } while(lastBit == 1);

        //decode remainder
        r = 0;
        for(std::uint32_t n = k; n > 0; n--) {
            lastBit = reader.fetchBit();
            r       = (r << 1) | lastBit;
        };

        absVal    = q * (1 << k) + r;
        dpcm_curr = DecoderBase::fromAbs(absVal);

        A += dpcm_curr > 0 ? dpcm_curr : -dpcm_curr;
        N += 1;

        if(N >= N_threshold) {
            N = N / 2;
            A = A / 2;
        }
        A = A < A_MIN ? A_MIN : A;

        quotients[idx]  = q;
        remainders[idx] = r;
        kValues[idx]    = k;
        dpcm[idx]       = dpcm_curr;
    }
    return idx;
}

/**
 * Reads bitstream from fileName. Interprets first 4 bytes as image resolution.
 * The pointer to the bitstream data is then assigned to member m_pFileData.
*/
void Decoder::importBitstream(const char* fileName)
{
    STATUS_t stat = DecoderBase::importBitstream(fileName, m_pFileData);
    if(stat) {
        char msg[200];
        sprintf(msg, "OMLS Error: %u:Error while importing bitstream: %s", stat, fileName);
        throw std::runtime_error(msg);
        // throw std::runtime_error("Error while importing bitstream");
    };

    // std::ifstream rf(fileName, std::ios::in | std::ios::binary);
    // if(!rf) {
    //     char msg[100];
    //     sprintf(msg, "Cannot open specified file: %s", fileName);
    //     throw std::runtime_error(msg);
    // }

    // std::vector<std::uint8_t> inputBytes(
    //    (std::istreambuf_iterator<char>(rf)),   //
    //    (std::istreambuf_iterator<char>()));

    // rf.close();

    // m_pFileData = std::make_unique<std::vector<std::uint8_t>>(std::forward<std::vector<std::uint8_t>>(inputBytes));
};

/** Get width.*/
std::size_t Decoder::getWidth() const
{
    return m_width;
};

/**
 * Get height.
*/
std::size_t Decoder::getHeight() const
{
    return m_height;
};

/**
 * Get height.
*/
std::size_t Decoder::getLength() const
{
    return m_pixelAmount;
};

std::size_t Decoder::getWidthBayer() const
{
    return m_width_bayer;
};
std::size_t Decoder::getHeightBayer() const
{
    return m_height_bayer;
};
