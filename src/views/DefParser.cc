/**
 * @file DefParser.cc
 * @brief Implementation of the legacy DEF binary format parser.
 */
#include "DefParser.h"

#include <iostream>

namespace views {

namespace {

sf::Color resolve_palette_entry( int i, std::uint8_t r, std::uint8_t g, std::uint8_t b ) {
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

std::uint8_t read_byte( std::ifstream& file ) {
    std::uint8_t v;
    if ( ! file.read( reinterpret_cast<char*>( &v ), 1 ) )
        throw std::runtime_error( "Unexpected EOF in DEF RLE data" );
    return v;
}

void decode_rle_line( std::ifstream& file, std::uint8_t* line, std::uint32_t width ) {
    std::uint32_t x = 0;
    while ( x < width ) {
        const std::uint8_t seg = read_byte( file );
        const std::uint32_t length = read_byte( file ) + 1u;
        if ( seg == 0xFF ) {
            for ( std::uint32_t i = 0; i < length && x < width; ++i )
                line[x++] = read_byte( file );
        } else {
            for ( std::uint32_t i = 0; i < length && x < width; ++i )
                line[x++] = seg;
        }
    }
}

} // namespace

DefResource DefParser::parse_file( const std::filesystem::path& filepath ) const {
    std::ifstream file( filepath, std::ios::binary );
    if ( ! file.is_open( ) )
        throw std::runtime_error( "Cannot open file: " + filepath.string( ) );

    DefHeader header;
    if ( ! file.read( reinterpret_cast<char*>( &header ), sizeof( DefHeader ) ) )
        throw std::runtime_error( "Unexpected EOF while reading DefHeader" );

    DefResource resource;
    resource.type = header.type;
    resource.canvas_width = header.width;
    resource.canvas_height = header.height;

    for ( int i = 0; i < 256; ++i ) {
        resource.palette[i] = resolve_palette_entry(
            i, header.palette[i * 3 + 0], header.palette[i * 3 + 1], header.palette[i * 3 + 2] );
    }

    for ( std::uint32_t block = 0; block < header.num_blocks; ++block ) {
        const std::uint32_t block_id = read_binary<std::uint32_t>( file, "block_id" );
        const std::uint32_t num_frames = read_binary<std::uint32_t>( file, "num_frames" );
        read_binary<std::uint32_t>( file, "unk1" );
        read_binary<std::uint32_t>( file, "unk2" );

        for ( std::uint32_t f = 0; f < num_frames; ++f ) {
            char name[14] = { };
            if ( ! file.read( name, 13 ) )
                throw std::runtime_error( "Unexpected EOF in frame name" );
        }

        std::vector<std::uint32_t> frame_offsets( num_frames );
        for ( std::uint32_t f = 0; f < num_frames; ++f )
            frame_offsets[f] = read_binary<std::uint32_t>( file, "frame_offset" );

        std::vector<DefFrame> frames( num_frames );
        for ( std::uint32_t f = 0; f < num_frames; ++f ) {
            const std::streampos return_pos = file.tellg( );
            file.seekg( frame_offsets[f], std::ios::beg );

            read_binary<std::uint32_t>( file, "frame_size" );
            const std::uint32_t fmt = read_binary<std::uint32_t>( file, "fmt" );
            const std::uint32_t full_w = read_binary<std::uint32_t>( file, "full_w" );
            const std::uint32_t full_h = read_binary<std::uint32_t>( file, "full_h" );
            const std::uint32_t crop_w = read_binary<std::uint32_t>( file, "crop_w" );
            const std::uint32_t crop_h = read_binary<std::uint32_t>( file, "crop_h" );
            const std::int32_t lmargin = read_binary<std::int32_t>( file, "lmargin" );
            const std::int32_t tmargin = read_binary<std::int32_t>( file, "tmargin" );

            if ( fmt == 1 && crop_w > 0 && crop_h > 0 && full_w > 0 && full_h > 0 ) {
                std::vector<std::uint32_t> line_offsets( crop_h );
                for ( std::uint32_t y = 0; y < crop_h; ++y )
                    line_offsets[y] = read_binary<std::uint32_t>( file, "line_offset" );

                std::vector<std::uint8_t> indexed( crop_w * crop_h, 0 );
                for ( std::uint32_t y = 0; y < crop_h; ++y ) {
                    const std::streamoff pos = static_cast<std::streamoff>( frame_offsets[f] ) +
                                               32 + static_cast<std::streamoff>( line_offsets[y] );
                    file.seekg( pos, std::ios::beg );
                    decode_rle_line( file, &indexed[y * crop_w], crop_w );
                }

                std::vector<std::uint8_t> rgba( full_w * full_h * 4u, 0 );
                for ( std::uint32_t y = 0; y < crop_h; ++y ) {
                    const std::int32_t dy = tmargin + static_cast<std::int32_t>( y );
                    if ( dy < 0 || dy >= static_cast<std::int32_t>( full_h ) )
                        continue;
                    for ( std::uint32_t x = 0; x < crop_w; ++x ) {
                        const std::int32_t dx = lmargin + static_cast<std::int32_t>( x );
                        if ( dx < 0 || dx >= static_cast<std::int32_t>( full_w ) )
                            continue;
                        const sf::Color& c = resource.palette[indexed[y * crop_w + x]];
                        const std::size_t idx = ( static_cast<std::size_t>( dy ) * full_w +
                                                  static_cast<std::size_t>( dx ) ) *
                                                4u;
                        rgba[idx + 0] = c.r;
                        rgba[idx + 1] = c.g;
                        rgba[idx + 2] = c.b;
                        rgba[idx + 3] = c.a;
                    }
                }

                (void) frames[f].texture.resize( { full_w, full_h } );
                frames[f].texture.update( rgba.data( ) );

                frames[f].texture.setSmooth( false );

                frames[f].offset_x = lmargin;
                frames[f].offset_y = tmargin;
                frames[f].width = crop_w;
                frames[f].height = crop_h;
                frames[f].canvas_width = full_w;
                frames[f].canvas_height = full_h;
            }

            file.seekg( return_pos );
        }
        resource.groups[block_id] = std::move( frames );
    }

    auto compute_anchor =
        []( const std::vector<DefFrame>& frames, std::uint32_t& out_x, std::uint32_t& out_y ) {
            std::uint32_t bottom_max = 0;

            std::uint64_t cx_sum = 0;
            std::uint32_t cx_count = 0;
            for ( const DefFrame& f : frames ) {
                if ( f.width == 0 || f.height == 0 )
                    continue;
                const std::uint32_t bottom = static_cast<std::uint32_t>( f.offset_y ) + f.height;
                if ( bottom > bottom_max )
                    bottom_max = bottom;
                cx_sum += static_cast<std::uint64_t>( f.offset_x ) + f.width / 2u;
                ++cx_count;
            }
            if ( cx_count > 0 )
                out_x = static_cast<std::uint32_t>( cx_sum / cx_count );
            out_y = bottom_max;
        };

    if ( auto it = resource.groups.find( 1 ); it != resource.groups.end( ) ) {
        compute_anchor( it->second, resource.feet_x, resource.feet_y );
    }
    if ( resource.feet_y == 0 ) {
        for ( const auto& [_, frames] : resource.groups ) {
            std::uint32_t fx = 0, fy = 0;
            compute_anchor( frames, fx, fy );
            if ( fy > resource.feet_y ) {
                resource.feet_x = fx;
                resource.feet_y = fy;
            }
        }
    }
    if ( resource.feet_y == 0 || resource.feet_y > resource.canvas_height ) {
        resource.feet_y = resource.canvas_height;
    }
    if ( resource.feet_x == 0 || resource.feet_x > resource.canvas_width ) {
        resource.feet_x = resource.canvas_width / 2u;
    }

    return resource;
}

} // namespace views
