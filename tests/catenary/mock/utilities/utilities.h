#pragma once
#include "stdafx.h"
inline std::string to_string( glm::dvec3 const &p ) { return std::to_string( p.x ) + " " + std::to_string( p.y ) + " " + std::to_string( p.z ); }
