#pragma once

#include "Channels.hpp"
#include <cstdint>
#include <memory>

/*
Class for ImageYCCC Image with Y, Cd, Cm, Co channels.
*/
class ImageYCCC
{
    std::size_t m_width;
    std::size_t m_height;
    std::size_t m_length;
    sQuadChannelCS m_full; /*Max 16320 Y ch, 255 others; min -255*/
    sQuadChannelCS m_dpcm; /*Max 16320-(-255) = 16575; min -255-0 = -255*/
  public:
    //  Constructor:
    ImageYCCC(std::size_t w, std::size_t h);

    sQuadChannelCS* getFullChannels();
    const sQuadChannelCS* getFullChannelsConst() const;

    std::size_t getWidth() const;
    std::size_t getHeight() const;

    void dumpFullToFile(const char* fileName);
};

// use of rvalue referencess val&&
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2006/n2027.html#Move_Semantics

using pImageYCCC = std::unique_ptr<ImageYCCC>;
pImageYCCC make_imageYCCC(std::size_t width, std::size_t height);
