#pragma once
// atrapa przęsła do testu iskrzenia: zasilacz i przebieg przewodu
#include <string>
#include <glm/glm.hpp>
class TTraction {
public:
    std::string asPowerSupplyName;
    glm::dvec3 pPoint1 { 0.0 };
    glm::dvec3 vParametric { 0.0 };
};
