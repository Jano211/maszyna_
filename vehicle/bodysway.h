/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

// physical model of vehicle body movement on its suspension, driven by track irregularities.
// self-contained (standard library only) so it can be tested outside of the simulator.
//
// the body is a rigid mass resting on two bogies (or two single axles):
//   rail -> axles -> primary suspension (bogie frame) -> secondary suspension (body) -> viewpoint
// each axle samples the track surface at its own location, so the rhythm of rail joints
// comes out of the axle layout and speed on its own.
//
// axes, vehicle space: x - sideways, y - up, z - forward (towards coupler 0)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace sway {

constexpr double pi { 3.14159265358979323846 };
constexpr double gravity { 9.81 };
constexpr double gauge { 1.5 }; // distance between rail head centres, for cross-level -> angle conversion

// state of the track surface under a single axle
struct rail_sample {
    double vertical { 0.0 }; // mean height of both rails, metres (negative = dip)
    double lateral { 0.0 }; // sideways alignment error, metres
    double crosslevel { 0.0 }; // rail height difference, as an angle in radians (positive = +x rail higher)

    rail_sample &operator+=( rail_sample const &Right ) {
        vertical += Right.vertical;
        lateral += Right.lateral;
        crosslevel += Right.crosslevel;
        return *this; }
};

// deterministic pseudo-random value in range 0-1 for given key
inline
double
hash01( std::uint32_t Key ) {
    Key ^= Key >> 16; Key *= 0x7feb352dU;
    Key ^= Key >> 15; Key *= 0x846ca68bU;
    Key ^= Key >> 16;
    return ( Key & 0xffffffU ) / static_cast<double>( 0x1000000 );
}

// continuous track unevenness at given place in the world.
// the unevenness belongs to the place and not to the vehicle: every vehicle passing over it gets the same treatment,
// one after another, and neighbouring tracks have unevenness of their own.
// X, Z: world location. Left, Front: horizontal components of unit vectors pointing to the left and to the front of the vehicle
// Roughness: 1.0 - maintained track, higher values - worse track
inline
rail_sample
track_roughness( double const X, double const Z, double const Leftx, double const Leftz, double const Frontx, double const Frontz, double const Roughness ) {

    struct wave { double length; double amplitude; double phase; double heading; };
    // lengths and amplitudes in metres. each wave is laid across the world in two directions,
    // so its length along a track varies with direction of the track, and never falls below the listed value
    static constexpr std::array<wave, 5> vertical { {
        { 25.0, 0.0022, 0.3, 0.2 }, { 14.0, 0.0016, 1.9, 1.3 }, { 7.8, 0.0010, 4.1, 2.5 }, { 4.7, 0.0006, 2.6, 0.9 }, { 2.5, 0.0003, 5.2, 1.9 } } };
    static constexpr std::array<wave, 4> lateral { {
        { 33.0, 0.0020, 1.1, 0.7 }, { 17.7, 0.0014, 3.7, 2.1 }, { 9.8, 0.0008, 0.6, 0.1 }, { 5.7, 0.0004, 4.8, 1.5 } } };
    static constexpr std::array<wave, 4> crosslevel { {
        { 21.6, 0.0038, 2.2, 1.0 }, { 11.9, 0.0028, 5.6, 2.8 }, { 6.6, 0.0015, 1.4, 0.4 }, { 3.8, 0.0008, 3.3, 2.2 } } };

    // value of a field made of given wave at current location. Variant: selects one of independent fields made of the same wave
    auto const field = [ & ]( wave const &Wave, int const Variant ) {
        auto sum { 0.0 };
        for( int direction = 0; direction < 2; ++direction ) {
            auto const angle { Wave.heading + 1.1 * Variant + direction * ( pi / 3.0 ) };
            sum += std::sin(
                2.0 * pi * ( X * std::cos( angle ) + Z * std::sin( angle ) ) / Wave.length
                + Wave.phase + 2.1 * direction + 3.7 * Variant );
        }
        return Wave.amplitude * sum * 0.7071; };

    // stretches of better and worse track, a few hundred metres across
    auto const patch {
        0.85
        + 0.30 * std::sin( 2.0 * pi * ( 0.8 * X + 0.6 * Z ) / 310.0 + 1.3 )
        + 0.25 * std::sin( 2.0 * pi * ( -0.6 * X + 0.8 * Z ) / 170.0 + 4.0 ) };
    auto const scale { Roughness * patch };

    rail_sample sample;
    for( auto const &w : vertical ) {
        sample.vertical += field( w, 0 );
    }
    // sideways shift and tilt are directions in the world, the vehicle gets as much of them as it's turned towards
    for( auto const &w : lateral ) {
        sample.lateral += field( w, 0 ) * Leftx + field( w, 1 ) * Leftz;
    }
    for( auto const &w : crosslevel ) {
        sample.crosslevel += ( field( w, 0 ) * Frontx + field( w, 1 ) * Frontz ) / gauge;
    }
    sample.vertical *= scale;
    sample.lateral *= scale;
    sample.crosslevel *= scale;
    return sample;
}

// dip of the rails at a joint.
// Offset: distance from the joint along the track, metres
// Key: unique id of the joint, makes each joint slightly different
// Depth: nominal depth of the dip, metres
// Sidekick: amount of sideways kick, metres (rail crossings in switches)
inline
rail_sample
joint_dip( double const Offset, std::uint32_t const Key, double const Depth, double const Sidekick = 0.0 ) {

    constexpr double reach { 0.3 }; // length scale of the dip, metres
    auto const distance { std::abs( Offset ) };
    if( distance > 12.0 * reach ) { return {}; }

    auto const shape { std::exp( -distance / reach ) };
    // no two joints are alike, and rails of a pair never wear down identically
    auto const condition { 0.3 + 1.2 * hash01( Key * 2U + 4U ) };
    auto const left { Depth * condition * ( 0.8 + 0.4 * hash01( Key * 2U + 1U ) ) };
    auto const right { Depth * condition * ( 0.8 + 0.4 * hash01( Key * 2U + 2U ) ) };

    rail_sample sample;
    sample.vertical = -0.5 * ( left + right ) * shape;
    sample.crosslevel = ( right - left ) * shape / gauge;
    sample.lateral = Sidekick * ( hash01( Key * 2U + 3U ) < 0.5 ? -1.0 : 1.0 ) * shape;
    return sample;
}

// dip of the track surface at a gap in one rail: a frog, a crossing with another track. Offset: distance from the gap, metres
// Side: the rail with the gap as seen from the vehicle, 1 right, -1 left, 0 both; Depth: how far the wheel drops into it, metres
// Sidekick: how far the check rail or the blade shoves the wheelset sideways, metres
inline
rail_sample
gap_dip( double const Offset, std::uint32_t const Key, double const Depth, double const Sidekick, int const Side ) {

    constexpr double reach { 0.25 }; // length scale of the dip, metres: the gap is short, the wheel lands on the other side of it
    auto const distance { std::abs( Offset ) };
    if( distance > 12.0 * reach ) { return {}; }

    auto const shape { std::exp( -distance / reach ) };
    auto const depth { Depth * ( 0.7 + 0.6 * hash01( Key * 2U + 4U ) ) * shape };
    auto const right { Side >= 0 ? depth : 0.0 };
    auto const left { Side <= 0 ? depth : 0.0 };

    rail_sample sample;
    sample.vertical = -0.5 * ( left + right );
    sample.crosslevel = ( right - left ) / gauge;
    // the check rail pushes the wheelset away from the rail with the gap
    sample.lateral = Sidekick * ( Side != 0 ? static_cast<double>( Side ) : ( hash01( Key * 2U + 3U ) < 0.5 ? -1.0 : 1.0 ) ) * shape;
    return sample;
}

// damped oscillator following a moving base
struct oscillator {
    double value { 0.0 };
    double rate { 0.0 };

    void
        step( double const Target, double const Targetrate, double const Frequency, double const Damping, double const Acceleration, double const Step ) {
            auto const omega { 2.0 * pi * Frequency };
            rate += ( -omega * omega * ( value - Target ) - 2.0 * Damping * omega * ( rate - Targetrate ) + Acceleration ) * Step;
            value += rate * Step; }
};

struct body_config {
    double bogie_distance { 10.0 }; // distance between bogie pivots (or between axles of a two-axle vehicle)
    double axle_distance { 0.0 }; // distance between neighbouring axles of a bogie
    int axles_per_bogie { 2 }; // 1 if the supports are single axles
    // primary suspension (axle -> bogie frame)
    double primary_frequency { 5.5 };
    double primary_damping { 0.35 };
    // secondary suspension (bogie frame -> body), natural frequencies in Hz and damping ratios
    double heave_frequency { 1.25 };
    double heave_damping { 0.38 };
    double pitch_frequency { 1.5 };
    double pitch_damping { 0.40 };
    double roll_frequency { 0.75 };
    double roll_damping { 0.28 };
    double sway_frequency { 0.9 };
    double sway_damping { 0.32 };
    double yaw_frequency { 1.1 };
    double yaw_damping { 0.3 };
    double surge_frequency { 1.4 };
    double surge_damping { 0.45 };
    double roll_centre_height { 0.8 }; // above rail head
    double roll_coefficient { 0.12 }; // body roll angle per unit of unbalanced lateral acceleration, as fraction of a/g
    double sway_gain { 0.3 }; // how much of lateral acceleration pushes the body sideways
    double surge_gain { 1.5 }; // how much of longitudinal acceleration pushes the observer along the vehicle
    double sway_roll_coupling { 0.4 }; // body tilt caused by its sideways shift on the suspension, radians per metre
    // wheelsets. tapered wheels make a wheelset steer itself back towards the middle of the track, overshoot and repeat,
    // weaving from side to side over a fixed distance travelled (kinematic hunting).
    // the weaving dies out at low speeds and feeds itself above the critical speed, until the flanges start hitting the rails
    double wheel_radius { 0.46 }; // metres
    double conicity { 0.12 }; // effective taper of the wheel tread; grows as the wheels wear
    double flange_clearance { 0.006 }; // room for the wheelset to move sideways between the rails, each way, metres
    double critical_speed { 1000.0 }; // m/s
    double hunting_damping { 0.15 }; // damping ratio of the weaving at standstill; falls with speed, reaching 0 at critical speed
    // head of the observer: carried by the body, but with inertia of its own
    double head_frequency { 2.2 }; // Hz
    double head_damping { 0.45 };
    double head_gain { 1.0 }; // 1.0: actual movement of the head in relation to the cab; higher values exaggerate it
    double head_lean { 0.012 }; // sideways lean under lateral acceleration, metres per m/s^2
    double head_roll_share { 0.3 }; // part of the body tilt the head doesn't follow, staying level
    double floor_vibration { 0.12 }; // share of bogie frame movement (relative to the body) passed to the floor near that bogie
};

struct body_input {
    double speed { 0.0 }; // m/s, positive towards coupler 0
    double lateral_acceleration { 0.0 }; // m/s^2, unbalanced
    double longitudinal_acceleration { 0.0 }; // m/s^2
    // tilt of the track already applied to the vehicle as a whole, radians. the track samples carry actual tilt of the track
    // under each bogie, and the results are given in relation to this value
    double reference_roll { 0.0 };
    // location of the viewpoint in vehicle space, relative to vehicle centre at rail level
    double view_x { 0.0 };
    double view_y { 2.5 };
    double view_z { 0.0 };
};

// displacement of the viewpoint from its rest position
struct body_pose {
    double x { 0.0 }, y { 0.0 }, z { 0.0 }; // metres
    double roll { 0.0 }, pitch { 0.0 }, yaw { 0.0 }; // radians
};

class body {

public:
// members
    body_config config;
    // strength of track unevenness currently applied under each end of the vehicle; negative until first use.
    // kept by the user of the model, to let the strength change gradually between tracks of different quality
    double roughness_scale[ 2 ] { -1.0, -1.0 };
    bool reported { false }; // whether the user of the model already logged its setup
// methods
    void
        reset() {
            auto const settings { config };
            *this = body {};
            config = settings; }
    // fades the movement out without running the simulation
    void
        relax( double const Timedelta ) {
            auto const keep { std::clamp( 1.0 - 3.0 * Timedelta, 0.0, 1.0 ) };
            for( auto *mode : { &m_heave, &m_pitch, &m_sway, &m_yaw, &m_surge } ) {
                mode->value *= keep;
                mode->rate *= keep; }
            m_roll.value = m_reference_roll + ( m_roll.value - m_reference_roll ) * keep;
            m_roll.rate *= keep;
            for( auto &bogie : m_bogies ) {
                for( auto *mode : { &bogie.vertical, &bogie.lateral, &bogie.roll, &bogie.wheelset } ) {
                    mode->value *= keep;
                    mode->rate *= keep; } }
            for( auto *pose : { &m_pose, &m_rigid, &m_residual } ) {
                pose->x *= keep; pose->y *= keep; pose->z *= keep;
                pose->roll *= keep; pose->pitch *= keep; pose->yaw *= keep; } }
    // advances the simulation.
    // Sampler: callable returning rail_sample for ( bogie index, axle offset from vehicle centre, distance to move back from current position )
    template <typename Sampler_>
    void
        update( double const Timedelta, body_input const &Input, Sampler_ &&Sampler ) {

            if( Timedelta <= 0.0 ) { return; }
            // short sub-steps, so a joint isn't skipped over between two updates at speed
            auto const travel { std::abs( Input.speed ) * Timedelta };
            auto const stepcount { std::clamp( static_cast<int>( std::ceil( std::max( Timedelta / 0.005, travel / 0.12 ) ) ), 1, 24 ) };
            auto const step { Timedelta / stepcount };
            auto const halfspan { 0.5 * std::max( config.bogie_distance, 0.5 ) };
            auto const axlecount { config.axle_distance > 0.0 ? std::clamp( config.axles_per_bogie, 1, 4 ) : 1 };
            // kinematic hunting
            auto const huntingomega { 2.0 * pi * std::abs( Input.speed ) / hunting_wavelength() };
            auto const speedratio { std::abs( Input.speed ) / std::max( config.critical_speed, 1.0 ) };
            auto const huntingdamping { std::max( config.hunting_damping * ( 1.0 - speedratio * speedratio ), -0.08 ) };
            auto const clearance { std::max( config.flange_clearance, 0.001 ) };

            for( int stepindex = 1; stepindex <= stepcount; ++stepindex ) {
                // how far back from the final position the vehicle is at the end of this sub-step
                auto const back { Input.speed * Timedelta * ( 1.0 - static_cast<double>( stepindex ) / stepcount ) };

                for( int bogieindex = 0; bogieindex < 2; ++bogieindex ) {
                    auto &bogie { m_bogies[ bogieindex ] };
                    auto const pivot { bogieindex == 0 ? halfspan : -halfspan };
                    rail_sample input;
                    for( int axleindex = 0; axleindex < axlecount; ++axleindex ) {
                        auto const offset { pivot + ( 0.5 * ( axlecount - 1 ) - axleindex ) * config.axle_distance };
                        input += Sampler( bogieindex, offset, back );
                    }
                    input.vertical /= axlecount;
                    input.lateral /= axlecount;
                    input.crosslevel /= axlecount;
                    // rate of change of the track under the bogie.
                    // a jump means the reference changed (first run, other track, teleport), not a real movement
                    rail_sample rate;
                    if( bogie.primed
                     && std::abs( input.vertical - bogie.input.vertical ) < 0.02
                     && std::abs( input.lateral - bogie.input.lateral ) < 0.02 ) {
                        rate.vertical = ( input.vertical - bogie.input.vertical ) / step;
                        rate.lateral = ( input.lateral - bogie.input.lateral ) / step;
                        rate.crosslevel = ( input.crosslevel - bogie.input.crosslevel ) / step;
                    }
                    if( false == bogie.primed ) {
                        // first contact with the track, start at rest on it instead of dropping onto it
                        bogie.vertical = { input.vertical, 0.0 };
                        bogie.lateral = { input.lateral, 0.0 };
                        bogie.wheelset = { input.lateral, 0.0 };
                        bogie.roll = { input.crosslevel, 0.0 };
                    }
                    bogie.input = input;
                    bogie.primed = true;

                    bogie.vertical.step( input.vertical, rate.vertical, config.primary_frequency, config.primary_damping, 0.0, step );
                    // wheelsets weave along the track...
                    auto &wheelset { bogie.wheelset };
                    wheelset.rate += (
                        -huntingomega * huntingomega * ( wheelset.value - input.lateral )
                        - 2.0 * huntingdamping * huntingomega * ( wheelset.rate - rate.lateral ) ) * step;
                    wheelset.value += wheelset.rate * step;
                    auto offset { wheelset.value - input.lateral };
                    if( std::abs( offset ) > clearance ) {
                        // ...as far as the flanges let them
                        offset = std::copysign( clearance, offset );
                        wheelset.value = input.lateral + offset;
                        auto const closing { wheelset.rate - rate.lateral };
                        if( closing * offset > 0.0 ) {
                            wheelset.rate = rate.lateral - 0.3 * closing;
                        }
                    }
                    bogie.hunting = std::max( std::abs( offset ) / clearance, bogie.hunting * ( 1.0 - 0.5 * step ) );
                    // ...and take the bogie frame with them
                    bogie.lateral.step( wheelset.value, wheelset.rate, config.primary_frequency, config.primary_damping, 0.0, step );
                    bogie.roll.step( input.crosslevel, rate.crosslevel, config.primary_frequency, config.primary_damping, 0.0, step );
                }

                auto const &front { m_bogies[ 0 ] };
                auto const &rear { m_bogies[ 1 ] };
                auto const span { 2.0 * halfspan };
                if( false == m_primed ) {
                    // likewise for the body
                    m_heave = { 0.5 * ( front.vertical.value + rear.vertical.value ), 0.0 };
                    m_pitch = { ( front.vertical.value - rear.vertical.value ) / span, 0.0 };
                    m_roll = { 0.5 * ( front.roll.value + rear.roll.value ), 0.0 };
                    m_sway = { 0.5 * ( front.lateral.value + rear.lateral.value ), 0.0 };
                    m_yaw = { ( front.lateral.value - rear.lateral.value ) / span, 0.0 };
                    m_primed = true;
                }

                m_heave.step(
                    0.5 * ( front.vertical.value + rear.vertical.value ),
                    0.5 * ( front.vertical.rate + rear.vertical.rate ),
                    config.heave_frequency, config.heave_damping, 0.0, step );
                m_pitch.step(
                    ( front.vertical.value - rear.vertical.value ) / span,
                    ( front.vertical.rate - rear.vertical.rate ) / span,
                    config.pitch_frequency, config.pitch_damping, 0.0, step );
                m_roll.step(
                    0.5 * ( front.roll.value + rear.roll.value )
                    + config.roll_coefficient * Input.lateral_acceleration / gravity
                    - config.sway_roll_coupling * ( m_sway.value - 0.5 * ( front.lateral.value + rear.lateral.value ) ),
                    0.5 * ( front.roll.rate + rear.roll.rate ),
                    config.roll_frequency, config.roll_damping, 0.0, step );
                m_sway.step(
                    0.5 * ( front.lateral.value + rear.lateral.value ),
                    0.5 * ( front.lateral.rate + rear.lateral.rate ),
                    config.sway_frequency, config.sway_damping, -config.sway_gain * Input.lateral_acceleration, step );
                m_yaw.step(
                    ( front.lateral.value - rear.lateral.value ) / span,
                    ( front.lateral.rate - rear.lateral.rate ) / span,
                    config.yaw_frequency, config.yaw_damping, 0.0, step );
                m_surge.step(
                    0.0, 0.0,
                    config.surge_frequency, config.surge_damping, -config.surge_gain * Input.longitudinal_acceleration, step );
            }

            if( false == is_sane() ) {
                // shouldn't happen, but a camera thrown into infinity is worse than a moment of stillness
                reset();
                return;
            }

            // movement of the body as seen from the viewpoint
            m_reference_roll = Input.reference_roll;
            auto const roll { m_roll.value - m_reference_roll }; // tilt of the body in relation to the vehicle frame
            auto const arm { Input.view_y - config.roll_centre_height };
            // movement of the nearer bogie frame gets through to the floor, softened.
            // NOTE: measured against the body and not the rail, a frame which doesn't follow a dip in the rail doesn't shake anything
            auto const frontshare { std::clamp( 0.5 + 0.5 * Input.view_z / halfspan, 0.0, 1.0 ) };
            auto const floor_vertical { config.floor_vibration * (
                frontshare * ( m_bogies[ 0 ].vertical.value - ( m_heave.value + m_pitch.value * halfspan ) )
                + ( 1.0 - frontshare ) * ( m_bogies[ 1 ].vertical.value - ( m_heave.value - m_pitch.value * halfspan ) ) ) };
            auto const floor_lateral { config.floor_vibration * (
                frontshare * ( m_bogies[ 0 ].lateral.value - ( m_sway.value + m_yaw.value * halfspan ) )
                + ( 1.0 - frontshare ) * ( m_bogies[ 1 ].lateral.value - ( m_sway.value - m_yaw.value * halfspan ) ) ) };

            m_pose.x = m_sway.value + m_yaw.value * Input.view_z - roll * arm + floor_lateral;
            m_pose.y = m_heave.value + m_pitch.value * Input.view_z + roll * Input.view_x + floor_vertical;
            m_pose.z = m_surge.value - m_pitch.value * arm;
            m_pose.roll = roll;
            m_pose.pitch = m_pitch.value;
            m_pose.yaw = m_yaw.value;

            constexpr double offsetlimit { 0.15 };
            constexpr double anglelimit { 0.07 }; // ~4 degrees
            m_pose.x = std::clamp( m_pose.x, -offsetlimit, offsetlimit );
            m_pose.y = std::clamp( m_pose.y, -offsetlimit, offsetlimit );
            m_pose.z = std::clamp( m_pose.z, -offsetlimit, offsetlimit );
            m_pose.roll = std::clamp( m_pose.roll, -anglelimit, anglelimit );
            m_pose.pitch = std::clamp( m_pose.pitch, -anglelimit, anglelimit );
            m_pose.yaw = std::clamp( m_pose.yaw, -anglelimit, anglelimit );

            // the same movement split in two: the body as a rigid object...
            m_rigid.x = std::clamp( m_sway.value, -offsetlimit, offsetlimit );
            m_rigid.y = std::clamp( m_heave.value, -offsetlimit, offsetlimit );
            m_rigid.z = 0.0;
            m_rigid.roll = m_pose.roll;
            m_rigid.pitch = m_pose.pitch;
            m_rigid.yaw = m_pose.yaw;
            // ...and what the observer experiences on top of it
            // the head lags behind movements of the place it's carried at
            auto const basex { m_sway.value + m_yaw.value * Input.view_z - roll * arm };
            auto const basey { m_heave.value + m_pitch.value * Input.view_z + roll * Input.view_x };
            if( false == m_headprimed ) {
                m_headx = { basex, 0.0 };
                m_heady = { basey, 0.0 };
                m_headbasex = basex;
                m_headbasey = basey;
                m_headprimed = true;
            }
            m_headx.step( basex, ( basex - m_headbasex ) / Timedelta, config.head_frequency, config.head_damping, 0.0, Timedelta );
            m_heady.step( basey, ( basey - m_headbasey ) / Timedelta, config.head_frequency * 1.4, config.head_damping, 0.0, Timedelta );
            m_headbasex = basex;
            m_headbasey = basey;
            if( false == ( std::abs( m_headx.value ) < 10.0 && std::abs( m_heady.value ) < 10.0 ) ) {
                m_headx = { basex, 0.0 };
                m_heady = { basey, 0.0 };
            }
            m_residual.x = std::clamp(
                floor_lateral
                + config.head_gain * ( m_headx.value - basex )
                - config.head_lean * Input.lateral_acceleration,
                -offsetlimit, offsetlimit );
            m_residual.y = std::clamp(
                floor_vertical
                + config.head_gain * ( m_heady.value - basey ),
                -offsetlimit, offsetlimit );
            m_residual.z = std::clamp( m_surge.value, -offsetlimit, offsetlimit );
            // tilt of the head in relation to the body
            m_residual.roll = -config.head_roll_share * m_pose.roll;
        }
    body_pose const &
        pose() const {
            return m_pose; }
    // distance over which the wheelsets complete one sway from side to side and back, metres
    double
        hunting_wavelength() const {
            constexpr double contactspan { 0.75 }; // half of the distance between wheel-rail contact points
            auto const axlecount { config.axle_distance > 0.0 ? std::clamp( config.axles_per_bogie, 1, 4 ) : 1 };
            // a wheelset held in a bogie frame can't turn freely, which stretches the distance
            auto const halfwheelbase { 0.5 * ( axlecount - 1 ) * config.axle_distance };
            return
                2.0 * pi
                * std::sqrt( std::max( config.wheel_radius, 0.2 ) * contactspan / std::max( config.conicity, 0.01 ) )
                * std::sqrt( 1.0 + ( halfwheelbase * halfwheelbase ) / ( contactspan * contactspan ) ); }
    // how much of the room between the rails the wheelsets have been using lately, 0-1; 1 means flanges hitting the rails
    double
        hunting() const {
            return std::max( m_bogies[ 0 ].hunting, m_bogies[ 1 ].hunting ); }
    // movement of the body as a rigid object: x, y - shift of the body at roll centre height, angles - rotation around that point
    body_pose const &
        rigid() const {
            return m_rigid; }
    // displacement of the viewpoint not covered by rigid(); for the case when the body movement is applied to the vehicle itself
    body_pose const &
        residual() const {
            return m_residual; }

private:
// types
    struct bogie_state {
        oscillator vertical;
        oscillator lateral;
        oscillator roll;
        oscillator wheelset; // sideways location of the wheelsets
        double hunting { 0.0 }; // how much of available clearance the wheelsets have been using lately, 0-1
        rail_sample input; // track under the bogie during previous step
        bool primed { false };
    };
// methods
    bool
        is_sane() const {
            constexpr double limit { 10.0 };
            for( auto const *mode : { &m_heave, &m_pitch, &m_roll, &m_sway, &m_yaw, &m_surge,
                                      &m_bogies[ 0 ].vertical, &m_bogies[ 0 ].lateral, &m_bogies[ 0 ].roll,
                                      &m_bogies[ 1 ].vertical, &m_bogies[ 1 ].lateral, &m_bogies[ 1 ].roll } ) {
                if( false == ( std::abs( mode->value ) < limit && std::abs( mode->rate ) < limit * 100.0 ) ) { return false; } // also catches NaN
            }
            return true; }
// members
    std::array<bogie_state, 2> m_bogies;
    oscillator m_heave; // up/down
    oscillator m_pitch; // nose up/down
    oscillator m_roll; // side to side tilt
    oscillator m_sway; // side to side shift
    oscillator m_yaw; // nose left/right
    oscillator m_surge; // observer pushed along the vehicle by acceleration and braking
    body_pose m_pose;
    body_pose m_rigid;
    body_pose m_residual;
    oscillator m_headx; // head of the observer, sideways
    oscillator m_heady; // and up/down
    double m_headbasex { 0.0 };
    double m_headbasey { 0.0 };
    bool m_headprimed { false };
    double m_reference_roll { 0.0 };
    bool m_primed { false };
};

} // namespace sway
