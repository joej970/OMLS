#include "Channels.hpp"
#include <iostream>

/**
 * 
 * SIGNED quad channel
*/
sQuadChannelCS::sQuadChannelCS(){};
sQuadChannelCS::sQuadChannelCS(std::size_t length)
   : Y(length)
   , Cd(length)
   , Cm(length)
   , Co(length){};

/**
 * Get span (*data, size) of Channel member
*/
std::span<std::int16_t> sQuadChannelCS::getChannel(Channel channel)
{
    switch(channel) {
        case Channel::Y:
            return {Y.data(), Y.size()};
        case Channel::Cd:
            return {Cd.data(), Cd.size()};
        case Channel::Cm:
            return {Cm.data(), Cm.size()};
        case Channel::Co:
            return {Co.data(), Co.size()};
        default:
            throw std::runtime_error("getChannel(): Invalid member requested.");
            return {Co.data(), Co.size()};   // dummy return
    }
};

/**
 * Returns span (*data, size) of Channel member
*/
std::span<std::int16_t> sQuadChannelCS::getChannel(std::size_t chIdx)
{
    switch(chIdx) {
        case 0:
            return {Y.data(), Y.size()};
        case 1:
            return {Cd.data(), Cd.size()};
        case 2:
            return {Cm.data(), Cm.size()};
        case 3:
            return {Co.data(), Co.size()};
        default:
            throw std::runtime_error("getChannel(): Invalid member requested.");
            return {Co.data(), Co.size()};   // dummy return
    }
};

/**
 * Get span (*data, size) of SIGNED Channel member as View only.
*/
std::span<std::int16_t const> sQuadChannelCS::getChannelView(Channel channel) const
{
    switch(channel) {
        case Channel::Y:
            return {Y.data(), Y.size()};
        case Channel::Cd:
            return {Cd.data(), Cd.size()};
        case Channel::Cm:
            return {Cm.data(), Cm.size()};
        case Channel::Co:
            return {Co.data(), Co.size()};
        default:
            throw std::runtime_error("getChannel(): Invalid member requested.");
            return {Co.data(), Co.size()};   // dummy return
    }
};

/**
 * Get *data of UNSIGNED Channel member as View only.
*/
const std::int16_t* sQuadChannelCS::getChannelDataConst(Channel channel) const
{
    switch(channel) {
        case Channel::Y:
            return Y.data();
        case Channel::Cd:
            return Cd.data();
        case Channel::Cm:
            return Cm.data();
        case Channel::Co:
            return Co.data();
        default:
            throw std::runtime_error("getChannel(): Invalid member requested.");
            return NULL;
    }
};
/**
 * Get *data of UNSIGNED Channel member as View only.
*/
const std::int16_t* sQuadChannelCS::getChannelDataConst(std::size_t chIdx) const
{
    switch(chIdx) {
        case 0:
            return Y.data();
        case 1:
            return Cd.data();
        case 2:
            return Cm.data();
        case 3:
            return Co.data();
        default:
            throw std::runtime_error("getChannel(): Invalid member requested.");
            return NULL;
    }
};

const std::size_t sQuadChannelCS::getChannelSizeConst(Channel channel) const
{
    switch(channel) {
        case Channel::Y:
            return Y.size();
        case Channel::Cd:
            return Cd.size();
        case Channel::Cm:
            return Cm.size();
        case Channel::Co:
            return Co.size();
        default:
            throw std::runtime_error("getChannelSize(): Invalid member requested.");
            return 0;
    }
}

/**
 * 
 * UNSIGNED quad channel
*/
uQuadChannelCS::uQuadChannelCS(std::size_t length)
   : Y(length)
   , Cd(length)
   , Cm(length)
   , Co(length){};

/**
 * Returns span (*data, size) of Channel member
*/
std::span<std::uint16_t> uQuadChannelCS::getChannel(Channel channel)
{
    switch(channel) {
        case Channel::Y:
            return {Y.data(), Y.size()};
        case Channel::Cd:
            return {Cd.data(), Cd.size()};
        case Channel::Cm:
            return {Cm.data(), Cm.size()};
        case Channel::Co:
            return {Co.data(), Co.size()};
        default:
            throw std::runtime_error("getChannel(): Invalid member requested.");
            return {Co.data(), Co.size()};   // dummy return
    }
};

/**
 * Returns span (*data, size) of Channel member
*/
std::span<std::uint16_t> uQuadChannelCS::getChannel(std::size_t chIdx)
{
    switch(chIdx) {
        case 0:
            return {Y.data(), Y.size()};
        case 1:
            return {Cd.data(), Cd.size()};
        case 2:
            return {Cm.data(), Cm.size()};
        case 3:
            return {Co.data(), Co.size()};
        default:
            throw std::runtime_error("getChannel(): Invalid member requested.");
            return {Co.data(), Co.size()};   // dummy return
    }
};

/**
 * Returns span (*data, size) of Channel member as View only.
*/
std::span<std::uint16_t const> uQuadChannelCS::getChannelView(Channel channel) const
{
    switch(channel) {
        case Channel::Y:
            return {Y.data(), Y.size()};
        case Channel::Cd:
            return {Cd.data(), Cd.size()};
        case Channel::Cm:
            return {Cm.data(), Cm.size()};
        case Channel::Co:
            return {Co.data(), Co.size()};
        default:
            throw std::runtime_error("getChannel(): Invalid member requested.");
            return {Co.data(), Co.size()};   // dummy return
    }
};

/**
 * Get pointer to Channel member data as View only.
*/
const std::uint16_t* uQuadChannelCS::getChannelDataConst(Channel channel) const
{
    switch(channel) {
        case Channel::Y:
            return Y.data();
        case Channel::Cd:
            return Cd.data();
        case Channel::Cm:
            return Cm.data();
        case Channel::Co:
            return Co.data();
        default:
            throw std::runtime_error("getChannel(): Invalid member requested.");
            return NULL;
    }
};
/**
 * Get pointer to Channel member data as View only.
*/
const std::uint16_t* uQuadChannelCS::getChannelDataConst(std::size_t chIdx) const
{
    switch(chIdx) {
        case 0:
            return Y.data();
        case 1:
            return Cd.data();
        case 2:
            return Cm.data();
        case 3:
            return Co.data();
        default:
            throw std::runtime_error("getChannel(): Invalid member requested.");
            return NULL;
    }
};

const std::size_t uQuadChannelCS::getChannelSizeConst(Channel channel) const
{
    switch(channel) {
        case Channel::Y:
            return Y.size();
        case Channel::Cd:
            return Cd.size();
        case Channel::Cm:
            return Cm.size();
        case Channel::Co:
            return Co.size();
        default:
            throw std::runtime_error("getChannelSize(): Invalid member requested.");
            return 0;
    }
}