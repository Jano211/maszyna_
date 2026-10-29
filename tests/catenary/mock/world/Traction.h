#pragma once
#include "stdafx.h"
#include "rendering/geometrybank.h"
struct catenary_span;
class TTraction {
public:
    TTraction *hvNext[ 2 ] { nullptr, nullptr };
    int iNext[ 2 ] { 0, 0 };
    glm::dvec3 pPoint1, pPoint2, pPoint3, pPoint4, m_origin;
    float WireOffset { 0.04f };
    int iNumSections { 10 };
    unsigned int Material { 1 };
    unsigned int DamageFlag { 0 };
    gfx::geometry_handle m_geometry;
    std::shared_ptr<catenary_span> m_catenaryspan;
    std::string m_name { "none" };
    std::string asParallel;
    TTraction *hvParallel { nullptr };
    std::string const &name() const { return m_name; }
};
