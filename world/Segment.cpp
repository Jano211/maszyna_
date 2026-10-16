/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "world/Segment.h"

#include "utilities/Globals.h"
#include "utilities/Logs.h"
#include "utilities/parser.h"
#include "utilities/utilities.h"
#include "world/Track.h"
#include "rendering/renderer.h"

void
segment_data::deserialize( cParser &Input, glm::dvec3 const &Offset ) {

    points[ segment_data::point::start ] = LoadPoint( Input ) + Offset;
    Input.getTokens();
    Input >> rolls[ 0 ];
    points[ segment_data::point::control1 ] = LoadPoint( Input );
    points[ segment_data::point::control2 ] = LoadPoint( Input );
    points[ segment_data::point::end ] = LoadPoint( Input ) + Offset;
    Input.getTokens( 2 );
    Input
        >> rolls[ 1 ]
        >> radius;
}


TSegment::TSegment(TTrack *owner) :
                   pOwner( owner )
{}

bool TSegment::Init(glm::dvec3 NewPoint1, glm::dvec3 NewPoint2, double fNewStep, double fNewRoll1, double fNewRoll2)
{ // wersja dla prostego - wyliczanie punktów kontrolnych
	glm::dvec3 dir;

    // NOTE: we're enforcing division also for straight track, to ensure dense enough mesh for per-vertex lighting
/*
    if (fNewRoll1 == fNewRoll2)
    { // faktyczny prosty
        dir = Normalize(NewPoint2 - NewPoint1); // wektor kierunku o długości 1
        return TSegment::Init(NewPoint1, dir, -dir, NewPoint2, fNewStep, fNewRoll1, fNewRoll2,
                              false);
    }
    else
*/
    { // prosty ze zmienną przechyłką musi być segmentowany jak krzywe
        dir = (NewPoint2 - NewPoint1) / 3.0; // punkty kontrolne prostego są w 1/3 długości
        return TSegment::Init(
            NewPoint1, NewPoint1 + dir,
            NewPoint2 - dir, NewPoint2,
            fNewStep, fNewRoll1, fNewRoll2, true);
    }
};

bool TSegment::Init(glm::dvec3 &NewPoint1, glm::dvec3 NewCPointOut, glm::dvec3 NewCPointIn, glm::dvec3 &NewPoint2, double fNewStep, double fNewRoll1, double fNewRoll2, bool bIsCurve)
{ // wersja uniwersalna (dla krzywej i prostego)
    Point1 = NewPoint1;
    CPointOut = NewCPointOut;
    CPointIn = NewCPointIn;
    Point2 = NewPoint2;
    // poprawienie przechyłki
    fRoll1 = static_cast<float>(glm::radians(fNewRoll1)); // Ra: przeliczone jest bardziej przydatne do obliczeń
    fRoll2 = static_cast<float>(glm::radians(fNewRoll2));
    bCurve = bIsCurve;
    if (Global.bRollFix)
    { // Ra: poprawianie przechyłki
        // Przechyłka powinna być na środku wewnętrznej szyny, a standardowo jest w osi
        // toru. Dlatego trzeba podnieść tor oraz odpowiednio podwyższyć podsypkę.
        // Nie wykonywać tej funkcji, jeśli podwyższenie zostało uwzględnione w edytorze.
        // Problematyczne mogą byc rozjazdy na przechyłce - lepiej je modelować w edytorze.
        // Na razie wszystkie scenerie powinny być poprawiane.
        // Jedynie problem będzie z podwójną rampą przechyłkową, która w środku będzie
        // mieć moment wypoziomowania, ale musi on być również podniesiony.
        if (fRoll1 != 0.0)
        { // tylko jeśli jest przechyłka
            double w1 = std::abs(std::sin(fRoll1) * 0.75); // 0.5*w2+0.0325; //0.75m dla 1.435
            Point1.y += w1; // modyfikacja musi być przed policzeniem dalszych parametrów
            if (bCurve)
                CPointOut.y += w1; // prosty ma wektory jednostkowe
            pOwner->MovedUp1(static_cast<float>(w1)); // zwrócić trzeba informację o podwyższeniu podsypki
        }
        if (fRoll2 != 0.f)
        {
            double w2 = std::abs(std::sin(fRoll2) * 0.75); // 0.5*w2+0.0325; //0.75m dla 1.435
            Point2.y += w2; // modyfikacja musi być przed policzeniem dalszych parametrów
            if (bCurve)
                CPointIn.y += w2; // prosty ma wektory jednostkowe
            // zwrócić trzeba informację o podwyższeniu podsypki
        }
    }
    // kąt w planie, żeby nie liczyć wielokrotnie
    // Ra: ten kąt jeszcze do przemyślenia jest
    fDirection = -std::atan2(Point2.x - Point1.x, Point2.z - Point1.z);
    if (bCurve)
    { // przeliczenie współczynników wielomianu, będzie mniej mnożeń i można policzyć pochodne
        vC = 3.0 * (CPointOut - Point1); // t^1
        vB = 3.0 * (CPointIn - CPointOut) - vC; // t^2
        vA = Point2 - Point1 - vC - vB; // t^3
        fLength = ComputeLength();
    }
    else {
		fLength = glm::length(Point1 - Point2);
    }
    if (fLength <= 0) {

        ErrorLog( "Bad track: zero length spline \"" + pOwner->name() + "\" (location: " + to_string( glm::dvec3{ Point1 } ) + ")" );
        fLength = 0.01; // crude workaround TODO: fix this properly
    }

    fStoop = std::atan2(Point2.y - Point1.y, fLength); // pochylenie toru prostego, żeby nie liczyć wielokrotnie

    fStep = fNewStep;
    // NOTE: optionally replace this part with the commented version, after solving geometry issues with double switches
    if( pOwner->eType == tt_Switch
     && fStep * (3.0 * Global.SplineFidelity) > fLength ) {
        // NOTE: a workaround for too short switches (less than 3 segments) messing up animation/generation of blades
        fStep = fLength / ( 3.0 * Global.SplineFidelity );
    }
//    iSegCount = static_cast<int>( std::ceil( fLength / fStep ) ); // potrzebne do VBO
    iSegCount = pOwner->eType == tt_Switch ? static_cast<int>(6 * Global.SplineFidelity) : static_cast<int>(std::ceil(fLength / fStep)); // potrzebne do VBO

    fStep = fLength / iSegCount; // update step to equalize size of individual pieces

	InitTFromSInterpolateData();
    return true;
}

void TSegment::InitTFromSInterpolateData()
{
    // pre-compute the curve parameter t at equal distance intervals
    fTsBuffer.resize( iSegCount + 1 );
    fTsBuffer[ 0 ] = 0.0;
    for( int i = 1; i < iSegCount; ++i ) {
        fTsBuffer[ i ] = GetTFromS( i * fStep );
    }
    fTsBuffer[ iSegCount ] = 1.0;

    // pre-compute Hermite slopes dt/du = fStep / |B'(t)| for FastGetTFromS
    // (dt/ds = 1/|B'(t)|, scaled by fStep to the interval coordinate u)
    fTsSlope.resize( iSegCount + 1 );
    for( int i = 0; i <= iSegCount; ++i ) {
        // compute ds/dt derivative = |B'(t)|
        double const speed = glm::length( GetFirstDerivative( fTsBuffer[ i ] ) );
        // compute dt/du derivative = fStep / (ds/dt)
        fTsSlope[ i ] = speed > 0.0 ? fStep / speed : 0.0;
    }
    // clamp slopes to keep t(s) monotone (PCHIP / Fritsch-Carlson condition)
    for( int i = 0; i <= iSegCount; ++i ) {
        double limit = std::numeric_limits<double>::max();
        if( i > 0 )         { limit = std::min( limit, 3.0 * ( fTsBuffer[ i ] - fTsBuffer[ i - 1 ] ) ); }
        if( i < iSegCount ) { limit = std::min( limit, 3.0 * ( fTsBuffer[ i + 1 ] - fTsBuffer[ i ] ) ); }
        fTsSlope[ i ] = std::min( fTsSlope[ i ], limit );
    }
}

glm::dvec3 TSegment::GetFirstDerivative(double const fTime) const
{
	// derivative is 3*A*t^2 + 2*B*t + C
	return fTime * ( fTime * 3.0 * vA + vB + vB ) + vC;
}

double TSegment::RombergIntegral(double const fA, double const fB) const
{
    double fH = fB - fA;

    const int ms_iOrder = 5;

    double ms_apfRom[2][ms_iOrder];

    ms_apfRom[0][0] =
        0.5 * fH * (glm::length(GetFirstDerivative(fA)) + glm::length(GetFirstDerivative(fB)));
    for (int i0 = 2, iP0 = 1; i0 <= ms_iOrder; i0++, iP0 *= 2, fH *= 0.5)
    {
        // approximations via the trapezoid rule
        double fSum = 0.0;
        int i1;
        for (i1 = 1; i1 <= iP0; i1++)
            fSum += glm::length(GetFirstDerivative(fA + fH * (i1 - 0.5)));

        // Richardson extrapolation
        ms_apfRom[1][0] = 0.5 * (ms_apfRom[0][0] + fH * fSum);
        for (int i2 = 1, iP2 = 4; i2 < i0; i2++, iP2 *= 4)
        {
            ms_apfRom[1][i2] = (iP2 * ms_apfRom[1][i2 - 1] - ms_apfRom[0][i2 - 1]) / (iP2 - 1);
        }

        for (i1 = 0; i1 < i0; i1++)
            ms_apfRom[0][i1] = ms_apfRom[1][i1];
    }

    return ms_apfRom[0][ms_iOrder - 1];
}

double TSegment::GetTFromS(double const s) const
{
    // initial guess for Newton's method
    double fTolerance = 0.001;
    double fRatio = s / fLength; // fLength is approx RombergIntegral(0, 1), good enough to start Newton
    double fTime = std::lerp( 0.0, 1.0, fRatio );

    int iteration = 0;
    double fDifference {}; // exposed for debug down the road
	do {
        fDifference = RombergIntegral(0, fTime) - s;
		if( std::abs( fDifference ) < fTolerance ) {
			return fTime;
		}
		fTime -= fDifference / glm::length(GetFirstDerivative(fTime));
		++iteration;
	}
	while( iteration < 10 ); // arbitrary limit

    // Newton's method failed.  If this happens, increase iterations or
    // tolerance or integration accuracy.
    // return -1; //Ra: tu nigdy nie dojdzie
    ErrorLog( "Bad track: shape estimation failed for spline \"" + pOwner->name() + "\" (location: " + to_string( glm::dvec3{ Point1 } ) + ")" );
	// MessageBox(0,"Too many iterations","GetTFromS",MB_OK);
	return fTime;
};

double TSegment::FastGetTFromS(double const s) const
{ // Faster version of GetTFromS: cubic Hermite interpolation using
	// the precomputed values in fTsBuffer and slopes in fTsSlope. It
	// avoids the expensive RombergIntegral+Newton per call. Hermite
	// interpolation is C1-continuous, so no sudden speed changes when
	// moving from one interval to the next.
	// NOTE: for s outside <0, fLength> the result is extrapolated
	// outside <0, 1>, which is the expected behavior (see e.g. usage
	// in GetDirection())
	int const i = std::clamp(static_cast<int>(s / fStep), 0, iSegCount - 1); // interval index
	double const u = (s - i * fStep) / fStep; // position within interval (possibly outside)
	double const u2 = u * u;
	double const u3 = u2 * u;
	double const h01 = -2.0 * u3 + 3.0 * u2;
	double const t = ( ( 1.0 - h01 )         * fTsBuffer[ i ] +
					   ( u3 - 2.0 * u2 + u ) * fTsSlope [ i ] +
					   h01               * fTsBuffer[ i + 1 ] +
					   ( u3 - u2 )           * fTsSlope [ i + 1 ] );
	return t;
}

glm::dvec3 TSegment::RaInterpolate(double const t) const
{ // wyliczenie XYZ na krzywej Beziera z użyciem współczynników
    return t * (t * (t * vA + vB) + vC) + Point1; // 9 mnożeń, 9 dodawań
};

glm::dvec3 TSegment::RaInterpolate0(double const t) const
{ // wyliczenie XYZ na krzywej Beziera, na użytek liczenia długości nie jest dodawane Point1
    return t * (t * (t * vA + vB) + vC); // 9 mnożeń, 6 dodawań
};

double TSegment::ComputeLength() const // McZapkie-150503: dlugosc miedzy punktami krzywej
{ // obliczenie długości krzywej Beziera za pomocą interpolacji odcinkami
    // Ra: zamienić na liczenie rekurencyjne średniej z cięciwy i łamanej po kontrolnych
    // Ra: koniec rekurencji jeśli po podziale suma długości nie różni się więcej niż 0.5mm od
    // poprzedniej
    // Ra: ewentualnie rozpoznać łuk okręgu płaskiego i liczyć ze wzoru na długość łuku
    double t;
    double l = 0;
	glm::dvec3 last{0, 0, 0}; // długość liczona po przesunięciu odcinka do początku układu
	glm::dvec3 tmp = Point2 - Point1;
    auto m = static_cast<int>(20.0 * glm::length(tmp)); // było zawsze do 10000, teraz jest liczone odcinkami po około 5cm
    for (int i = 1; i <= m; i++)
    {
        t = double(i) / double(m); // wyznaczenie parametru na krzywej z przedziału (0,1>
        // tmp=std::lerp(t,p1,cp1,cp2,p2);
        tmp = RaInterpolate0(t); // obliczenie punktu dla tego parametru
		t = glm::length(tmp - last); // obliczenie długości wektora
        l += t; // zwiększenie wyliczanej długości
        last = tmp;
    }
    return l;
}

// finds point on segment closest to specified point in 3d space. returns: point on segment as value in range 0-1
double
TSegment::find_nearest_point( glm::dvec3 const &Point ) const {

    if( false == bCurve || iSegCount == 1 ) {
        // for straight track just treat it as a single segment
        return
            nearest_segment_point(
                glm::dvec3{ FastGetPoint_0() },
                glm::dvec3{ FastGetPoint_1() },
                Point );
    }
    else {
        // for curves iterate through segment chunks, and find the one which gives us the least distance to the specified point
        double distance = std::numeric_limits<double>::max();
        double nearest { 0.0 };
        // NOTE: we're reusing already created segment chunks, which are created based on splinefidelity setting
        // this means depending on splinefidelity the results can be potentially slightly different
        for( int segmentidx = 0; segmentidx < iSegCount; ++segmentidx ) {

            auto const segmentpoint =
                std::clamp(
                    nearest_segment_point(
                        glm::dvec3{ FastGetPoint( fTsBuffer[ segmentidx ] ) },
                        glm::dvec3{ FastGetPoint( fTsBuffer[ segmentidx + 1 ] ) },
                        Point ) // point in range 0-1 on current segment
                    * ( fTsBuffer[ segmentidx + 1 ] - fTsBuffer[ segmentidx ] ) // segment length
                    + fTsBuffer[ segmentidx ], // segment offset
                    0.0, 1.0 ); // we clamp the range in case there's some floating point math inaccuracies

            auto const segmentdistance = glm::length2( Point - glm::dvec3{ FastGetPoint( segmentpoint ) } );
            if( segmentdistance < distance ) {

                nearest = segmentpoint;
                distance = segmentdistance;
            }
        }
        return nearest; // NOSONAR initialised above, the PR analysis keeps a stale report of the old declaration
    }
}

const double fDirectionOffset = 0.1; // długość wektora do wyliczenia kierunku

glm::dvec3 TSegment::GetDirection(double const fDistance) const
{ // takie toporne liczenie pochodnej dla podanego dystansu od Point1
    double t1 = FastGetTFromS(fDistance - fDirectionOffset);
    if (t1 <= 0.0)
        return CPointOut - Point1; // na zewnątrz jako prosta
    double t2 = FastGetTFromS(fDistance + fDirectionOffset);
    if (t2 >= 1.0)
        return Point1 - CPointIn; // na zewnątrz jako prosta
    return FastGetPoint(t2) - FastGetPoint(t1);
}

glm::dvec3 TSegment::FastGetDirection(double fDistance, double fOffset) const
{ // takie toporne liczenie pochodnej dla parametru 0.0÷1.0
    double t1 = fDistance - fOffset;
    if (t1 <= 0.0)
        return CPointOut - Point1; // wektor na początku jest stały
    double t2 = fDistance + fOffset;
    if (t2 >= 1.0)
        return Point2 - CPointIn; // wektor na końcu jest stały
    return FastGetPoint(t2) - FastGetPoint(t1);
}
/*
Math3D::vector3 TSegment::GetPoint(double const fDistance) const
{ // wyliczenie współrzędnych XYZ na torze w odległości (fDistance) od Point1
    if (bCurve)
    { // można by wprowadzić uproszczony wzór dla okręgów płaskich
        double t = FastGetTFromS(fDistance); // aproksymacja dystansu na krzywej Beziera
        // return std::lerp(t,Point1,CPointOut,CPointIn,Point2);
        return RaInterpolate(t);
    }
    else {
        // wyliczenie dla odcinka prostego jest prostsze
        return
            std::lerp(
                Point1, Point2,
                std::clamp(
                    fDistance / fLength,
                    0.0, 1.0 ) );
    }
};
*/

void TSegment::RaPositionGet(double const fDistance, glm::dvec3 &position, glm::vec3 &rotation) const {
    if (bCurve) {
        // można by wprowadzić uproszczony wzór dla okręgów płaskich
        auto const t = FastGetTFromS(fDistance); // aproksymacja dystansu na krzywej Beziera na parametr (t)
        position = FastGetPoint( t );
        // przechyłka w danym miejscu (zmienia się liniowo)
        rotation.x = static_cast<float>(std::lerp( fRoll1, fRoll2, t ));
        // pochodna
        auto const tangent = GetFirstDerivative( t );
        // pochylenie krzywej (w pionie)
        rotation.y = static_cast<float>(std::atan( tangent.y ));
        // kierunek krzywej w planie
        rotation.z = static_cast<float>(-std::atan2( tangent.x, tangent.z ));
    }
    else {
        // wyliczenie dla odcinka prostego jest prostsze
        auto const t = fDistance / fLength; // zerowych torów nie ma
        position = FastGetPoint( t );
        // przechyłka w danym miejscu (zmienia się liniowo)
        rotation.x = static_cast<float>(std::lerp( fRoll1, fRoll2, t ));
        rotation.y = static_cast<float>(fStoop); // pochylenie toru prostego
        rotation.z = static_cast<float>(fDirection); // kierunek toru w planie
    }
};

glm::dvec3 TSegment::FastGetPoint(double const t) const
{
    // return (bCurve?std::lerp(t,Point1,CPointOut,CPointIn,Point2):((1.0-t)*Point1+(t)*Point2));
    return true == bCurve || iSegCount != 1 ? RaInterpolate(t) : glm::mix(Point1, Point2, t);
}

// calculates placement frame matching geometry generated by RenderLoft() at specified distance along the segment.
// the loft is a chain of straight pieces, so anything meant to sit on it has to follow the chain and not the ideal curve
bool TSegment::LoftFrameGet( double const Distance, glm::dvec3 &Position, glm::vec3 &Parallel, glm::vec3 &Direction, float &Progress ) {

    if( fTsBuffer.empty() ) { return false; }
    if( fLength <= 0.0 ) { return false; }
    if( iSegCount < 1 ) { return false; }
    if( fStep <= 0.0 ) { return false; }

    auto const distance { std::clamp( Distance, 0.0, fLength ) };
    auto const index { std::clamp( static_cast<int>( distance / fStep ), 0, iSegCount - 1 ) };
    auto const piecestart { index * fStep };
    auto const pieceend { ( index + 1 == iSegCount ) ? fLength : ( index + 1 ) * fStep };
    auto const piecelength { pieceend - piecestart };
    auto const blend { (
        piecelength > 0.0 ?
            std::clamp( ( distance - piecestart ) / piecelength, 0.0, 1.0 ) :
            0.0 ) };

    auto const offset { 0.1 / fLength };
    auto const crossvector = [ & ]( double const T ) {
        auto const direction { glm::vec3{ FastGetDirection( T, offset ) } };
        auto parallel { glm::vec3{ -direction.z, 0.f, direction.x } };
        if( glm::length2( parallel ) == 0.f ) {
            parallel = glm::vec3{ glm::dvec3{ FastGetPoint_1() - FastGetPoint_0() } };
        }
        return glm::normalize( parallel ); };

    auto const t1 { fTsBuffer[ index ] };
    auto const t2 { fTsBuffer[ index + 1 ] };
    auto const point1 { FastGetPoint( t1 ) };
    auto const point2 { FastGetPoint( t2 ) };

    Position = glm::mix( point1, point2, blend );
    // NOTE: intentionally not normalized, the loft interpolates its vertices the same way
    Parallel = glm::mix( crossvector( t1 ), crossvector( t2 ), static_cast<float>( blend ) );
    auto const direction { glm::vec3{ point2 - point1 } };
    if( glm::length2( direction ) == 0.f ) { return false; }
    Direction = glm::normalize( direction );
    Progress = static_cast<float>( ( index + blend ) / iSegCount );

    return true;
}

// generates lofted geometry the way RenderLoft() does, but split into separate pieces with gaps between them
bool TSegment::RenderLoftGapped( gfx::vertex_array &Output, gfx::vertex_array &Capvertices, gfx::index_array &Capindices, glm::dvec3 const &Origin, gfx::vertex_array const &ShapePoints, bool const Transition, double const Texturelength, std::vector<double> const &Gaps, double const Gapwidth ) {

    if( fTsBuffer.empty() ) { return false; }
    if( fLength <= 0.0 ) { return false; }
    if( iSegCount < 1 ) { return false; }
    if( fStep <= 0.0 ) { return false; }
    if( Texturelength <= 0.0 ) { return false; }

    auto const pointcount { static_cast<int>( Transition ? ShapePoints.size() / 2 : ShapePoints.size() ) };
    if( pointcount < 2 ) { return false; }

    // cross-section of the loft at specified distance from the start of the segment. texture v coordinate is left for the caller to fill
    auto const make_ring = [ & ]( double const Distance, gfx::vertex_array &Ring, glm::vec3 &Direction ) {
        glm::dvec3 position;
        glm::vec3 parallel;
        float progress;
        if( false == LoftFrameGet( Distance, position, parallel, Direction, progress ) ) { return false; }
        auto const origin { glm::vec3{ position - Origin } };
        Ring.resize( pointcount );
        for( int j = 0; j < pointcount; ++j ) {
            auto const &shape1 { ShapePoints[ j ] };
            if( Transition ) {
                // profile blends from the starting to the ending shape along the segment
                auto const &shape2 { ShapePoints[ j + pointcount ] };
                auto point { parallel * glm::mix( shape1.position.x, shape2.position.x, progress ) + origin };
                point.y += glm::mix( shape1.position.y, shape2.position.y, progress );
                auto normal { glm::mix( shape1.normal.x, shape2.normal.x, progress ) * parallel };
                normal.y += glm::mix( shape1.normal.y, shape2.normal.y, progress );
                Ring[ j ] = { point, glm::normalize( normal ), glm::vec2{ glm::mix( shape1.texture.x, shape2.texture.x, progress ), 0.f } };
            }
            else {
                auto point { parallel * shape1.position.x + origin };
                point.y += shape1.position.y;
                auto normal { shape1.normal.x * parallel };
                normal.y += shape1.normal.y;
                Ring[ j ] = { point, glm::normalize( normal ), glm::vec2{ shape1.texture.x, 0.f } };
            }
        }
        return true; };

    // closes the cut end of a piece. Facing: 1 if the end faces the end of the segment, -1 if it faces its start
    auto const add_cap = [ & ]( gfx::vertex_array const &Ring, glm::vec3 const &Direction, float const Facing, float const Texturev ) {
        // the profile outline is expected to go up one side of the shape and down the other,
        // so points equally distant from both ends of the list can be bridged with quads
        if( pointcount < 4 ) { return; }
        if( ( pointcount % 2 ) != 0 ) { return; }
        auto const normal { Direction * Facing };
        auto const base { static_cast<gfx::basic_index>( Capvertices.size() ) };
        for( int j = 0; j < pointcount; ++j ) {
            Capvertices.emplace_back(
                Ring[ j ].position,
                normal,
                glm::vec2{ Ring[ j ].texture.x, Texturev + 0.5f * ShapePoints[ j ].position.y } );
        }
        for( int j = 0; j < pointcount / 2 - 1; ++j ) {
            gfx::basic_index const a = base + j;
            gfx::basic_index const b = base + j + 1;
            gfx::basic_index const c = base + pointcount - 2 - j;
            gfx::basic_index const d = base + pointcount - 1 - j;
            // wind the face counter-clockwise as seen from outside
            auto const facenormal {
                glm::cross(
                    Capvertices[ b ].position - Capvertices[ a ].position,
                    Capvertices[ c ].position - Capvertices[ a ].position ) };
            if( glm::dot( facenormal, normal ) >= 0.f ) { Capindices.insert( Capindices.end(), { a, b, c, a, c, d } ); }
            else                                        { Capindices.insert( Capindices.end(), { a, c, b, a, d, c } ); }
        }
    };

    auto const halfgap { 0.5 * Gapwidth };
    auto const texturev = [ & ]( double const Distance ) {
        return clamp_circular( static_cast<float>( 1.0 - Distance / Texturelength ), 1.f ); };

    gfx::vertex_array ring1, ring2;
    glm::vec3 direction1, direction2;
    std::vector<double> stations;
    auto piecestart { 0.0 };
    auto piececount { 0 };

    for( std::size_t gapindex = 0; gapindex <= Gaps.size(); ++gapindex ) {

        auto const pieceend { gapindex < Gaps.size() ? Gaps[ gapindex ] - halfgap : fLength };
        if( pieceend - piecestart > 0.001 ) {
            // cross-sections go at the ends of the piece and wherever regular loft would have them
            stations.clear();
            stations.emplace_back( piecestart );
            for( int k = static_cast<int>( piecestart / fStep ); k * fStep < pieceend - 0.001; ++k ) {
                if( k * fStep > piecestart + 0.001 ) {
                    stations.emplace_back( k * fStep );
                }
            }
            stations.emplace_back( pieceend );

            if( false == make_ring( stations.front(), ring1, direction1 ) ) { return false; }
            if( piecestart > 0.0 ) {
                add_cap( ring1, direction1, -1.f, texturev( piecestart ) );
            }
            for( std::size_t stationindex = 1; stationindex < stations.size(); ++stationindex ) {
                auto const distance1 { stations[ stationindex - 1 ] };
                auto const distance2 { stations[ stationindex ] };
                if( false == make_ring( distance2, ring2, direction2 ) ) { return false; }
                auto const tv1 { texturev( distance1 ) };
                auto const tv2 { tv1 - static_cast<float>( ( distance2 - distance1 ) / Texturelength ) };
                if( stationindex == 1 && piececount > 0 && false == Output.empty() ) {
                    // the pieces share a single triangle strip; repeated vertices make the triangles spanning the gap vanish.
                    // two extra vertices, to preserve the winding order
                    Output.emplace_back( Output.back() );
                    Output.emplace_back( ring1[ 0 ].position, ring1[ 0 ].normal, glm::vec2{ ring1[ 0 ].texture.x, tv1 } );
                }
                for( int j = 0; j < pointcount; ++j ) {
                    Output.emplace_back( ring1[ j ].position, ring1[ j ].normal, glm::vec2{ ring1[ j ].texture.x, tv1 } );
                    Output.emplace_back( ring2[ j ].position, ring2[ j ].normal, glm::vec2{ ring2[ j ].texture.x, tv2 } );
                }
                std::swap( ring1, ring2 );
                direction1 = direction2;
            }
            if( gapindex < Gaps.size() ) {
                add_cap( ring1, direction1, 1.f, texturev( pieceend ) );
            }
            ++piececount;
        }
        if( gapindex < Gaps.size() ) {
            piecestart = Gaps[ gapindex ] + halfgap;
        }
    }

    return ( piececount > 0 );
}

bool TSegment::RenderLoft( gfx::vertex_array &Output, glm::dvec3 const &Origin, const gfx::vertex_array &ShapePoints, bool const Transition, double fTextureLength, double Texturescale, int iSkip, int iEnd, std::pair<float, float> fOffsetX, glm::vec3 **p, bool bRender)
{ // generowanie trójkątów dla odcinka trajektorii ruchu
    // standardowo tworzy triangle_strip dla prostego albo ich zestaw dla łuku
    // po modyfikacji - dla ujemnego (iNumShapePoints) w dodatkowych polach tabeli podany jest przekrój końcowy

    if( fTsBuffer.empty() )
        return false; // prowizoryczne zabezpieczenie przed wysypem - ustalić faktyczną przyczynę

    glm::vec3 pos1;
    glm::vec3 pos2;
    glm::vec3 dir;
    glm::vec3 parallel1;
    glm::vec3 parallel2;
    glm::vec3 pt;
    glm::vec3 norm;
    float s;
    float step;
    float fOffset;
    float tv1;
    float tv2;
    float t;
    float fEnd;
    auto const iNumShapePoints = Transition ? ShapePoints.size() / 2 : ShapePoints.size();
    auto const texturelength = static_cast<float>(fTextureLength * Texturescale);
    auto const texturescale = static_cast<float>(Texturescale);

    float m1;
    float jmm1;
    float m2;
    float jmm2; // pozycje względne na odcinku 0...1 (ale nie parametr Beziera)
    step = static_cast<float>(fStep);
    tv1 = 1.0; // Ra: to by można było wyliczać dla odcinka, wyglądało by lepiej
    s = static_cast<float>(fStep * iSkip); // iSkip - ile odcinków z początku pominąć
    int i = iSkip; // domyślnie 0
    t = static_cast<float>(fTsBuffer[ i ]); // tabela wattości t dla segmentów
    // BUG: length of spline can be 0, we should skip geometry generation for such cases
    fOffset = static_cast<float>(0.1 / fLength); // pierwsze 10cm
    pos1 = glm::dvec3{ FastGetPoint( t ) - Origin }; // wektor początku segmentu
    dir = glm::dvec3{ FastGetDirection( t, fOffset ) }; // wektor kierunku
    parallel1 = glm::vec3{ -dir.z, 0.f, dir.x }; // wektor poprzeczny
    if( glm::length2( parallel1 ) == 0.f ) {
        // temporary workaround for malformed situations with control points placed above endpoints
        parallel1 = glm::vec3{ glm::dvec3{ FastGetPoint_1() - FastGetPoint_0() } };
    }
    parallel1 = glm::normalize( parallel1 );
    if( iEnd == 0 )
        iEnd = iSegCount;
    fEnd = static_cast<float>(fLength * double( iEnd ) / double( iSegCount ));
/*
    m2 = s / fEnd;
*/
    m2 = static_cast<float>( i - iSkip ) / ( static_cast<float>(iEnd - iSkip) );

    jmm2 = 1.f - m2;

    while( i < iEnd ) {

        ++i; // kolejny punkt łamanej
        s += step; // końcowa pozycja segmentu [m]
        m1 = m2;
        jmm1 = jmm2; // stara pozycja
/*
        m2 = s / fEnd;
*/
        m2 = static_cast<float>( i - iSkip ) / ( static_cast<float>(iEnd - iSkip) );

        jmm2 = 1.f - m2; // nowa pozycja
        if( i == iEnd ) { // gdy przekroczyliśmy koniec - stąd dziury w torach...
            step -= s - fEnd; // jeszcze do wyliczenia mapowania potrzebny
            s = fEnd;
            m2 = 1.f;
            jmm2 = 0.f;
        }
/*
        while( tv1 < 0.0 ) {
            tv1 += 1.0;
        }
*/
        tv1 = clamp_circular( tv1, 1.0f );
        tv2 = tv1 - step / texturelength; // mapowanie na końcu segmentu

        t = static_cast<float>(fTsBuffer[ i ]); // szybsze od GetTFromS(s);
        pos2 = glm::dvec3{ FastGetPoint( t ) - Origin };
        dir = glm::dvec3{ FastGetDirection( t, fOffset ) }; // nowy wektor kierunku
        parallel2 = glm::vec3{ -dir.z, 0.f, dir.x }; // wektor poprzeczny
        if( glm::length2( parallel2 ) == 0.f ) {
            // temporary workaround for malformed situations with control points placed above endpoints
            parallel2 = glm::vec3{ glm::dvec3{ FastGetPoint_1() - FastGetPoint_0() } };
        }
        parallel2 = glm::normalize( parallel2 );

        // TODO: refactor the loop, there's no need to calculate starting points for each segment when we can copy the end points of the previous one
        if( Transition ) {
            for( int j = 0; j < iNumShapePoints; ++j ) {
                pt = parallel1 * ( jmm1 * ( ShapePoints[ j ].position.x - fOffsetX.first ) + m1 * ( ShapePoints[ j + iNumShapePoints ].position.x - fOffsetX.second ) ) + pos1;
                pt.y += jmm1 * ShapePoints[ j ].position.y + m1 * ShapePoints[ j + iNumShapePoints ].position.y;
                norm = ( jmm1 * ShapePoints[ j ].normal.x + m1 * ShapePoints[ j + iNumShapePoints ].normal.x ) * parallel1;
                norm.y += jmm1 * ShapePoints[ j ].normal.y + m1 * ShapePoints[ j + iNumShapePoints ].normal.y;
                if( bRender ) {
                    // skrzyżowania podczas łączenia siatek mogą nie renderować poboczy, ale potrzebować punktów
                    Output.emplace_back(
                        pt,
                        glm::normalize( norm ),
                        glm::vec2 { ( jmm1 * ShapePoints[ j ].texture.x + m1 * ShapePoints[ j + iNumShapePoints ].texture.x ) / texturescale, tv1 } );
                }
                // jeśli jest wskaźnik do tablicy
                if (p && *p && !j) // to dla pierwszego punktu
                {
                    **p = pt;
                    ( *p )++;
                } // zapamiętanie brzegu jezdni
                // dla trapezu drugi koniec ma inne współrzędne
                pt = parallel2 * ( jmm2 * ( ShapePoints[ j ].position.x - fOffsetX.first ) + m2 * ( ShapePoints[ j + iNumShapePoints ].position.x - fOffsetX.second ) ) + pos2;
                pt.y += jmm2 * ShapePoints[ j ].position.y + m2 * ShapePoints[ j + iNumShapePoints ].position.y;
                norm = ( jmm2 * ShapePoints[ j ].normal.x + m2 * ShapePoints[ j + iNumShapePoints ].normal.x ) * parallel2;
                norm.y += jmm2 * ShapePoints[ j ].normal.y + m2 * ShapePoints[ j + iNumShapePoints ].normal.y;
                if( bRender ) {
                    // skrzyżowania podczas łączenia siatek mogą nie renderować poboczy, ale potrzebować punktów
                    Output.emplace_back(
                        pt,
                        glm::normalize( norm ),
                        glm::vec2 { ( jmm2 * ShapePoints[ j ].texture.x + m2 * ShapePoints[ j + iNumShapePoints ].texture.x ) / texturescale, tv2 } );
                }
                // jeśli jest wskaźnik do tablicy
                // to dla pierwszego punktu
                if (p && *p && !j && i == iSegCount) {
                    **p = pt;
                    ( *p )++;
                } // zapamiętanie brzegu jezdni
            }
        }
        else {
            if( bRender  ) {
                for( int j = 0; j < iNumShapePoints; ++j ) {
                    //łuk z jednym profilem
                    pt = parallel1 * ( jmm1 * ( ShapePoints[ j ].position.x - fOffsetX.first ) + m1 * ( ShapePoints[ j ].position.x - fOffsetX.second ) ) + pos1;
                    pt.y += ShapePoints[ j ].position.y;
                    norm = ShapePoints[ j ].normal.x * parallel1;
                    norm.y += ShapePoints[ j ].normal.y;

                    Output.emplace_back(
                        pt,
                        glm::normalize( norm ),
                        glm::vec2 { ShapePoints[ j ].texture.x / texturescale, tv1 } );

                    pt = parallel2 * ( jmm2 * ( ShapePoints[ j ].position.x - fOffsetX.first ) + m2 * ( ShapePoints[ j ].position.x - fOffsetX.second ) ) + pos2;
                    pt.y += ShapePoints[ j ].position.y;
                    norm = ShapePoints[ j ].normal.x * parallel2;
                    norm.y += ShapePoints[ j ].normal.y;

                    Output.emplace_back(
                        pt,
                        glm::normalize( norm ),
                        glm::vec2 { ShapePoints[ j ].texture.x / texturescale, tv2 } );
                }
            }
        }
        pos1 = pos2;
        parallel1 = parallel2;
        tv1 = tv2;
    }
    return true;
};

void TSegment::render_lines(std::vector<gfx::basic_vertex> &out, float quality) const
{
	float step = 1.0f / iSegCount / quality;
	auto const stepcount = static_cast<int>(static_cast<float>(iSegCount) * quality);

	glm::vec3 previous = FastGetPoint(0.0);

	for (int i = 1; i <= stepcount; ++i) {
		float const x = std::min(1.0f, static_cast<float>(i) * step);
		out.push_back(gfx::basic_vertex(previous, glm::vec3(0.0f), glm::vec2(0.0f)));

		previous = glm::vec3(FastGetPoint(x));
		out.emplace_back(previous, glm::vec3(0.0f), glm::vec2(0.0f));
	}

	out.emplace_back(previous, glm::vec3(0.0f), glm::vec2(0.0f));

	previous = glm::vec3(FastGetPoint(1.0));
	out.emplace_back(previous, glm::vec3(0.0f), glm::vec2(0.0f));
}

glm::vec3 TSegment::get_nearest_point(const glm::dvec3 &point, float quality) const
{
	float step = 1.0f / iSegCount / quality;
	auto const stepcount = static_cast<int>(static_cast<float>(iSegCount) * quality);

	glm::vec3 nearest;
	float min = std::numeric_limits<float>::max();

	for (int i = 1; i <= stepcount; ++i) {
		float const x = std::min(1.0f, static_cast<float>(i) * step);
        glm::vec3 p1 = FastGetPoint(x);

        if (glm::vec3 p2 = FastGetPoint(glm::min(1.0f, x + step)); p1 != p2) {
            float l2 = glm::distance2(p1, p2);
            float t = glm::max(0.0f, glm::min(1.0f, glm::dot((glm::vec3)point - p1, p2 - p1) / l2));
            glm::vec3 proj = p1 + t * (p2 - p1);
            p1 = proj;
        }

        float dist2 = glm::distance2(p1, (glm::vec3)point);

		if (dist2 < min) {
            nearest = p1;
			min = dist2;
		}
	}

	return nearest;
}
