/**
 * @file DefParser.h
 * @brief Binary parser for the legacy DEF sprite-animation format.
 * @author Dominik Śledziewski
 */
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <SFML/Graphics.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace views {

/**
 * @brief One decoded DEF sprite frame and its placement metadata.
 */
struct DefFrame {
    sf::Texture texture_;

    int32_t offsetX_ = 0;
    int32_t offsetY_ = 0;
    uint32_t width_ = 0;
    uint32_t height_ = 0;

    uint32_t canvasWidth_ = 0;
    uint32_t canvasHeight_ = 0;
};

/**
 * @brief Parsed DEF resource containing palette and frame groups.
 */
struct DefResource {
    std::uint32_t type_ = 0;
    std::uint32_t canvasWidth_ = 0;
    std::uint32_t canvasHeight_ = 0;

    std::uint32_t feetX_ = 0;
    std::uint32_t feetY_ = 0;
    std::array<sf::Color, 256> palette_{ };
    std::unordered_map<int, std::vector<DefFrame>> groups_;
};

#pragma pack( push, 1 )
/**
 * @brief Raw DEF file header as stored on disk.
 */
struct DefHeader {
    uint32_t type_;
    uint32_t width_;
    uint32_t height_;
    uint32_t numBlocks_;
    uint8_t palette_[768];
};
#pragma pack( pop )

/**
 * @brief Parses a binary .def animation file into a DefResource.
 *
 * Decodes the compressed indexed-colour frame data, applies the
 * embedded palette and produces sf::Texture objects that the rest
 * of the engine can use without further decoding.
 */
class DefParser {
public:
    DefResource parseFile( const std::filesystem::path& filepath ) const;

private:
    template <typename T>
    static T readBinary( std::ifstream& file, const std::string& context ) {
        T value;
        if ( ! file.read( reinterpret_cast<char*>( &value ), sizeof( T ) ) ) {
            throw std::runtime_error( "Unexpected EOF while reading: " + context );
        }
        return value;
    }

    static std::vector<std::uint8_t> decodeFrameData( std::ifstream& file,
                                                        std::uint32_t compression_type,
                                                        std::uint32_t width,
                                                        std::uint32_t height,
                                                        std::uint32_t frame_start_offset );

    static std::vector<std::uint8_t> indexedToRgba( const std::vector<std::uint8_t>& indexed,
                                                      std::uint32_t crop_width,
                                                      std::uint32_t crop_height,
                                                      std::uint32_t full_width,
                                                      std::uint32_t full_height,
                                                      std::int32_t left_margin,
                                                      std::int32_t top_margin,
                                                      const std::array<sf::Color, 256>& palette );
};

} // namespace views