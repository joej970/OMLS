#include <fstream>
#include <iostream>

#include "ImageYCCC.hpp"
#include "helpers.hpp"

/**
 * Supply width and height of each channel.
 * Each channel is half the size compared to BayerCFA image.
 *
 * G B | G B     Y Y | C C
 * R G | R G     Y Y | C C
 * ----+----  =  ----+----
 * G B | G B     C C | C C
 * R G | R G     C C | C C
 *
 */
ImageYCCC::ImageYCCC(std::size_t w, std::size_t h)
   : m_width(w)
   , m_height(h)
   , m_length(w * h)
   , m_full(m_length)
   , m_dpcm(m_length)
{
}

/**
 * Make unique pointer to imageYCCC object.
*/
pImageYCCC make_imageYCCC(std::size_t width, std::size_t height)
{
    return std::make_unique<ImageYCCC>(width, height);
}

/**
 * Getters
*/
sQuadChannelCS* ImageYCCC::getFullChannels()
{
    return &m_full;
};
const sQuadChannelCS* ImageYCCC::getFullChannelsConst() const
{
    return &m_full;
};

std::size_t ImageYCCC::getWidth() const
{
    return m_width;
};

std::size_t ImageYCCC::getHeight() const
{
    return m_height;
};

/* Dump full values to file.*/
void ImageYCCC::dumpFullToFile(const char* fileName)
{
    Helpers::dumpToFile(fileName, getFullChannels(), getWidth(), getHeight());
};
