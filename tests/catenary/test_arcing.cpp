// iskrzenie pantografu (world/catenary_arcing.cpp): oblodzenie z pogody, łuki przy oblodzeniu, przejściu sekcji i oderwaniu,
// iskry spadające i gasnące
#include "world/catenary_arcing.h"
#include "world/Traction.h"
#include "utilities/Globals.h"
#include <cstdio>
#include <cmath>

mock_globals Global;

int failures { 0 };
void check( bool const Good, char const *What ) { std::printf( "%s  %s\n", Good ? "OK  " : "BLAD", What ); if( false == Good ) { ++failures; } }

int main() {
    // pogoda: ciepło - nic, -3 °C we mgle - najwięcej, -3 °C sucho - mniej, -25 °C - mało
    Global.AirTemperature = 10.f;
    check( catenary_arcing::icing() == 0.f, "+10 C: bez oblodzenia" );
    Global.AirTemperature = -3.f; Global.fFogEnd = 300.f;
    auto const foggy { catenary_arcing::icing() };
    Global.fFogEnd = 7500.f;
    auto const dry { catenary_arcing::icing() };
    Global.AirTemperature = -25.f;
    auto const cold { catenary_arcing::icing() };
    std::printf( "  oblodzenie: -3 C mgla %.2f, -3 C sucho %.2f, -25 C sucho %.2f\n", foggy, dry, cold );
    check( foggy > 0.9f && dry > 0.2f && dry < 0.4f && cold < dry, "-3 C we mgle najwiecej, sucho mniej, silny mroz jeszcze mniej" );

    // jazda 30 m/s pod oblodzonym przewodem, 600 A, 10 s: łuki i iskry; na postoju nic
    TTraction a, b;
    a.asPowerSupplyName = "pwr01"; b.asPowerSupplyName = "pwr01";
    Global.AirTemperature = -3.f; Global.fFogEnd = 300.f;
    int key { 0 };
    int started { 0 };
    std::size_t sparksmax { 0 };
    glm::dvec3 point { 0.0, 5.6, 0.0 };
    double const dt { 0.02 };
    for( int step = 0; step < 500; ++step ) {
        point.z += 30.0 * dt;
        if( catenary_arcing::contact( &key, point, glm::dvec3 { 0.0, 0.0, 30.0 }, 600.0, &a, dt ) > 0.f ) { ++started; }
        catenary_arcing::update( dt );
        sparksmax = std::max( sparksmax, catenary_arcing::sparks().size() );
    }
    std::printf( "  10 s jazdy 30 m/s, 600 A, oblodzenie %.2f: lukow %d, iskier najwiecej %zu\n", foggy, started, sparksmax );
    check( started > 20 && sparksmax > 10, "oblodzony przewod: luki i iskry" );
    // iskry spadają (niżej niż punkt styku) i lecą z pociągiem, ale zostają w tyle
    bool fall { false };
    for( auto const &spark : catenary_arcing::sparks() ) { if( spark.position.y < 5.5 ) { fall = true; } }
    check( fall, "iskry spadaja" );
    // po 3 s bez styku wszystko gaśnie
    for( int step = 0; step < 150; ++step ) { catenary_arcing::update( dt ); }
    check( catenary_arcing::sparks().empty() && catenary_arcing::arcs().empty(), "iskry gasna, luki znikaja" );

    // ciepło: przejście na przęsło innego zasilacza pod prądem - łuk; ten sam zasilacz - nic
    Global.AirTemperature = 10.f;
    catenary_arcing::clear();
    b.asPowerSupplyName = "pwr02";
    catenary_arcing::contact( &key, point, glm::dvec3 { 0.0, 0.0, 20.0 }, 400.0, &a, dt );
    auto const section { catenary_arcing::contact( &key, point, glm::dvec3 { 0.0, 0.0, 20.0 }, 400.0, &b, dt ) };
    TTraction c; c.asPowerSupplyName = "pwr02";
    auto const same { catenary_arcing::contact( &key, point, glm::dvec3 { 0.0, 0.0, 20.0 }, 400.0, &c, dt ) };
    check( section > 0.f && same == 0.f, "przejscie sekcji pod pradem: luk, ten sam zasilacz: nic" );
    // oderwanie pod obciążeniem - łuk; bez prądu - nic
    auto const dropped { catenary_arcing::release( &key, point, glm::dvec3 { 0.0, 0.0, 20.0 }, 800.0 ) };
    int other { 1 };
    auto const idle { catenary_arcing::release( &other, point, glm::dvec3 { 0.0 }, 2.0 ) };
    catenary_arcing::update( dt );
    check( dropped > 0.5f && idle == 0.f && false == catenary_arcing::arcs().empty(), "opuszczenie pod pradem: luk, bez pradu: nic" );
    // pierwsze dotknięcie: krótki błysk
    catenary_arcing::clear();
    int raised { 2 };
    auto const touched { catenary_arcing::touch( &raised, point, 0.3 ) };
    catenary_arcing::update( dt );
    auto const flash { catenary_arcing::arcs().empty() ? 0.f : catenary_arcing::arcs().front().intensity };
    for( int step = 0; step < 10; ++step ) { catenary_arcing::update( dt ); }
    check( touched > 0.f && flash > 0.f && catenary_arcing::arcs().empty(), "podniesienie pantografu: krotki blysk (do 0,2 s)" );
    // wyłączone w ustawieniach
    Global.CatenaryArcing = false;
    catenary_arcing::clear();
    check( catenary_arcing::release( &key, point, glm::dvec3 { 0.0 }, 800.0 ) == 0.f && catenary_arcing::icing() == 0.f, "catenary.arcing no: nic" );

    std::printf( failures == 0 ? "wszystko OK\n" : "BLEDY: %d\n", failures );
    return failures == 0 ? 0 : 1;
}
