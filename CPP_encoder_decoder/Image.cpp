#include "Image.hpp"
#include <iostream>
#include <span>

Image::Image(std::vector<std::uint16_t>&& data, std::size_t w, std::size_t h)
   : m_width(w)
   , m_height(h)
   , m_length(w * h)
   , m_bitDepth(8)
   , m_colorDepth(1)
   , m_data(std::move(data))
{
    for(uint8_t p = 0; p < m_bitDepth; p++) {   //
        m_colorDepth = m_colorDepth * 2;
    }
};

std::size_t Image::getWidth() const
{
    return m_width;
};
std::size_t Image::getHeight() const
{
    return m_height;
};
std::size_t Image::getLength() const
{
    return m_length;
};
std::uint16_t Image::getColorDepth() const
{
    return m_colorDepth;
};

std::span<std::uint16_t> Image::getData()
{
    return {m_data.data(), m_data.size()};
}

std::span<std::uint16_t const> Image::getDataView() const
{
    return {m_data.data(), m_data.size()};
}

/**
 * Make unique pointer to imageYCCC object.
*/
pImage make_image(std::vector<std::uint16_t>&& data, std::size_t width, std::size_t heigth)
{
    return std::make_unique<Image>(std::forward<std::vector<std::uint16_t>>(data), width, heigth);
}
