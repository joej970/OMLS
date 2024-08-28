#include "DecoderBase.hpp"
#include <stdexcept>

#if(defined(__MINGW32__) || defined(__MINGW64__)) && __cplusplus < 201703L
#    include <experimental/filesystem>
#else
#    include <filesystem>
#endif

#include <cstdint>
#include <vector>


/**********************************
 * Reader class
 *********************************/

Reader::Reader(const std::uint8_t* bitStream, const std::size_t bitStreamSize)
   : m_bitStream(bitStream)
   , m_bitStreamSize(bitStreamSize){};

void Reader::loadFirstByte(const std::uint8_t* bitStream, std::size_t& byteIdx, std::uint8_t& byte)
{
    byte = bitStream[byteIdx];
}

void Reader::loadFirstFourBytes(
   const std::uint8_t* bitStream,
   std::size_t& byteIdx,
   std::uint32_t (*bits_single_bfr)[4],
   std::uint32_t (*bits_shifted_bfr)[4])
{

    __uint128_t long_int_temp = 0;

    reverseByteOrder_16bytes(&(bitStream[byteIdx]), (uint8_t*)&long_int_temp);
    __uint128_t long_int = 0;
    memcpy(&long_int, &long_int_temp, sizeof(__uint128_t));
    // __uint128_t long_int;

    for(int i = 0; i < 32; i++) {
        bits_shifted_bfr[i][0] = (uint32_t)(long_int >> (127 - i - 0 * 32));
        bits_shifted_bfr[i][1] = (uint32_t)(long_int >> (127 - i - 1 * 32));
        bits_shifted_bfr[i][2] = (uint32_t)(long_int >> (127 - i - 2 * 32));
        bits_shifted_bfr[i][3] = (uint32_t)(long_int >> (127 - i - 3 * 32));

        bits_single_bfr[i][0] = bits_shifted_bfr[i][0] & 1;
        bits_single_bfr[i][1] = bits_shifted_bfr[i][1] & 1;
        bits_single_bfr[i][2] = bits_shifted_bfr[i][2] & 1;
        bits_single_bfr[i][3] = bits_shifted_bfr[i][3] & 1;
    }

    // printf("bitStream[0:3]: %x %x %x %x\n", bitStream[byteIdx], bitStream[byteIdx + 1], bitStream[byteIdx + 2], bitStream[byteIdx + 3]);
    // printf("      long_int: %x %x %x %x\n", (uint8_t)(long_int >> 24), (uint8_t)(long_int >> 16), (uint8_t)(long_int >> 8), (uint8_t)(long_int));
}

/**
 * Read last/rightmost/LSB bit at position indicated by ^, from a char* bitStream, 
 * then set cursor to next bit position indicated by '.
 * XXXXXXA____
 * XXXXXXXB___
 * XXXXXXXXC__
 * ________^'_
 */
std::uint32_t Reader::fetchBit_impl(
   const std::uint8_t* bitStream,
   std::size_t bitStream_size,
   std::size_t& bitsReadFromByte,
   std::size_t& byteIdx,
   std::uint8_t& byte)
{
    if(byteIdx == bitStream_size) {
        std::cout << "All bytes have been read." << std::endl;
        throw(BASE_ERROR_ALL_BYTES_ALREADY_READ);
    }
    if(bitsReadFromByte == 8) {
        bitsReadFromByte = 0;
        (byteIdx)++;
        byte = bitStream[byteIdx];   //update only if it all bits have been read
    }
    // todo: consider changing bit order so that there is only bitshift for 1 bit >> 1
    std::uint32_t bit = (byte >> (7 - bitsReadFromByte)) & (std::uint32_t)1;
    bitsReadFromByte++;
    return bit;
}




STATUS_t Reader::getTimestampAndCompressionInfoFromHeader(
   const std::uint8_t* bitStream,
   std::uint64_t& timestamp,
   std::uint64_t& roi,
   std::uint16_t& width,
   std::uint16_t& height,
   std::uint8_t& unaryMaxWidth,
   std::uint8_t& bpp,
   std::uint8_t& lossyBits,
   std::uint8_t& reserved)
{
    // std::cout << "Align of bitStream: " << alignof(bitStream)<<"-byte." << std::endl;
    if(alignof(decltype(bitStream)) != 8) {
        std::cout << "Header can be read only from 8-byte aligned memory." << std::endl;
        return BASE_ERROR_HEADER_NOT_ALIGNED;
    }

    //timestamp = Reader::fetch8bytes(bitStream, 0);
    timestamp = Reader::getTimestamp(bitStream);
    //printf("Decompression Image timestamp: %llu\n", timestamp);

#ifndef ROI_NOT_INCLUDED
    roi = Reader::getRoiHeader(bitStream);
#else
    roi = 0;
#endif
    std::uint64_t header = Reader::getOmlsHeader(bitStream);

    //printf("Decompression Image header: 0x%016llX\n", header);
    // 2-byte aligned
    width         = (reinterpret_cast<const std::uint16_t*>(&header))[0];
    height        = (reinterpret_cast<const std::uint16_t*>(&header))[1];
    unaryMaxWidth = (reinterpret_cast<const std::uint8_t*>(&header))[4];
    bpp           = (reinterpret_cast<const std::uint8_t*>(&header))[5];
    // 1-byte aligned
    lossyBits = (reinterpret_cast<const std::uint8_t*>(&header))[6];
    reserved  = (reinterpret_cast<const std::uint8_t*>(&header))[7];

    //printf("Width: %0d Height: %0d Lossy bits: %0d Unary width: %0d\n", width, height, lossyBits, unaryMaxWidth);

    // Backward compatibility: if bpp is not set, assume 8
    if(bpp == 0) {
        bpp = 8;
    }

    if(width % 16 != 0 || height % 16 != 0 || (bpp != 8 && bpp != 10 && bpp != 12)) {
        return BASE_ERROR_HEADER_DATA_INVALID;
    }

    return BASE_SUCCESS;
}

std::uint64_t Reader::fetch8bytes(const std::uint8_t* bitStream, std::size_t byteOffset)
{
    // std::cout << "Align of bitStream: " << alignof(decltype(bitStream)) << "-byte." << std::endl;
    if(alignof(decltype(bitStream)) != 8) {
        std::cout << "Header can be read only from 8-byte aligned memory." << std::endl;
        std::cerr << "Header can be read only from 8-byte aligned memory." << std::endl;
    }
    auto p_header = reinterpret_cast<const std::uint64_t*>(&bitStream[byteOffset]);

    return *p_header;
};

/**********************************
 * Misc functions
 *********************************/

/**
     * Inverts byte order. Used to calculate how much time could be saved if byte order is changed on FPGA.
     */
// inline void reverseByteOrder_16bytes(const std::uint8_t* bitStream, std::uint8_t* dest)
void reverseByteOrder_16bytes(const std::uint8_t* bitStream, std::uint8_t* dest)
{
    const int bytes = 16;
    for(std::size_t i = 0; i < bytes; i++) {
        dest[i] = bitStream[bytes - i - 1];
    }
}

/**********************************
 * DecoderBase class
 *********************************/

std::int16_t DecoderBase::fromAbs(std::uint16_t absValue)
{
    if(absValue % 2 == 0) {
        return absValue / 2;
    } else {
        return -((absValue + 1) / 2);
    }
};

/**
 * Reads bitstream from fileName. Interprets first 4 bytes as image resolution.
 * The pointer to the bitstream data is then assigned to member m_pFileData.
*/
STATUS_t DecoderBase::importBitstream(const char* fileName, std::unique_ptr<std::vector<std::uint8_t>>& outData)
{
    std::ifstream rf(fileName, std::ios::in | std::ios::binary);
    if(!rf) {
        return BASE_CANNOT_OPEN_INPUT_FILE;
    }

    std::vector<std::uint8_t> inputBytes(
       (std::istreambuf_iterator<char>(rf)),   //
       (std::istreambuf_iterator<char>()));

    outData =
       std::move(std::make_unique<std::vector<std::uint8_t>>(std::forward<std::vector<std::uint8_t>>(inputBytes)));

    rf.close();

    return BASE_SUCCESS;
};

STATUS_t DecoderBase::handleReturnValue(STATUS_t status)
{
    switch(status) {
        case BASE_SUCCESS:
            //std::cout << "Operation successful." << std::endl;
            break;
        case BASE_ERROR:
            std::cout << "DecoderBase: General error." << std::endl;
            break;
        case BASE_DIVISION_ERROR:
            std::cout << "DecoderBase: Division error." << std::endl;
            break;
        case BASE_ERROR_ALL_BYTES_ALREADY_READ:
            std::cout << "DecoderBase: All bytes already read error." << std::endl;
            break;
        case BASE_CANNOT_OPEN_INPUT_FILE:
            std::cout << "DecoderBase: Cannot open input file error." << std::endl;
            break;
        case BASE_CANNOT_OPEN_OUTPUT_FILE:
            std::cout << "DecoderBase: Cannot open output file error." << std::endl;
            break;
        case BASE_OUTPUT_BUFFER_FALSE_SIZE:
            std::cout << "DecoderBase: Output buffer false size error." << std::endl;
            break;
        case BASE_ERROR_HEADER_NOT_ALIGNED:
            std::cout << "DecoderBase: Header not aligned error." << std::endl;
            break;
        case BASE_ERROR_HEADER_DATA_INVALID:
            std::cout << "Invalid header values. Received image is corrupted. Might be camera image buffer issue. Try "
                         "increasing buffer size."
                      << std::endl;
            break;
        case BASE_OPENCL_ERROR:
            std::cout << "DecoderBase: OpenCL error." << std::endl;
            break;
        default:
            std::cout << "DecoderBase: Unknown error." << std::endl;
            break;
    }
    return status;
}

void createMissingDirectory(const char* folder_out)
{
    /** Create missing directories*/
#if(defined(__MINGW32__) || defined(__MINGW64__)) && __cplusplus < 201703L
    using namespace std::experimental::filesystem;
#else
    using namespace std::filesystem;
#endif
    char out_path[500];

    if(!path(folder_out).is_absolute()) {
        sprintf_s(out_path, "./%s", folder_out);
    } else {
        sprintf_s(out_path, "%s", folder_out);
    };
    //}

    //std::cout << "Proposed out location is: " << out_path << std::endl;
    if(!exists(out_path)) {
        if(create_directory(out_path)) {
            //std::cout << "Created new directory: " << out_path << std::endl;
        } else {
            std::cout << "Failed to create directory: " << out_path << std::endl;
        }
    }
    {
        //std::cout << "Directory already exists: " << out_path << std::endl;
    }
}
