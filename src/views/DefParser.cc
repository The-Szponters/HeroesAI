/**
 * @file DefParser.cc
 * @brief Implementation of the legacy DEF binary format parser.
 * @author Dominik Śledziewski
 */
#include <cstddef>
#include <iostream>

#include "DefParser.h"

namespace views {

namespace {

sf::Color resolvePaletteEntry( int i, std::uint8_t r, std::uint8_t g, std::uint8_t b ) {
    switch ( i ) {
    case 0:
        return sf::Color::Transparent;
    case 1:
        return sf::Color::Transparent;
    case 2:
        return sf::Color( 0, 0, 0, 64 );
    case 3:
        return sf::Color( 0, 0, 0, 128 );
    case 4:
        return sf::Color( 0, 0, 0, 128 );
    case 5:
        return sf::Color( 0, 0, 0, 128 );
    case 6:
        return sf::Color( 0, 0, 0, 160 );
    case 7:
        return sf::Color( 0, 0, 0, 192 );
    default:
        return sf::Color( r, g, b, 255 );
    }
}

std::uint8_t readByte( std::ifstream& file ) {
    std::uint8_t v;
    if ( ! file.read( reinterpret_cast<char*>( &v ), 1 ) ) {
        throw std::runtime_error( "Unexpected EOF in DEF RLE data" );
}
    return v;
}

void decodeRleLine( std::ifstream& file, std::uint8_t* line, std::uint32_t width ) {
    std::uint32_t x = 0;
    while ( x < width ) {
        const std::uint8_t seg = readByte( file );
        const std::uint32_t length = readByte( file ) + 1u;
        if ( seg == 0xFF ) {
            for ( std::uint32_t i = 0; i < length && x < width; ++i ) {
                line[x++] = readByte( file );
}
        } else {
            for ( std::uint32_t i = 0; i < length && x < width; ++i ) {
                line[x++] = seg;
}
        }
    }
}

} // namespace

DefResource DefParser::parseFile( const std::filesystem::path& filepath ) const {
    std::ifstream file( filepath, std::ios::binary );
    if ( ! file.is_open( ) ) {
        throw std::runtime_error( "Cannot open file: " + filepath.string( ) );
}

    DefHeader header{};
    if ( ! file.read( reinterpret_cast<char*>( &header ), sizeof( DefHeader ) ) ) {
        throw std::runtime_error( "Unexpected EOF while reading DefHeader" );
}

    DefResource resource;
    resource.type_ = header.type_;
    resource.canvasWidth_ = header.width_;
    resource.canvasHeight_ = header.height_;

    for ( int i = 0; i < 256; ++i ) {
        resource.palette_[i] = resolvePaletteEntry(
            i, header.palette_[i * 3 + 0], header.palette_[i * 3 + 1], header.palette_[i * 3 + 2] );
    }

    for ( std::uint32_t block = 0; block < header.numBlocks_; ++block ) {
        const std::uint32_t block_id = readBinary<std::uint32_t>( file, "block_id" );
        const std::uint32_t num_frames = readBinary<std::uint32_t>( file, "num_frames" );
        readBinary<std::uint32_t>( file, "unk1" );
        readBinary<std::uint32_t>( file, "unk2" );

        for ( std::uint32_t f = 0; f < num_frames; ++f ) {
            char name[14] = { };
            if ( ! file.read( name, 13 ) ) {
                throw std::runtime_error( "Unexpected EOF in frame name" );
}
        }

        std::vector<std::uint32_t> frame_offsets( num_frames );
        for ( std::uint32_t f = 0; f < num_frames; ++f ) {
            frame_offsets[f] = readBinary<std::uint32_t>( file, "frame_offset" );
}

        std::vector<DefFrame> frames( num_frames );
        for ( std::uint32_t f = 0; f < num_frames; ++f ) {
            const std::streampos return_pos = file.tellg( );
            file.seekg( frame_offsets[f], std::ios::beg );

            readBinary<std::uint32_t>( file, "frame_size" );
            const std::uint32_t fmt = readBinary<std::uint32_t>( file, "fmt" );
            const std::uint32_t full_w = readBinary<std::uint32_t>( file, "full_w" );
            const std::uint32_t full_h = readBinary<std::uint32_t>( file, "full_h" );
            const std::uint32_t crop_w = readBinary<std::uint32_t>( file, "crop_w" );
            const std::uint32_t crop_h = readBinary<std::uint32_t>( file, "crop_h" );
            const std::int32_t lmargin = readBinary<std::int32_t>( file, "lmargin" );
            const std::int32_t tmargin = readBinary<std::int32_t>( file, "tmargin" );

            if ( fmt == 1 && crop_w > 0 && crop_h > 0 && full_w > 0 && full_h > 0 ) {
                std::vector<std::uint32_t> line_offsets( crop_h );
                for ( std::uint32_t y = 0; y < crop_h; ++y ) {
                    line_offsets[y] = readBinary<std::uint32_t>( file, "line_offset" );
}

                std::vector<std::uint8_t> indexed( static_cast<std::size_t>( crop_w * crop_h ), 0 );
                for ( std::uint32_t y = 0; y < crop_h; ++y ) {
                    const std::streamoff pos = static_cast<std::streamoff>( frame_offsets[f] ) +
                                               32 + static_cast<std::streamoff>( line_offsets[y] );
                    file.seekg( pos, std::ios::beg );
                    decodeRleLine( file, &indexed[static_cast<std::size_t>( y * crop_w )], crop_w );
                }

                std::vector<std::uint8_t> rgba( static_cast<std::size_t>( full_w * full_h * 4u ), 0 );
                for ( std::uint32_t y = 0; y < crop_h; ++y ) {
                    const std::int32_t dy = tmargin + static_cast<std::int32_t>( y );
                    if ( dy < 0 || dy >= static_cast<std::int32_t>( full_h ) ) {
                        continue;
}
                    for ( std::uint32_t x = 0; x < crop_w; ++x ) {
                        const std::int32_t dx = lmargin + static_cast<std::int32_t>( x );
                        if ( dx < 0 || dx >= static_cast<std::int32_t>( full_w ) ) {
                            continue;
}
                        const sf::Color& c = resource.palette_[indexed[y * crop_w + x]];
                        const std::size_t idx = ( static_cast<std::size_t>( dy ) * full_w +
                                                  static_cast<std::size_t>( dx ) ) *
                                                4u;
                        rgba[idx + 0] = c.r;
                        rgba[idx + 1] = c.g;
                        rgba[idx + 2] = c.b;
                        rgba[idx + 3] = c.a;
                    }
                }

                if ( frames[f].texture_.resize( { full_w, full_h } ) ) {
                    frames[f].texture_.update( rgba.data( ) );
                    frames[f].texture_.setSmooth( false );
                    frames[f].offsetX_ = lmargin;
                    frames[f].offsetY_ = tmargin;
                    frames[f].width_ = crop_w;
                    frames[f].height_ = crop_h;
                    frames[f].canvasWidth_ = full_w;
                    frames[f].canvasHeight_ = full_h;
                }
            }

            file.seekg( return_pos );
        }
        resource.groups_[block_id] = std::move( frames );
    }

    auto compute_anchor =
        []( const std::vector<DefFrame>& frames, std::uint32_t& out_x, std::uint32_t& out_y ) {
            std::uint32_t bottom_max = 0;

            std::uint64_t cx_sum = 0;
            std::uint32_t cx_count = 0;
            for ( const DefFrame& f : frames ) {
                if ( f.width_ == 0 || f.height_ == 0 ) {
                    continue;
}
                const std::uint32_t bottom = static_cast<std::uint32_t>( f.offsetY_ ) + f.height_;
                if ( bottom > bottom_max ) {
                    bottom_max = bottom;
}
                cx_sum += static_cast<std::uint64_t>( f.offsetX_ ) + f.width_ / 2u;
                ++cx_count;
            }
            if ( cx_count > 0 ) {
                out_x = static_cast<std::uint32_t>( cx_sum / cx_count );
}
            out_y = bottom_max;
        };

    if ( auto it = resource.groups_.find( 1 ); it != resource.groups_.end( ) ) {
        compute_anchor( it->second, resource.feetX_, resource.feetY_ );
    }
    if ( resource.feetY_ == 0 ) {
        for ( const auto& [_, frames] : resource.groups_ ) {
            std::uint32_t fx = 0, fy = 0;
            compute_anchor( frames, fx, fy );
            if ( fy > resource.feetY_ ) {
                resource.feetX_ = fx;
                resource.feetY_ = fy;
            }
        }
    }
    if ( resource.feetY_ == 0 || resource.feetY_ > resource.canvasHeight_ ) {
        resource.feetY_ = resource.canvasHeight_;
    }
    if ( resource.feetX_ == 0 || resource.feetX_ > resource.canvasWidth_ ) {
        resource.feetX_ = resource.canvasWidth_ / 2u;
    }

    return resource;
}

} // namespace views
