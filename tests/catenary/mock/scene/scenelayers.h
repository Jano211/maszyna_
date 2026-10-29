#pragma once
// atrapa scene/scenelayers.h dla testow sieci: catenary_scene.cpp pyta tylko o biezaca warstwe wczytywanego pliku
#include <cstddef>
namespace scene
{
using layer_handle = std::size_t;
struct node_layers
{
	layer_handle handle() const { return 0; }
};
inline node_layers Layers;
} // namespace scene
