#pragma once

#include <SFML/Graphics.hpp>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include <array>
#include <stdexcept>

struct DefFrame {
    sf::Texture texture;

    // Content geometry (crop region inside the full canvas texture):
    //   The visible pixels occupy a `width × height` sub-rectangle whose
    //   top-left corner is at (offset_x, offset_y) within the texture.
    int32_t  offset_x      = 0;   // left margin in texture pixels
    int32_t  offset_y      = 0;   // top  margin in texture pixels
    uint32_t width         = 0;   // content width  (actual creature pixels)
    uint32_t height        = 0;   // content height (actual creature pixels)

    // Full texture / canvas dimensions — same for every frame in a DEF.
    uint32_t canvas_width  = 0;
    uint32_t canvas_height = 0;
};

struct DefResource {
    std::uint32_t type = 0;
    std::uint32_t canvas_width = 0;
    std::uint32_t canvas_height = 0;
    // Anchor coordinates (in canvas pixels) describing where the creature's
    // feet rest on the canvas.  Computed once at parse time from the Stand
    // group only — Stand frames represent the unit's natural at-rest pose, so
    // the resulting (feet_x, feet_y) is the consistent ground point that the
    // AnimationController glues to hex_center.  Anchoring by *canvas*
    // coordinates (not content-crop coordinates) is the Pikeman wobble fix.
    //
    // feet_x is critical: HoMM3 DEFs are NOT horizontally centered on the
    // canvas — the artwork sits on one side, so canvas_width/2 would shove
    // every unit off-centre on its hex.
    std::uint32_t feet_x = 0;
    std::uint32_t feet_y = 0;
    std::array<sf::Color, 256> palette{};
    std::unordered_map<int, std::vector<DefFrame>> groups;
};

// Zabezpieczenie przed paddingiem C++ (absolutnie konieczne do plików binarnych)
#pragma pack(push, 1)
struct DefHeader {
    uint32_t type;
    uint32_t width;
    uint32_t height;
    uint32_t num_blocks;
    uint8_t palette[768];
};
#pragma pack(pop)

class DefParser {
public:
    DefResource parse_file(const std::filesystem::path& filepath) const;
    static void debug_parse_file(const std::string& filepath);

private:
    template <typename T>
    static T read_binary(std::ifstream& file, const std::string& context) {
        T value;
        if (!file.read(reinterpret_cast<char*>(&value), sizeof(T))) {
            throw std::runtime_error("Unexpected EOF while reading: " + context);
        }
        return value;
    }

    static std::vector<std::uint8_t> decode_frame_data(std::ifstream& file,
                                                       std::uint32_t compression_type,
                                                       std::uint32_t width,
                                                       std::uint32_t height,
                                                       std::uint32_t frame_start_offset);

    static std::vector<std::uint8_t> indexed_to_rgba(const std::vector<std::uint8_t>& indexed,
                                                     std::uint32_t crop_width, std::uint32_t crop_height,
                                                     std::uint32_t full_width, std::uint32_t full_height,
                                                     std::int32_t left_margin, std::int32_t top_margin,
                                                     const std::array<sf::Color, 256>& palette);
};