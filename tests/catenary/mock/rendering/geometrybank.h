#pragma once
#include "stdafx.h"
namespace gfx {
struct basic_vertex { glm::vec3 position; glm::vec3 normal; };
struct vertex_userdata {};
using vertex_array = std::vector<basic_vertex>;
using userdata_array = std::vector<vertex_userdata>;
struct geometry_handle {
    std::uint32_t bank { 0 }, chunk { 0 };
    geometry_handle() = default;
    geometry_handle( std::uint32_t Bank, std::uint32_t Chunk ) : bank( Bank ), chunk( Chunk ) {}
    operator std::uint64_t() const { return ( std::uint64_t { bank } << 32 | chunk ); }
};
using geometrybank_handle = geometry_handle;
}
