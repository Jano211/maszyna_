#pragma once
#include <map>
#include <string>
#include "world/Traction.h"
#include "model/AnimModel.h"
template <typename Type_> struct mock_table {
    std::map<std::string, Type_ *> items;
    Type_ *find( std::string const &Name ) const { auto const lookup { items.find( Name ) }; return lookup != items.end() ? lookup->second : nullptr; }
};
namespace simulation { extern mock_table<TTraction> Traction; extern mock_table<TAnimModel> Instances; }
// atrapa wstawiania modeli do scenerii: zapamiętuje wpisy i położenia
struct mock_state {
    struct created_model { std::string entry; std::string name; glm::dvec3 position; };
    std::vector<created_model> created;
    TAnimModel instance;
    TAnimModel *create_model( std::string const &Entry, std::string const &Name, glm::dvec3 const &Position ) { created.push_back( { Entry, Name, Position } ); return &instance; }
};
namespace simulation { extern mock_state State; }
