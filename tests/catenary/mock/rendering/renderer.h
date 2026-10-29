#pragma once
#include "rendering/geometrybank.h"
// atrapa renderera: banki i fragmenty geometrii trzymane w pamięci
struct mock_renderer {
    std::vector<std::vector<gfx::vertex_array>> banks;
    int inserts { 0 }, releases { 0 };
    gfx::geometrybank_handle Create_Bank() { banks.emplace_back(); return { static_cast<std::uint32_t>( banks.size() ), 0 }; }
    gfx::geometry_handle Insert( gfx::vertex_array &Vertices, gfx::userdata_array &, gfx::geometrybank_handle const &Bank, int ) {
        auto &bank { banks[ Bank.bank - 1 ] }; bank.push_back( Vertices ); ++inserts; return { Bank.bank, static_cast<std::uint32_t>( bank.size() ) }; }
    bool Replace( gfx::vertex_array &Vertices, gfx::userdata_array &, gfx::geometry_handle const &Geometry, int, std::size_t = 0 ) {
        banks[ Geometry.bank - 1 ][ Geometry.chunk - 1 ] = Vertices; ++releases; return true; }
    gfx::vertex_array const &Vertices( gfx::geometry_handle const &Geometry ) const { return banks[ Geometry.bank - 1 ][ Geometry.chunk - 1 ]; }
    std::size_t stored() const { std::size_t count { 0 }; for( auto const &bank : banks ) { for( auto const &chunk : bank ) { count += chunk.size(); } } return count; }
};
extern mock_renderer *GfxRenderer;
