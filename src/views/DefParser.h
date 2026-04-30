/**
 * @file DefParser.h
 * @brief Binary parser for the legacy DEF sprite-animation format.
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
    sf::Texture texture;

    int32_t offset_x = 0;
    int32_t offset_y = 0;
    uint32_t width = 0;
    uint32_t height = 0;

    uint32_t canvas_width = 0;
    uint32_t canvas_height = 0;
};

/**
 * @brief Parsed DEF resource containing palette and frame groups.
 */
struct DefResource {
    std::uint32_t type = 0;
    std::uint32_t canvas_width = 0;
    std::uint32_t canvas_height = 0;

    std::uint32_t feet_x = 0;
    std::uint32_t feet_y = 0;
    std::array<sf::Color, 256> palette{ };
    std::unordered_map<int, std::vector<DefFrame>> groups;
};

#pragma pack( push, 1 )
/**
 * @brief Raw DEF file header as stored on disk.
 */
struct DefHeader {
    uint32_t type;
    uint32_t width;
    uint32_t height;
    uint32_t num_blocks;
    uint8_t palette[768];
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
    DefResource parse_file( const std::filesystem::path& filepath ) const;

private:
    template <typename T>
    static T read_binary( std::ifstream& file, const std::string& context ) {
        T value;
        if ( ! file.read( reinterpret_cast<char*>( &value ), sizeof( T ) ) ) {
            throw std::runtime_error( "Unexpected EOF while reading: " + context );
        }
        return value;
    }

    static std::vector<std::uint8_t> decode_frame_data( std::ifstream& file,
                                                        std::uint32_t compression_type,
                                                        std::uint32_t width,
                                                        std::uint32_t height,
                                                        std::uint32_t frame_start_offset );

    static std::vector<std::uint8_t> indexed_to_rgba( const std::vector<std::uint8_t>& indexed,
                                                      std::uint32_t crop_width,
                                                      std::uint32_t crop_height,
                                                      std::uint32_t full_width,
                                                      std::uint32_t full_height,
                                                      std::int32_t left_margin,
                                                      std::int32_t top_margin,
                                                      const std::array<sf::Color, 256>& palette );
};

} // namespace views