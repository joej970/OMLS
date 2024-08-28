#pragma once

#define DUMP_VERIFICATION

// #define k_MIN 2
// #define k_MAX 11 // no need for higher value
#define N_START \
    4 + 1   // this is because of parallel implementation on FPGA where first pixel is calculated but values are ignored
#define A_MIN 0
#define k_SEED \
    11   // because seed will be max 2040, thus max 11 bits required for unsigned, one "0" bit will be added as delimiter, totaling to 12 bits (when decoding)
#define C_MAX_UNARY_LENGTH_FULL (2040 + 1) /* Unary length when compressor switches to binary coding of positive value*/
#define C_MAX_UNARY_LENGTH (8) /* Unary length when compressor switches to binary coding of positive value*/

#define RAW_HEADER_SIZE 16 /* Size of initial raw image size. Timestamp + ROI */