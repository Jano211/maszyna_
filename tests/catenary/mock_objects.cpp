#include <sstream>
// atrapa world/catenary_scene_objects.cpp: liczy usunięte części i przęsła, zapamiętuje treść przęseł łańcuchów
#include "stdafx.h"
#include "model/AnimModel.h"
#include "world/Traction.h"
#include <string>
#include <vector>
int mock_deleted_parts { 0 };
int mock_deleted_spans { 0 };
std::vector<std::string> mock_span_bodies;   // treść każdego zrobionego przęsła, po kolei
std::vector<std::string> mock_span_names;
void catenary_part_detach( TAnimModel * ) {}
void catenary_part_delete( TAnimModel * ) { ++mock_deleted_parts; }
TTraction *catenary_span_create( std::string const &Name, double, double, std::string const &Body, bool ) {
    mock_span_bodies.push_back( Body );
    mock_span_names.push_back( Name );
    auto *span { new TTraction() };
    span->m_name = Name;
    // jak TTraction::Load: druga linia treści to punkty przewodu (koniec 1, koniec 2), "parallel <nazwa>" - bieżnia wspólna
    std::istringstream lines { Body };
    std::string line;
    std::getline( lines, line );
    auto const point = [ & ]( glm::dvec3 &P ) { if( std::getline( lines, line ) ) { std::istringstream words { line }; words >> P.x >> P.y >> P.z; } };
    point( span->pPoint1 );
    point( span->pPoint2 );
    if( auto const at { Body.find( " parallel " ) }; at != std::string::npos ) { std::istringstream words { Body.substr( at + 10 ) }; words >> span->asParallel; }
    return span;
}
void catenary_span_delete( TTraction *Span ) { ++mock_deleted_spans; delete Span; }
