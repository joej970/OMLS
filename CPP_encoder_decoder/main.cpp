
#include <bitset>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Decoder.hpp"
#include "Encoder.hpp"
#include "Image.hpp"
#include "ImageYCCC.hpp"
#include "helpers.hpp"
#include "main.hpp"


// example command:
// main.exe -i C:/DATA/Repos/OMLS_Masters_SW/images/lite_dataset -o C:/DATA/Repos/OMLS_Masters_SW/images/lite_dataset -s 0 -e 9 -l 0 -b 16 -f img_ -r 8 -d -c

int main(int argc, char* argv[])
{

    Params params = parseArguments(argc, argv);

    std::filesystem::path cwd = std::filesystem::current_path();

    std::cout << "Main program running at directory: " << cwd.string() << std::endl;

    if(!Helpers::isLittleEndian()) {
        std::cerr << "System is not little endian. Problems will occur with reading and writing binary files.";
    }

    createMissingDirectories(params.folder_out);

    /* Compress */
    std::vector<std::uint32_t> N{8};
    std::vector<std::uint32_t> A_init{32};
    // this will be populated with width and height of each image.
    // this can be removed later when width and height is encdoded in the header
    std::vector<std::size_t> widthHeight;

    if(params.header_bytes != 4 && params.header_bytes != 16) {
        for(std::size_t imgIdx = params.imgIdx_min; imgIdx <= params.imgIdx_max; imgIdx++) {
            widthHeight.push_back(params.width);
            widthHeight.push_back(params.height);
        }
    }

    if(params.ideal_compress) {
        compressImageRangeIdeal(
           params.fileName,
           params.folder_in,
           params.folder_out,
           params.imgIdx_min,
           params.imgIdx_max,
           params.unaryMaxWidth,
           params.bpp,
           params.lossyBits,
           &widthHeight,
           16);
    } else if(params.compress) {
        compressImageRangeAGOR(
           params.fileName,
           params.folder_in,
           params.folder_out,
           params.imgIdx_min,
           params.imgIdx_max,
           params.unaryMaxWidth,
           params.bpp,
           &N,
           &A_init,
           params.lossyBits,
           &widthHeight,
           16);
        if(params.decompress) {
            decompressImageRangeAGOR(
               params.fileName,
               params.folder_out,
               params.folder_out,
               params.imgIdx_min,
               params.imgIdx_max,
               params.unaryMaxWidth,
               params.bpp,
               &N,
               &A_init,
               params.lossyBits,
               &widthHeight,
               //    24, // 24 if you want to include header (width[15:0], height[15:0], unary_width[7:0], bpp[7:0], lossy_bits[7:0], reserved). If you want to verify decompressed with original using Winmerge, set this to 16 (exclude header)
               16,
               params.use_gpu);
        }
    } else if(params.decompress) {
        for(std::size_t imgIdx = params.imgIdx_min; imgIdx <= params.imgIdx_max; imgIdx++) {
            widthHeight.push_back(params.width);
            widthHeight.push_back(params.height);
        }   // end for
        decompressImageRangeAGOR(
           params.fileName,
           params.folder_in,
           params.folder_out,
           params.imgIdx_min,
           params.imgIdx_max,
           params.unaryMaxWidth,
           params.bpp,
           &N,
           &A_init,
           params.lossyBits,
           &widthHeight,
           params.header_bytes,
           params.use_gpu);
    }

    // if((params.p_ideal_compress == 'n') & (params.p_compress == 'n') & (params.p_decompress == 'n')) {
    //     std::cout << "Usage: input location | output location | min index of xx | max index of xx for img_xx.bin | "
    //                  "lossy_bits | minimum k param. | compress enable y/n | ideal y/n | decompress enable y/n  | "
    //                  "[width] | [height]";
    // }

    std::cout << "Program finished" << std::endl;
}

/**
 * Compresses BayerCFA binary files with name img_xx.bin from base folder "folder"
 * 
*/
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
   std::size_t headerBytes)
{
    std::cout << "\nAGOR compression with Q max width: " << unsigned(unaryMaxWidth) << std::endl;
    char path[200];

    using namespace std;

    static std::ofstream wf_report;
    sprintf(path, "%s/compressed/report.csv", folder_out);
    wf_report.open(path, std::ios::app);
    if(!wf_report) {
        char msg[200];
        std::sprintf(msg, "Cannot open specified file: %s", path);
        throw std::runtime_error(msg);
    }

    for(std::size_t imgIdx = imgIdx_min; imgIdx <= imgIdx_max; imgIdx++) {

        sprintf(path, "%s/%s%02zu.bin", folder_in, fileName, imgIdx);
        cout << "Check if exist: " << path << endl;
        // check if file exists
        if(!std::filesystem::exists(path)) {
            sprintf(path, "%s/bayerCFA_GB/%s%02zu.bin", folder_in, fileName, imgIdx);
        }
        if(!std::filesystem::exists(path)) {
            std::cerr << "Did you put binary files into bayerCFA_GB folder? File does not exist: " << path << std::endl;
        }
        cout << "\nReading image: " << path << endl;

        // Read binary image
        // using pImage = std::unique_ptr<Image>
        std::unique_ptr<Image> pImg = nullptr;
        if(headerBytes == 4 || headerBytes == 16 || headerBytes == 24) {
            // width/height will be read from the header
            std::unique_ptr<Image> pImg_temp = Helpers::read_image(path, headerBytes, 0, 0);
            imageSizes->push_back(pImg_temp->getWidth());   // add width info
            imageSizes->push_back(pImg_temp->getHeight());   // add height info
            printf("imageSizes->size() = %zu\n", imageSizes->size());
            printf("imageSizes->data()[%zu] = %zu\n", imgIdx - imgIdx_min, imageSizes->data()[imgIdx - imgIdx_min]);
            printf(
               "imageSizes->data()[%zu] = %zu\n",
               imgIdx - imgIdx_min + 1,
               imageSizes->data()[imgIdx - imgIdx_min + 1]);
            // pImg.reset(pImg_temp.get());
            pImg.reset(pImg_temp.release());
        } else {

            std::unique_ptr<Image> pImg_temp = Helpers::read_image(
               path,
               headerBytes,
               imageSizes->data()[imgIdx - imgIdx_min],   // width already available from the user
               imageSizes->data()[imgIdx - imgIdx_min + 1]);   // height already available from the user

            pImg.reset(pImg_temp.release());
        }

        cout << "Read raw image binary, width: " << pImg->getWidth() << ", height: " << pImg->getHeight() << '\n';
        cout << "Original file size: " << unsigned(pImg->getDataView().size()) << " Bytes" << endl;

#ifdef DUMP_VERIFICATION
        sprintf(path, "%s/dump/%s%02zu_16pp.txt", folder_out, fileName, imgIdx);
        Helpers::dump16pp(path, pImg.get());
        sprintf(path, "%s/dump/%s%02zu_8pp.txt", folder_out, fileName, imgIdx);
        Helpers::dump8pp(path, pImg.get());
#endif

        // AGOR
        sprintf(path, "%s/compressed/%s%02zu.bin", folder_out, fileName, imgIdx);
        std::cout << "Output file: " << path << std::endl;

        // ACTUAL COMPRESSION
        Encoder enc{
           pImg.get(),
           pImg->getWidth(),
           pImg->getHeight(),
           folder_out,
           imgIdx,
           A_init->data()[0],
           N->data()[0],
           lossyBits,
           unaryMaxWidth,
           bpp,
           24,
           fileName};

        std::unique_ptr<std::vector<std::size_t>> fileSize;
        if(unaryMaxWidth == (2040 + 1)) {
            fileSize = enc.encodeUsingMethod(Encoder::method::parallel_standard);
        } else {
            fileSize = enc.encodeUsingMethod(Encoder::method::parallel_limited);
        }
        // DONE

        cout << "Parallel encoder: "
             << "N/A: " << unsigned(N->data()[0]) << "/" << unsigned(A_init->data()[0])
             << ", max Q width: " << unsigned(unaryMaxWidth) << ", file size: " << unsigned(fileSize->data()[0])
             << " bytes" << endl;

#ifdef DUMP_VERIFICATION
        translateBinaryToASCII_hex(path);
        translateBinaryToASCII_bin(path);
#endif
        //  auto current_time = std::chrono::system_clock::now();
        // wf_report << std::ctime(&current_time) << " : img_" << unsigned(imgIdx)
        //           << ".png : " << unsigned(lossyBits)
        //           << " lossy bits, ideal N: " << unsigned(enc.getFileSize()) << " bytes" << std::endl;

        std::time_t current_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char msg[200];
        struct tm* p = localtime(&current_time);
        strftime(msg, sizeof(msg), "%a %b %m %Y %H:%M:%S", p);

        wf_report << msg << " , " << fileName << unsigned(imgIdx) << ".png , " << unsigned(lossyBits)
                  << " lossy bits, AGOR , " << unsigned(enc.getFileSize()) << " ,bytes"
                  << ",max unary length," << unsigned(unaryMaxWidth) << std::endl;
    }
    wf_report.close();
}
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
   std::size_t headerBytes)
{
    std::cerr << "Ideal compression not verified, probably broken." << std::endl;
    std::cerr << "Ideal compression not verified, probably broken." << std::endl;
    std::cerr << "Ideal compression not verified, probably broken." << std::endl;
    std::cerr << "Ideal compression not verified, probably broken." << std::endl;
    std::cerr << "Ideal compression not verified, probably broken." << std::endl;

    std::cout << "Ideal compression" << std::endl;
    char path[200];

    static std::ofstream wf_report;
    sprintf(path, "%s/compressed/report.csv", folder_out);
    wf_report.open(path, std::ios::app);
    if(!wf_report) {
        char msg[200];
        std::sprintf(msg, "Cannot open specified file: %s", path);
        throw std::runtime_error(msg);
    }

    using namespace std;

    for(std::size_t imgIdx = imgIdx_min; imgIdx <= imgIdx_max; imgIdx++) {
        sprintf(path, "%s/bayerCFA_GB/%s%02zu.bin", folder_in, fileName, imgIdx);
        cout << "\n\nReading image: " << path << endl;
        // Read binary image
        pImage pImg = Helpers::read_image(path, headerBytes, 0, 0);
        cout << "Read raw image binary, width: " << pImg->getWidth() << ", height: " << pImg->getHeight() << '\n';
        cout << "Original file size: " << unsigned(pImg->getDataView().size() / 1024) << " kBytes" << endl;

        sprintf(path, "%s/dump/%s%02zu_%zu_%04zu_16pp.txt", folder_out, fileName, imgIdx, lossyBits, unaryMaxWidth);
        imageSizes->push_back(pImg->getWidth());
        imageSizes->push_back(pImg->getHeight());

        // Ideal
        auto pImg_YCCC = Helpers::bayer_to_YCCC(pImg.get(), lossyBits);
        cout << "YCCC image size, width: " << pImg_YCCC->getWidth() << ", height: " << pImg_YCCC->getHeight() << '\n';

        std::unique_ptr<std::vector<std::size_t>> p_fileSize;   // = fileSize;

        sprintf(
           path,
           "%s/compressed/%s%02zu_%zu_%04zu_ideal.bin",
           folder_out,
           fileName,
           imgIdx,
           lossyBits,
           unaryMaxWidth);
        cout << "Output file: " << path << endl;

        // ACTUAL COMPRESSION IDEAL
        Encoder enc{pImg_YCCC.get(), folder_out, imgIdx, lossyBits, headerBytes};
        p_fileSize = enc.encodeUsingMethod(Encoder::method::ideal);
        // DONE

        cout << "Ideal encoder: "
             << "img " << unsigned(imgIdx) << " Y ch: " << unsigned(p_fileSize->at(0))
             << " bytes, Cd ch: " << unsigned(p_fileSize->at(1)) << " bytes, Cm ch: " << unsigned(p_fileSize->at(2))
             << " bytes, Co ch: " << unsigned(p_fileSize->at(3)) << "  bytes." << endl;
        auto total_size = p_fileSize->at(0) + p_fileSize->at(1) + p_fileSize->at(2) + p_fileSize->at(3);
        cout << "Total file size: " << unsigned(total_size) << " bytes" << endl;

        std::time_t current_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        // std::time_t current_time;   // = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char msg[200];
        struct tm* p = localtime(&current_time);
        strftime(msg, sizeof(msg), "%a %b %m %Y %H:%M:%S", p);

        wf_report << msg << " , " << fileName << unsigned(imgIdx) << ".png , " << unsigned(lossyBits)
                  << " lossy bits, ideal , " << unsigned(total_size) << ", bytes" << std::endl;

    }   // end for
    wf_report.close();
}

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
   bool use_gpu)
{

    std::cout << "\nAGOR decompression" << std::endl;
    char path[200];
    std::size_t it = 0;

    std::cout << "Image sizes: ";
    for(std::size_t i = 0; i < imageSizes->size(); i++) {   //
        std::cout << imageSizes->data()[i] << " ";
    }
    std::cout << std::endl;

    for(std::size_t imgIdx = imgIdx_min; imgIdx <= imgIdx_max; imgIdx++) {

        // PARELLEL IMPLEMENTATION
        try {
            std::cout << "\nParallel implementation:" << std::endl;
            sprintf(path, "%s/compressed/%s%02zu.bin", folder_in, fileName, imgIdx);
            std::cout << "\nLoading image at " << path << std::endl;
            // Decoder
            //    dec{path, imageSizes->data()[2 * it], imageSizes->data()[2 * it + 1], A_init->data()[0], N->data()[0]};
            Decoder dec{path, A_init->data()[0], N->data()[0]};
            std::cout << "\nLoaded image at " << path << std::endl;
#ifdef TIMING_EN
            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
#endif
            headerData_t headerData;
            if(use_gpu) {
                headerData = dec.decodeParallelGPU();
            } else {
                headerData = dec.decodeParallel();
            }
#ifdef TIMING_EN
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
#endif

            sprintf(path, "%s/decompressed/%s%02zu.bin", folder_out, fileName, imgIdx);
            if(std::filesystem::exists(path)) {
                // Delete the file
                std::cout << "Deleting existing file: " << path << " \n";
                try {
                    std::filesystem::remove(path);
                    std::cout << "File deleted successfully.\n";
                } catch(const std::exception& e) {
                    std::cerr << "Error deleting file: " << e.what() << "\n";
                }
            }
            // if headerBytes == 4, then it is old binary with no header
            // ( well actually 4 bytes header with width and height) --- well this appends 8 bytes to the file so it will be wrong anyways
            // std::uint64_t header = (headerBytes == 4) ? 1 : 0;
            // header = ()
            // get timestamp in microseconds
            // std::uint64_t timestamp = getCurrentTimeMicros();
            std::uint64_t timestamp = headerData.timestamp;
            std::uint64_t roi       = headerData.roi;

            std::uint64_t header = 0;
            if(headerBytes == 24) {
                header |= (std::uint64_t)headerData.reserved << 56;
                header |= (std::uint64_t)headerData.lossyBits << 48;
                header |= (std::uint64_t)headerData.bpp << 40;
                header |= (std::uint64_t)headerData.unaryMaxWidth << 32;
                header |= (std::uint64_t)headerData.height << 16;
                header |= (std::uint64_t)headerData.width << 0;
            }

            dec.exportBayerImage(path, header, roi, timestamp);
            std::cout << "\nSaved an image." << std::endl;
#ifdef TIMING_EN
            std::chrono::steady_clock::time_point end_exported = std::chrono::steady_clock::now();
            // std::cout << "Parallel decoding time = "
            //           << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "[us]"
            //           << std::endl;
            std::cout << "Parallel decoding time = "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]"
                      << std::endl;
            std::cout << "Exporting time = "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(end_exported - end).count() << "[ms]"
                      << std::endl;
#endif
        } catch(std::runtime_error& e) {
            std::cout << "RUNTIME ERROR: \n";
            std::cout << e.what() << "\n";
        }
        it++;
    }
}

std::uint64_t getCurrentTimeMicros()
{
    auto now      = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

void runTests()
{
    char folder[] = "../images/optomotive_test";
    createMissingDirectories(folder);

    Helpers::isLittleEndian();
}

void createMissingDirectories(const char* folder_out)
{
    /** Create missing directories*/
    char path[200];
    std::cout << "Proposed out location is: " << folder_out << std::endl;
    sprintf(path, "%s/decompressed/", folder_out);
    if(!std::filesystem::exists(path)) {
        if(std::filesystem::create_directory(path)) {
            std::cout << "Created new directory: " << path << std::endl;
        }
    }
    sprintf(path, "%s/compressed/", folder_out);
    if(!std::filesystem::exists(path)) {
        if(std::filesystem::create_directory(path)) {
            std::cout << "Created new directory: " << path << std::endl;
        }
    }
    sprintf(path, "%s/dump/", folder_out);
    if(!std::filesystem::exists(path)) {
        if(std::filesystem::create_directory(path)) {
            std::cout << "Created new directory: " << path << std::endl;
        }
    }
}

void translateBinaryToASCII_hex(char* fileNameIn)
{
    // copy input file name
    auto fileName = std::strcpy(new char[200], fileNameIn);
    // char fileName_bin[200];
    // sprintf(fileName_bin, "%s.bin", fileName);
    std::ifstream rf_bin(fileName, std::ios::binary);
    if(!rf_bin) {
        char msg[200];
        std::sprintf(msg, "Cannot open specified file: %s", fileName);
        throw std::runtime_error(msg);
    }

    printf("Reading file: %s\n", fileName);
    char fileName_ascii[200];
    // remove ".bin" from fileName
    fileName[strlen(fileName) - 4] = '\0';

    sprintf(fileName_ascii, "%s_hex.txt", fileName);
    std::ofstream wf_ascii(fileName_ascii, std::ios::out);
    if(!wf_ascii) {
        char msg[200];
        std::sprintf(msg, "Cannot open specified file: %s", fileName_ascii);
        throw std::runtime_error(msg);
    }
    printf("Writing file: %s\n", fileName_ascii);
    std::uint8_t byte;
    std::uint8_t bit;
    std::uint8_t bit_cnt   = 0;
    std::uint8_t ascii_cnt = 0;
    std::uint8_t ascii     = 0;
    while(rf_bin.read(reinterpret_cast<char*>(&byte), sizeof(byte))) {
        for(std::uint8_t i = 0; i < 8; i++) {
            bit = (byte >> i) & 1;
            ascii |= bit << bit_cnt;
            bit_cnt++;
            if(bit_cnt == 8) {
                // write binary representation of ascii to file
                // wf_ascii << std::bitset<8>(ascii);
                // write hex representation of ascii to file
                wf_ascii << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << unsigned(ascii) << " ";
                // wf_ascii << std::hex << ascii;
                // wf_ascii << ascii;
                ascii   = 0;
                bit_cnt = 0;
                ascii_cnt++;
                if(ascii_cnt == 64 / 8) {
                    wf_ascii << "' ";
                }
                if(ascii_cnt == 128 / 8) {
                    wf_ascii << std::endl;
                    ascii_cnt = 0;
                }
            }
        }
    }
    rf_bin.close();
    wf_ascii.close();
}

void translateBinaryToASCII_bin(char* fileNameIn)
{
    auto fileName = std::strcpy(new char[200], fileNameIn);
    // char fileName_bin[200];
    // sprintf(fileName_bin, "%s.bin", fileName);
    std::ifstream rf_bin(fileName, std::ios::binary);
    if(!rf_bin) {
        char msg[200];
        std::sprintf(msg, "Cannot open specified file: %s", fileName);
        throw std::runtime_error(msg);
    }

    printf("Reading file: %s\n", fileName);
    char fileName_ascii[200];
    // remove ".bin" from fileName
    fileName[strlen(fileName) - 4] = '\0';

    sprintf(fileName_ascii, "%s_bin.txt", fileName);
    std::ofstream wf_ascii(fileName_ascii, std::ios::out);
    if(!wf_ascii) {
        char msg[200];
        std::sprintf(msg, "Cannot open specified file: %s", fileName_ascii);
        throw std::runtime_error(msg);
    }
    printf("Writing file: %s\n", fileName_ascii);
    std::uint8_t byte;
    std::uint8_t bit;
    std::uint8_t bit_cnt   = 0;
    std::uint8_t ascii_cnt = 0;
    std::uint8_t ascii     = 0;
    while(rf_bin.read(reinterpret_cast<char*>(&byte), sizeof(byte))) {
        for(std::uint8_t i = 0; i < 8; i++) {
            bit = (byte >> i) & 1;
            ascii |= bit << bit_cnt;
            bit_cnt++;
            if(bit_cnt == 8) {
                // write binary representation of ascii to file
                wf_ascii << std::bitset<8>(ascii);
                ascii   = 0;
                bit_cnt = 0;
                ascii_cnt++;
                if(ascii_cnt == 128 / 8) {
                    wf_ascii << std::endl;
                    ascii_cnt = 0;
                }
            }
        }
    }
    rf_bin.close();
    wf_ascii.close();
}

void printHelp()
{
    std::cout << "NOTE: Consider using python scripts to run this executable. \n"
              << "Usage:\n"
              << "-i input_location (where bayerCFA_GB folder is)\n"
              << "-o output_location\n"
              << "-s min_index\n"
              << "-e max_index\n"
              << "-c (add for compress)\n"
              << "-d (add for decompress)\n"
              << "-p (add for ideal compress) \n"
              << "[-f filename (default 'img_' for 'img_xx.bin')]"
              << "[-l lossy_bits, default 0] \n"
              << "[-u unary_max_width]\n"
              << "[-b header_bytes (for input file, default 16 (if not specified or set to 0, specify image width and "
                 "height); compressed file always has 24 bytes)\n]"
              << "[-x width -y height] (necesarry only if header == 0)"
              << "[-r bpp] (resolution in bits per pixel, default 8)\n"
              << std::endl;
}

Params parseArguments(int argc, char* argv[])
{
    Params params;
    // params.fileName       = "img_";
    sprintf(params.fileName, "img_");
    params.folder_in      = nullptr;
    params.folder_out     = nullptr;
    params.imgIdx_min     = 0;
    params.imgIdx_max     = 0;
    params.lossyBits      = 0;
    params.unaryMaxWidth  = C_MAX_UNARY_LENGTH;
    params.width          = 0;
    params.height         = 0;
    params.compress       = false;
    params.decompress     = false;
    params.ideal_compress = false;
    params.header_bytes   = 16;
    params.bpp            = 8;
    params.use_gpu        = false;

    if(argc == 1) {
        std::cout << "No arguments supplied." << std::endl;
        std::cout << "Continuing with default values." << std::endl;
        printHelp();
        exit(EXIT_FAILURE);
        sprintf(params.fileName, "gpu_img_");
        params.folder_in      = "../images/lite_dataset";
        params.folder_out     = "../images/lite_dataset";
        params.imgIdx_min     = 0;
        params.imgIdx_max     = 3;
        params.width          = 0;
        params.height         = 0;
        params.lossyBits      = 0;
        params.compress       = true;
        params.decompress     = true;
        params.ideal_compress = false;
        params.header_bytes   = 16;
        params.bpp            = 8;
        params.use_gpu        = true;
    } else {
        for(int i = 1; i < argc; i += 2) {
            const char* flag = argv[i];

            if(std::strcmp(flag, "-i") == 0) {
                params.folder_in = argv[i + 1];
            } else if(std::strcmp(flag, "-o") == 0) {
                params.folder_out = argv[i + 1];
            } else if(std::strcmp(flag, "-s") == 0) {
                params.imgIdx_min = std::stoi(argv[i + 1]);
            } else if(std::strcmp(flag, "-e") == 0) {
                params.imgIdx_max = std::stoi(argv[i + 1]);
            } else if(std::strcmp(flag, "-l") == 0) {
                params.lossyBits = std::stoi(argv[i + 1]);
            } else if(std::strcmp(flag, "-u") == 0) {
                params.unaryMaxWidth = std::stoi(argv[i + 1]);
            } else if(std::strcmp(flag, "-c") == 0) {
                params.compress = true;
                i--;
            } else if(std::strcmp(flag, "-d") == 0) {
                params.decompress = true;
                i--;
            } else if(std::strcmp(flag, "-p") == 0) {
                params.ideal_compress = true;
                i--;   // single parameter
            } else if(std::strcmp(flag, "-x") == 0) {
                params.width = std::stoi(argv[i + 1]);
            } else if(std::strcmp(flag, "-y") == 0) {
                params.height = std::stoi(argv[i + 1]);
            } else if(std::strcmp(flag, "-b") == 0) {
                params.header_bytes = std::stoi(argv[i + 1]);
            } else if(std::strcmp(flag, "-f") == 0) {
                std::strcpy(params.fileName, argv[i + 1]);
                // params.fileName = argv[i + 1];
            } else if(std::strcmp(flag, "-r") == 0) {
                params.bpp = std::stoi(argv[i + 1]);
            } else if(std::strcmp(flag, "-g") == 0) {
                params.use_gpu = true;
            } else {
                std::cerr << "Invalid flag: " << flag << std::endl;
                printHelp();
                exit(EXIT_FAILURE);
            }
        }
    }

    std::cout << "Current filename: " << params.fileName << std::endl;

    char temp_buffer[200];
    if(params.use_gpu) {
        // sprintf(params.fileName, "gpu_%s%dbpp_", params.fileName, params.bpp);
        sprintf(temp_buffer, "gpu_%s%dbpp_", params.fileName, params.bpp);
    } else {
        // sprintf(params.fileName, "%s%dbpp_", params.fileName, params.bpp);
        sprintf(temp_buffer, "%s%dbpp_", params.fileName, params.bpp);
    }
    std::strcpy(params.fileName, temp_buffer);

    // Check for missing required input arguments
    if(params.folder_in == nullptr || params.folder_out == nullptr ||
       (params.compress == false && params.decompress == false)) {
        std::cerr << "Missing required input arguments." << std::endl;
        printHelp();
        exit(EXIT_FAILURE);
    }

    if(params.header_bytes == 0) {
        if(params.width == 0 || params.height == 0) {
            std::cerr << "Missing width and height info. Specify width and height by -x and -y flags, respectively"
                      << std::endl;
            printHelp();
            exit(EXIT_FAILURE);
        }
    } else {
        printf(
           "Header bytes: %zu. Compression info (width, height, lossy bits) will be read from the header.\n",
           params.header_bytes);
    }

    // if(params.header_bytes != 0 && params.header_bytes != 4 && params.header_bytes != 8 && params.header_bytes != 16 && params.header_bytes != 24) {
    //     std::cerr << "Invalid header size. Must be 0, 4, 8, 16 or 24." << std::endl;
    //     printHelp();
    //     exit(EXIT_FAILURE);
    // }

    // if(params.header_bytes != 16 && params.header_bytes != 4) {
    //     if(params.width == 0 || params.height == 0) {
    //         std::cerr << "Header might not contain width and height info. Specify width and height by -x and -y flags, "
    //                      "respectively"
    //                   << std::endl;
    //         printHelp();
    //         exit(EXIT_FAILURE);
    //     }
    // }

    // if(params.compress == false && params.header_bytes != 16 && (params.width == 0 || params.height == 0)) {
    //     std::cerr << "Header might not contain width and height info. Specify width and height by -x and -y flags, "
    //                  "respectively"
    //               << std::endl;
    //     printHelp();
    //     exit(EXIT_FAILURE);
    // }

    std::cout << "Running decompression with parameters:" << std::endl;
    std::cout << "            fileName: '" << params.fileName << "', effectively: '" << params.fileName << "xx.bin'."
              << std::endl;
    std::cout << "        image folder: " << params.folder_in << std::endl;
    std::cout << "       output folder: " << params.folder_out << std::endl;
    std::cout << "              idxMin: " << params.imgIdx_min << std::endl;
    std::cout << "              idxMax: " << params.imgIdx_max << std::endl;
    // std::cout << " width: " << params.width << std::endl;
    // std::cout << " height: " << params.height << std::endl;
    std::cout << "           lossyBits: " << params.lossyBits << std::endl;
    std::cout << "       unaryMaxWidth: " << params.unaryMaxWidth << std::endl;
    std::cout << "      bits per pixel: " << params.bpp << std::endl;
    std::cout << "       compress flag: " << (params.compress ? "true" : "false") << std::endl;
    std::cout << "     decompress flag: " << (params.decompress ? "true" : "false") << std::endl;
    std::cout << " ideal compress flag: " << (params.ideal_compress ? "true" : "false") << std::endl;
    std::cout << "        header_bytes: " << params.header_bytes << std::endl;
    if(params.header_bytes == 0) {
        std::cout << "               width: " << params.width << std::endl;
        std::cout << "              height: " << params.height << std::endl;
    }

    return params;
}
