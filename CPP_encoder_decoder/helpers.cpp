#include "helpers.hpp"
#include <fstream>
#include <iostream>
// #include <stdexcept>

// void Helpers::dumpBayer(const char* outputFile, const pImage img)
void Helpers::dump16pp(const char* outputFile, const Image* pImg)
{
    // # step 1: read .bin then generate ASCII file of pixels. Start with width, height \n
    // # then write <parapxl> in one line.
    std::ofstream wf(outputFile, std::ios::out);
    if(!wf) {
        char msg[200];
        sprintf(msg, "Cannot open specified file: %s", outputFile);
        throw std::runtime_error(msg);
    }

    char txt[200];

    sprintf(txt, "%zu %zu\n", pImg->getWidth(), pImg->getHeight());
    wf << txt;

    auto data = pImg->getDataView().data();

    for(std::size_t row = 0; row < pImg->getHeight(); row += 1) {
        for(std::size_t col = 0; col < pImg->getWidth(); col += 16) {
            sprintf(
               txt,
               "%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
               data[row * pImg->getWidth() + col + 0],
               data[row * pImg->getWidth() + col + 1],
               data[row * pImg->getWidth() + col + 2],
               data[row * pImg->getWidth() + col + 3],
               data[row * pImg->getWidth() + col + 4],
               data[row * pImg->getWidth() + col + 5],
               data[row * pImg->getWidth() + col + 6],
               data[row * pImg->getWidth() + col + 7],
               data[row * pImg->getWidth() + col + 8],
               data[row * pImg->getWidth() + col + 9],
               data[row * pImg->getWidth() + col + 10],
               data[row * pImg->getWidth() + col + 11],
               data[row * pImg->getWidth() + col + 12],
               data[row * pImg->getWidth() + col + 13],
               data[row * pImg->getWidth() + col + 14],
               data[row * pImg->getWidth() + col + 15]);
            wf << txt;
        }
    }

    wf.close();
}

void Helpers::dump8pp(const char* outputFile, const Image* pImg)
{
    std::ofstream wf(outputFile, std::ios::out);
    if(!wf) {
        char msg[200];
        sprintf(msg, "Cannot open specified file: %s", outputFile);
        throw std::runtime_error(msg);
    }

    char txt[200];

    sprintf(txt, "%zu %zu\n", pImg->getWidth(), pImg->getHeight());
    wf << txt;

    auto data = pImg->getDataView().data();

    for(std::size_t row = 0; row < pImg->getHeight(); row += 1) {
        for(std::size_t col = 0; col < pImg->getWidth(); col += 8) {
            sprintf(
               txt,
               "%u %u %u %u %u %u %u %u\n",
               data[row * pImg->getWidth() + col + 0],
               data[row * pImg->getWidth() + col + 1],
               data[row * pImg->getWidth() + col + 2],
               data[row * pImg->getWidth() + col + 3],
               data[row * pImg->getWidth() + col + 4],
               data[row * pImg->getWidth() + col + 5],
               data[row * pImg->getWidth() + col + 6],
               data[row * pImg->getWidth() + col + 7]);
            wf << txt;
        }
    }

    wf.close();
}

/**
 * Reads binary file with image data. Returns unique pointer to Image object.
 * 
*/
pImage Helpers::read_image(const char* input, std::size_t header_bytes, size_t width_a, size_t height_a)
{
    printf("Reading header...\n");

    FILE* file = fopen(input, "rb");
    if(file == nullptr) {
        std::cerr << "Failed to open the file." << std::endl;
        char msg[200];
        sprintf(msg, "Cannot open specified file: %s", input);
        throw std::runtime_error(msg);
    }

    auto header =
       new(std::align_val_t(16)) char[header_bytes];   // this will ensure that the header is aligned to 16 bytes
    size_t bytesRead = fread(header, 1, header_bytes, file);   // this makes a copy
    (void)bytesRead;

    std::uint16_t width;
    std::uint16_t height;
    std::uint8_t bpp;
    std::uint8_t unary_width;
    std::uint8_t lossy_bits;
    std::uint8_t reserved;
    std::uint64_t timestamp;
    std::uint64_t roi;

    switch(header_bytes) {
        case 4: {
            width  = (reinterpret_cast<std::uint16_t*>(header))[0];
            height = (reinterpret_cast<std::uint16_t*>(header))[1];
            printf("Image read: width = %u, height = %u\n", width, height);
            break;
        }

        case 8: {
            timestamp = (reinterpret_cast<std::uint64_t*>(header))[0];
            width     = width_a;
            height    = height_a;
            printf(
               "Image read: timestamp = %llu, width = %u, height = %u (using user specified width and height)\n",
               timestamp,
               width,
               height);
            break;
        }

        //
        case 16: {

            printf("Reading 16 byte header...\n");
            timestamp          = (reinterpret_cast<std::uint64_t*>(header))[0];
            uint64_t remainder = (reinterpret_cast<std::uint64_t*>(header))[1];

            width  = (reinterpret_cast<std::uint16_t*>(header))[sizeof(uint64_t) / sizeof(uint16_t) + 0];
            height = (reinterpret_cast<std::uint16_t*>(header))[sizeof(uint64_t) / sizeof(uint16_t) + 1];
            // unary_width = (reinterpret_cast<std::uint16_t*>(header))[sizeof(uint64_t) / sizeof(uint16_t) + 2];
            unary_width = header[sizeof(uint64_t) / sizeof(uint8_t) + 2 * sizeof(uint16_t) / sizeof(uint8_t) + 0];
            bpp         = header[sizeof(uint64_t) / sizeof(uint8_t) + 2 * sizeof(uint16_t) / sizeof(uint8_t) + 1];
            lossy_bits  = header[sizeof(uint64_t) / sizeof(uint8_t) + 2 * sizeof(uint16_t) / sizeof(uint8_t) + 2];
            reserved    = header[sizeof(uint64_t) / sizeof(uint8_t) + 2 * sizeof(uint16_t) / sizeof(uint8_t) + 3];

            printf(
               "Image read: timestamp = %llu, header = %016llX, width = %u, height = %u, unary_width = %u, bpp = %u, "
               "lossy_bits "
               "= %u\n",
               timestamp,
               remainder,
               width,
               height,
               unary_width,
               bpp,
               lossy_bits);

            break;
        }

        case 24: {
            // clang-format off
            timestamp   = (reinterpret_cast<std::uint64_t*>(header))[0];
            roi         = (reinterpret_cast<std::uint64_t*>(header))[1];
            width       = (reinterpret_cast<std::uint16_t*>(header))[2*sizeof(uint64_t)/sizeof(uint16_t) + 0];
            height      = (reinterpret_cast<std::uint16_t*>(header))[2*sizeof(uint64_t)/sizeof(uint16_t) + 1];
            unary_width = header[2*sizeof(uint64_t)/sizeof(uint8_t) + 2 * sizeof(uint16_t)/sizeof(uint8_t) + 0];
            bpp         = header[2*sizeof(uint64_t)/sizeof(uint8_t) + 2 * sizeof(uint16_t)/sizeof(uint8_t) + 1];
            lossy_bits  = header[2*sizeof(uint64_t)/sizeof(uint8_t) + 2 * sizeof(uint16_t)/sizeof(uint8_t) + 2];
            reserved    = header[2*sizeof(uint64_t)/sizeof(uint8_t) + 2 * sizeof(uint16_t)/sizeof(uint8_t) + 3];
            // lossy_bits  = saturate_cast<std::uint8_t>(reinterpret_cast<std::uint16_t*>(header))[4 + 3];
            // clang-format on
            printf(
               "Image read: timestamp = %llu, ROI = %llX, width = %u, height = %u, unary_width = %u, bpp = %u, "
               "lossy_bits = "
               "%u\n",
               timestamp,
               roi,
               width,
               height,
               unary_width,
               bpp,
               lossy_bits);
            break;
        }

        default: {
            std::cerr << "Invalid header size: " << header_bytes << ". Should be 4, 8, 16 or 24." << '\n';   //
            // return nullptr;
            break;
        }
            (void)reserved;
    }

    // Seek to the end of the file to get its size
    fpos_t position;
    fgetpos(file, &position);
    fseek(file, 0, SEEK_END);
    long sourceSize = ftell(file);
    fsetpos(file, &position);

    std::vector<std::uint16_t> inputBytes(width * height);
    std::uint64_t expectedDataSize = width * height;
    if(bpp != 8) {
        expectedDataSize = expectedDataSize * 2;   // two bytes per pixel
    }

    bytesRead = fread(inputBytes.data(), 1, (sourceSize - header_bytes), file);

    if(bpp == 8) {
        std::vector<std::uint8_t> inputBytes_8(width * height);
        for(std::size_t i = 0; i < inputBytes.size()/2; i++) { // inputBytes is only half filled
            inputBytes_8[2*i] = (std::uint8_t)(inputBytes[i] & 0xFF);
            inputBytes_8[2*i+1] = (std::uint8_t)(inputBytes[i] >> 8);
        }
        for(std::size_t i = 0; i < inputBytes_8.size(); i++) {
            inputBytes[i] = (std::uint16_t)inputBytes_8[i];
        }
    }

    fclose(file);

    if(bytesRead != expectedDataSize) {
        throw std::runtime_error("Not all bytes read!");
    }

    // Create unique pointer to Image object while moving ownership of inputBytes vector to Image object
    pImage p_img = std::make_unique<Image>(std::move(inputBytes), width, height);

#ifdef TEST_OUT
    auto temp = p_img->getDataView().data();
    //for(uint32_t i = 0; i < 10; i++) {
    //    //uint16_t temp = p_img->m_data[i];
    //    std::cout << "img.pRowData[" << unsigned(i) << "] = " << unsigned(temp[i]) << '\n';
    //}
    //
    //for(uint32_t i = p_img->getLength() - 10; i < p_img->getLength(); i++) {
    //    std::cout << "img.pRowData[" << unsigned(i) << "] = " << unsigned(temp[i]) << '\n';
    //}
    std::cout << '\n' << "------ Test 0.2 --- Bayer[0:1,0:4]-------" << '\n';
    std::cout << unsigned(temp[0]) << " " << unsigned(temp[1]) << " "   //
              << unsigned(temp[2]) << " " << unsigned(temp[3]) << " "   //
              << unsigned(temp[4]) << " " << unsigned(temp[5]) << '\n';
    std::cout << unsigned(temp[0 + width]) << " " << unsigned(temp[1 + width]) << " "   //
              << unsigned(temp[2 + width]) << " " << unsigned(temp[3 + width]) << " "   //
              << unsigned(temp[4 + width]) << " " << unsigned(temp[5 + width]) << '\n';
    std::cout << '\n' << "------ Test 0.2 --- Bayer[end-1:end,end-4:end]----" << '\n';
    std::cout << unsigned(temp[length - width - 6]) << " " << unsigned(temp[length - width - 5]) << " "
              << unsigned(temp[length - width - 4]) << " " << unsigned(temp[length - width - 3]) << " "
              << unsigned(temp[length - width - 2]) << " " << unsigned(temp[length - width - 1]) << '\n';
    std::cout << unsigned(temp[length - 6]) << " " << unsigned(temp[length - 5]) << " "   //
              << unsigned(temp[length - 4]) << " " << unsigned(temp[length - 3]) << " "   //
              << unsigned(temp[length - 2]) << " " << unsigned(temp[length - 1]) << '\n';
#endif

    printf("Image read: width = %u, height = %u\n", width, height);
    // delete[] header;
    // printf("Header deleted\n");
    return p_img;
}

/*
 * Transform Bayer RGB to YCCC color space. Return unique pointer to ImageYCCC object
 * Max value = 2040 Y ch,  255 others
 * Min value =    0 Y ch, -255 others;
 * Y channel is represented by 11 bit unsigned integer
 * other channels            by 9 bit   signed integer
 * Matlab test 1.1
 */
pImageYCCC Helpers::bayer_to_YCCC(const Image* img, std::size_t lossyBits)
{

    auto const width       = img->getWidth();
    auto const height      = img->getHeight();
    auto const imageData   = img->getDataView();
    pImageYCCC p_imgYCCC   = make_imageYCCC(width / 2, height / 2);
    sQuadChannelCS* p_full = p_imgYCCC->getFullChannels();

    for(std::size_t i = 0; i < height / 2; i++) {
        for(std::size_t j = 0; j < width / 2; j++) {
            std::size_t idx   = i * width / 2 + j;
            std::size_t idxGB = 2 * i * width + 2 * j;
            std::size_t idxB  = 2 * i * width + 2 * j + 1;
            std::size_t idxR  = (2 * i + 1) * width + 2 * j;
            std::size_t idxGR = (2 * i + 1) * width + 2 * j + 1;

            Helpers::transformColorGB(
               imageData[idxGB],   //
               imageData[idxB],   //
               imageData[idxR],   //
               imageData[idxGR],   //
               &(p_full->Y[idx]),   //
               &(p_full->Cd[idx]),   //
               &(p_full->Cm[idx]),   //
               &(p_full->Co[idx]),
               lossyBits);   //
        }
    }
#ifdef TEST_OUT
    // clang-format off
            std::cout << '\n' << "------ Test 1.1.2 --[0:3]-------" << '\n';
            std::cout << "Y [0:3]: " << signed(p_full->Y[0])  << " " << signed(p_full->Y[1]) << " "<< signed(p_full->Y[2]) << " "<< signed(p_full->Y[3]) <<'\n';
            std::cout << "Cd[0:3]: " << signed(p_full->Cd[0]) << " " << signed(p_full->Cd[1]) << " " << signed(p_full->Cd[2]) << " " << signed(p_full->Cd[3]) <<'\n';
            std::cout << "Cm[0:3]: " << signed(p_full->Cm[0]) << " " << signed(p_full->Cm[1]) << " " << signed(p_full->Cm[2]) << " " << signed(p_full->Cm[3]) <<'\n';
            std::cout << "Co[0:3]: " << signed(p_full->Co[0]) << " " << signed(p_full->Co[1]) << " " << signed(p_full->Co[2]) << " " << signed(p_full->Co[3]) <<'\n';
            std::cout << '\n' << "------ Test 1.1.2 -----[-3:end]----" << '\n';
            std::cout << "Y [-3:end]: " << signed(p_full-> Y.end()[-4]) << " " << signed(p_full-> Y.end()[-3]) << " " << signed(p_full-> Y.end()[-2]) << " " << signed(p_full-> Y.end()[-1]) <<'\n';
            std::cout << "Cd[-3:end]: " << signed(p_full->Cd.end()[-4]) << " " << signed(p_full->Cd.end()[-3]) << " " << signed(p_full->Cd.end()[-2]) << " " << signed(p_full->Cd.end()[-1]) <<'\n';
            std::cout << "Cm[-3:end]: " << signed(p_full->Cm.end()[-4]) << " " << signed(p_full->Cm.end()[-3]) << " " << signed(p_full->Cm.end()[-2]) << " " << signed(p_full->Cm.end()[-1]) <<'\n';
            std::cout << "Co[-3:end]: " << signed(p_full->Co.end()[-4]) << " " << signed(p_full->Co.end()[-3]) << " " << signed(p_full->Co.end()[-2]) << " " << signed(p_full->Co.end()[-1]) <<'\n';
    // clang-format on
#endif
    return p_imgYCCC;
};

/**
 * Transform from Bayer GB to YCdCmCo color space
 * NOTE: this function return values 8-times larger
 *       to keep fractional part after division.
 * NOTE: max: 4*1*255=1020 (Y ch), 255 others, but not all at the same time.
 * NOTE: min: 0 (Y ch) 1*0 + 0*0 + 0*0 - 1*255 = -255 others
 *
 * Gb | Bl =>  Y  | Cd
 * ---+--- =>  ---+---
 * Rd | Gr =>  Cm | Co
 *
 */
int Helpers::transformColorGB(
   const std::uint16_t gb,
   const std::uint16_t b,
   const std::uint16_t r,
   const std::uint16_t gr,
   std::int16_t* y_o,
   std::int16_t* cd_o,
   std::int16_t* cm_o,
   std::int16_t* co_o,
   std::size_t lossyBits)
{
    // this matrix also scales up by 8
    // clang-format off
    *y_o  = (1 * gr +  1 * r +  1 * b +  1 * gb) >> lossyBits; 
    *cd_o = (1 * gr +  0 * r +  0 * b + -1 * gb) >> lossyBits;
    *cm_o = (1 * gr + -1 * r +  0 * b +  0 * gb) >> lossyBits;
    *co_o = (0 * gr +  1 * r + -1 * b +  0 * gb) >> lossyBits;
    // clang-format on
    return 0;
}

/**
 * Determine whether system is little-endian. Important because values larger than 1 byte
 * are written to files in little-endian order.
 * https://www.gta.ufrj.br/ensino/eel878/sockets/htonsman.html
 * Intel, AMD and ARM processors use little endian order.
 * 
*/
bool Helpers::isLittleEndian()
{
    std::uint16_t dummy16    = 256;   // = 0x0100
    std::uint16_t* p_dummy16 = &dummy16;
    std::uint8_t* p_dummy8   = (std::uint8_t*)p_dummy16;

    if(p_dummy8[0] == (std::uint8_t)0) {
        if(p_dummy8[1] == (std::uint8_t)1) {
            std::cout << "System is Little-Endian" << std::endl;
            return true;
        }
    } else if(p_dummy8[0] == (std::uint8_t)1) {
        if(p_dummy8[1] == (std::uint8_t)0) {
            std::cout << "System is Big-Endian" << std::endl;
            return false;
        }
    }
    throw std::runtime_error("There is something odd with determining endianess.!");
    return NULL;
}

/**
 * On Intel, AMD and ARM processors this dumps data in little endian order (MSB bits last, i.e. at a higher address)
*/
void Helpers::dumpToFile(const char* fileName, const sQuadChannelCS* quadCh, std::size_t width, std::size_t height)
{
    std::ofstream wf(fileName, std::ios::out | std::ios::binary);
    if(!wf) {
        char msg[200];
        sprintf(msg, "Cannot open specified file: %s", fileName);
        throw std::runtime_error(msg);
    }

    std::uint16_t widthHeight[2];
    widthHeight[0] = width;
    widthHeight[1] = height;
    wf.write((char*)widthHeight, 4); /* Little endian order */

    using sQ = sQuadChannelCS::Channel;
    /* Little endian order, Write twice as many bytes */
    wf.write((char*)quadCh->getChannelDataConst(sQ::Y), 2 * quadCh->getChannelSizeConst(sQ::Y));
    wf.write((char*)quadCh->getChannelDataConst(sQ::Cd), 2 * quadCh->getChannelSizeConst(sQ::Cd));
    wf.write((char*)quadCh->getChannelDataConst(sQ::Cm), 2 * quadCh->getChannelSizeConst(sQ::Cm));
    wf.write((char*)quadCh->getChannelDataConst(sQ::Co), 2 * quadCh->getChannelSizeConst(sQ::Co));

    wf.close();
};

/**
 * On Intel, AMD and ARM processors this dumps data in little endian order (MSB bits last, i.e. at a higher address)
*/
void Helpers::dumpToFile(const char* fileName, const uQuadChannelCS* quadCh, std::size_t width, std::size_t height)
{
    std::ofstream wf(fileName, std::ios::out | std::ios::binary);
    if(!wf) {
        char msg[200];
        sprintf(msg, "Cannot open specified file: %s", fileName);
        throw std::runtime_error(msg);
    }

    std::uint16_t widthHeight[2];
    widthHeight[0] = width;
    widthHeight[1] = height;
    wf.write((char*)widthHeight, 4); /* Little endian order */

    using uQ = uQuadChannelCS::Channel;
    /* Little endian order, Write twice as many bytes */
    wf.write((char*)quadCh->getChannelDataConst(uQ::Y), 2 * quadCh->getChannelSizeConst(uQ::Y));
    wf.write((char*)quadCh->getChannelDataConst(uQ::Cd), 2 * quadCh->getChannelSizeConst(uQ::Cd));
    wf.write((char*)quadCh->getChannelDataConst(uQ::Cm), 2 * quadCh->getChannelSizeConst(uQ::Cm));
    wf.write((char*)quadCh->getChannelDataConst(uQ::Co), 2 * quadCh->getChannelSizeConst(uQ::Co));

    wf.close();
};