#pragma once
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
// atrapa parsera: słowa rozdzielone odstępami, z pliku albo z tekstu
class cParser {
public:
    enum buffertype { buffer_FILE, buffer_TEXT };
    cParser( std::string const &Stream, buffertype const Type = buffer_TEXT, std::string Path = "", bool const = true ) : m_name( Stream ) {
        if( Type == buffer_FILE ) { std::ifstream file { Path + Stream }; std::stringstream buffer; buffer << file.rdbuf(); m_stream.str( buffer.str() ); }
        else { m_stream.str( Stream ); }
    }
    template <typename Output_> Output_ getToken( bool const ToLower = true ) {
        std::string token; m_stream >> token;
        if( ToLower ) { std::transform( token.begin(), token.end(), token.begin(), []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } ); }
        return token;
    }
    std::string Name() const { return m_name; }
    std::size_t Line() const { return 0; }
private:
    std::string m_name; std::stringstream m_stream;
};
