// atrapa nagłówków symulatora do testu world/catenary_span.cpp poza symulatorem
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <locale>
#include <sstream>
#include <memory>
#include <string>
#include <vector>
namespace glm {
struct vec3 { float x { 0 }, y { 0 }, z { 0 }; vec3() = default; template <class A, class B, class C> vec3( A a, B b, C c ) : x( float( a ) ), y( float( b ) ), z( float( c ) ) {} };
struct dvec3 { double x { 0 }, y { 0 }, z { 0 }; };
inline dvec3 operator-( dvec3 const &a, dvec3 const &b ) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline dvec3 operator+( dvec3 const &a, dvec3 const &b ) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline dvec3 operator*( dvec3 const &a, double const b ) { return { a.x * b, a.y * b, a.z * b }; }
inline double dot( dvec3 const &a, dvec3 const &b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline double length( dvec3 const &a ) { return std::sqrt( dot( a, a ) ); }
}
constexpr int null_handle { 0 };
constexpr int GL_TRIANGLES { 4 };
constexpr int GL_LINES { 1 };
