#pragma once

#include <cstdint>
#include <span>
#include <vector>

/**
 * Quad channel color space signed.
*/
struct sQuadChannelCS {
    // maybe make it more general, eg. ch1, ch2 instead of Y, Cd,..
    enum class Channel
    {
        Y,
        Cd,
        Cm,
        Co
    };

    std::vector<std::int16_t> Y;
    std::vector<std::int16_t> Cd;
    std::vector<std::int16_t> Cm;
    std::vector<std::int16_t> Co;

    explicit sQuadChannelCS();
    explicit sQuadChannelCS(std::size_t length);
    std::span<std::int16_t> getChannel(Channel channel);
    std::span<std::int16_t> getChannel(std::size_t chIdx);
    std::span<std::int16_t const> getChannelView(Channel channel) const;
    const std::int16_t* getChannelDataConst(Channel channel) const;
    const std::int16_t* getChannelDataConst(std::size_t chIdx) const;
    const std::size_t getChannelSizeConst(Channel channel) const;
};

/**
 * Quad channel color space unsigned.
*/
struct uQuadChannelCS {
    // maybe make it more general, eg. ch1, ch2 instead of Y, Cd,..
    enum class Channel
    {
        Y,
        Cd,
        Cm,
        Co
    };

    std::vector<std::uint16_t> Y;
    std::vector<std::uint16_t> Cd;
    std::vector<std::uint16_t> Cm;
    std::vector<std::uint16_t> Co;

    explicit uQuadChannelCS(std::size_t length);
    std::span<std::uint16_t> getChannel(Channel channel);
    std::span<std::uint16_t> getChannel(std::size_t chIdx);
    std::span<std::uint16_t const> getChannelView(Channel channel) const;
    const std::uint16_t* getChannelDataConst(Channel channel) const;
    const std::uint16_t* getChannelDataConst(std::size_t chIdx) const;
    const std::size_t getChannelSizeConst(Channel channel) const;
};