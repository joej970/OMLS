
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

class Image
{
  private:
    std::size_t m_width        = 0;
    std::size_t m_height       = 0;
    std::size_t m_length       = 0;
    std::uint8_t m_bitDepth    = 0;
    std::uint16_t m_colorDepth = 0;
    std::vector<std::uint16_t> m_data;

  public:
    //  Constructor:
    Image(std::vector<std::uint16_t>&& data, std::size_t w, std::size_t h);
    // Destructor called automatically once function instantiating the object returns
    std::size_t getWidth() const;
    std::size_t getHeight() const;
    std::size_t getLength() const;
    std::uint16_t getColorDepth() const;

    std::span<std::uint16_t> getData();
    std::span<std::uint16_t const> getDataView() const;
};

using pImage = std::unique_ptr<Image>;
pImage make_image(std::vector<std::uint16_t>&& data, std::size_t width, std::size_t heigth);
