/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "rendering/common/skylight.h"

#include "environment/skydome.h"
#include "simulation/simulationenvironment.h"
#include "utilities/Globals.h"

namespace
{

// irradiance of a radiance given as spherical harmonics, divided by pi (the units of the ambient light of the shading:
// under a uniform sky of radiance L it is L): the coefficients of band l are scaled by A_l / pi (Ramamoorthi, Hanrahan 2001)
float const SH_IRRADIANCE_BAND[3]{1.f, 2.f / 3.f, 0.25f};
int const SH_BAND[gl::SKY_SH_COEFFICIENTS]{0, 1, 1, 1, 2, 2, 2, 2, 2};
float const SH_Y00{0.282095f};
float const SH_Y1{0.488603f};
float const PI{3.14159265359f};

// real spherical harmonics of order 2 in a world space direction, in the order of light_common.glsl
std::array<float, gl::SKY_SH_COEFFICIENTS> sh_basis(glm::vec3 const &n)
{
	return {SH_Y00,
	        SH_Y1 * n.y,
	        SH_Y1 * n.z,
	        SH_Y1 * n.x,
	        1.092548f * n.x * n.y,
	        1.092548f * n.y * n.z,
	        0.315392f * (3.f * n.z * n.z - 1.f),
	        1.092548f * n.x * n.z,
	        0.546274f * (n.x * n.x - n.y * n.y)};
}

float luminance(glm::vec3 const &Color)
{
	return glm::dot(Color, glm::vec3{0.2126f, 0.7152f, 0.0722f});
}

glm::vec3 desaturate(glm::vec3 const &Color, float const Amount)
{
	return glm::mix(Color, glm::vec3{luminance(Color)}, Amount);
}

// the response of color.frag to the vertex colours of the dome, so the light taken from the dome is the light it shows
glm::vec3 sky_shoulder(glm::vec3 const &Color)
{
	float const knee{0.25f};
	float const range{0.95f - knee};
	auto const excess{glm::max(Color - glm::vec3{knee}, glm::vec3{0.f})};
	return glm::min(Color, glm::vec3{knee}) + range * (glm::vec3{1.f} - glm::exp(-excess / range));
}

// the atmosphere is calibrated against the old sky: these scales make a sky of the atmosphere, with the sun 15 to 60 degrees
// high on a clear day, as bright as the old Perez dome is drawn on average, and the ambient light and the sunlight as strong
// as the old ones, with the default ground albedo (measured on the tables worked out by the same shaders on a software
// renderer, see the notes of the plan, stage S2)
float const ATMOSPHERE_SKY_SCALE{16.f};
float const ATMOSPHERE_AMBIENT_SCALE{44.f};
float const ATMOSPHERE_SUN_SCALE{1.45f};
// sun elevations (degrees) between which the light of the scene (sunlight, ambient light, fog colour) goes over from the
// old model (night, twilight) to the atmosphere. the atmosphere darkens at dusk much faster than the old model, which the
// rest of the lighting was tuned to. the drawn sky is the atmosphere's at every height of the sun (see sky_brightness)
float const ATMOSPHERE_FADE_START{-6.f};
float const ATMOSPHERE_FADE_END{10.f};
// the brightness of the drawn sky. from the day to the end of the twilight the light of the sky falls by about eight orders
// of magnitude; the eye adapts to most of it, the automatic exposure of the renderer only to a little (a stop or two). so the
// sky is drawn as bright as the atmosphere makes it while its average radiance is above SKY_ADAPTATION_START (the sun about
// 6 degrees high), and below that as the radiance to the power of SKY_ADAPTATION: a fall of the light by 10^n shows as a fall
// of 10^(0.4 n). the colours and the shape of the sky stay those of the atmosphere, only its brightness is evened out
float const SKY_ADAPTATION_START{0.015f};
float const SKY_ADAPTATION{0.4f};
// the moon lights the sky as the sun does, this many times weaker at the full moon (illuminance 0.25 lux against 100 klux)
float const MOON_TO_SUN{2.5e-6f};
// average radiance of the sky of a moonless night (airglow, starlight, the zodiacal light: about 2e-4 cd/m2) in the units of
// the atmosphere (a sun of illuminance 1), where the clear day sky has about 0.03 (5000 cd/m2)
float const NIGHT_SKY_RADIANCE{1.3e-9f};
// colour of the night sky as the eye adapted to the dark sees it, luminance 1: grey blue
glm::vec3 const NIGHT_SKY_COLOR{glm::vec3{0.55f, 0.72f, 1.f} / 0.7043f};
// average over the upper hemisphere of the brightening towards the horizon in atmo_night_sky (atmosphere_common.glsl)
float const NIGHT_SKY_HORIZON_GAIN{1.917f};
// average radiance (luminance) of the clear sky of the atmosphere by the elevation of the sun, log10, every 2 degrees from
// -20 to 20, then 30, 45, 60 and 90 degrees (worked out with the same model on the cpu). the sky-view table is exposed by it
float const SKY_AVERAGE_LOG[]{-10.27f, -9.69f, -8.84f, -7.93f, -6.91f, -6.13f, -5.18f, -4.45f, -3.55f, -2.92f, -2.39f, -2.10f, -1.93f, -1.83f, -1.76f, -1.71f, -1.68f, -1.66f, -1.64f, -1.62f, -1.60f};
float const SKY_AVERAGE_LOG_HIGH[]{-1.60f, -1.55f, -1.51f, -1.49f, -1.47f};
float const SKY_AVERAGE_HIGH_ANGLES[]{20.f, 30.f, 45.f, 60.f, 90.f};
// the shading scales the sunlight and the ambient light by these (light_common.glsl), so the light the sun gives the
// ground, in the units of the ambient light, is the diffuse colour times their ratio
float const SHADING_SUN_TO_AMBIENT{1.5f / 0.65f};

// parameters of the earth's atmosphere after Hillaire 2020 (km, 1/km), at four wavelengths: 630, 560, 490 and 430 nm.
// rayleigh: 13.558e-3 at 550 nm, going with the wavelength to the power of -4.05; ozone: absorption cross sections of the
// Chappuis band (about 3.6, 3.9, 1.0 and 0.1 e-21 cm2) scaled as Hillaire's; the aerosols: the haze of 550 nm, going with
// the wavelength to the power of -1.3 (Angstrom exponent of continental aerosols)
glm::vec4 const RAYLEIGH_SCATTERING{7.82e-3f, 12.60e-3f, 21.64e-3f, 36.76e-3f};
float const RAYLEIGH_SCALE_HEIGHT{8.f};
glm::vec4 const AEROSOL_SPECTRUM{0.8380f, 0.9769f, 1.1621f, 1.3771f};
float const MIE_SCATTERING{3.996e-3f};
float const MIE_EXTINCTION{4.440e-3f};
float const MIE_SCALE_HEIGHT{1.2f};
float const MIE_ASYMMETRY{0.8f};
glm::vec4 const OZONE_ABSORPTION{2.05e-3f, 2.22e-3f, 0.57e-3f, 0.057e-3f};
// from the four wavelengths to linear rgb (rows), with the colour of the sun (a black body of 5778 K): fitted on the cpu by
// least squares to the CIE 1931 observer over spectra of sunlight through and scattered by the air, within 0.2 per cent
glm::vec4 const SPECTRAL_TO_R{1.12936f, -0.22651f, 0.25696f, -0.21065f};
glm::vec4 const SPECTRAL_TO_G{-0.05107f, 0.80810f, 0.38715f, -0.12398f};
glm::vec4 const SPECTRAL_TO_B{-0.01557f, -0.11982f, 0.42496f, 0.66002f};
float const OZONE_ALTITUDE{25.f};
float const OZONE_HALF_WIDTH{15.f};
float const BOTTOM_RADIUS{6360.f};
float const TOP_RADIUS{6460.f};

// the average radiance of the clear sky of the atmosphere with the sun (or the moon) the given degrees high
float sky_average_estimate(float const Elevation)
{
	float log10value;
	if (Elevation < -20.f)
		log10value = SKY_AVERAGE_LOG[0] - 0.29f * (-20.f - Elevation);
	else if (Elevation < 20.f)
	{
		auto const position{(Elevation + 20.f) / 2.f};
		auto const index{std::min(static_cast<int>(position), 19)};
		log10value = std::lerp(SKY_AVERAGE_LOG[index], SKY_AVERAGE_LOG[index + 1], position - static_cast<float>(index));
	}
	else
	{
		log10value = SKY_AVERAGE_LOG_HIGH[4];
		for (int i = 0; i < 4; ++i)
		{
			if (Elevation <= SKY_AVERAGE_HIGH_ANGLES[i + 1])
			{
				auto const t{(Elevation - SKY_AVERAGE_HIGH_ANGLES[i]) / (SKY_AVERAGE_HIGH_ANGLES[i + 1] - SKY_AVERAGE_HIGH_ANGLES[i])};
				log10value = std::lerp(SKY_AVERAGE_LOG_HIGH[i], SKY_AVERAGE_LOG_HIGH[i + 1], t);
				break;
			}
		}
	}
	return std::pow(10.f, log10value);
}

// the drawn brightness of a sky of the given average radiance (see SKY_ADAPTATION)
float sky_brightness(float const Radiance)
{
	if (Radiance >= SKY_ADAPTATION_START)
		return Radiance;
	return SKY_ADAPTATION_START * std::pow(std::max(Radiance, 1e-30f) / SKY_ADAPTATION_START, SKY_ADAPTATION);
}

} // namespace

namespace gfx
{

sky_light::sky_light()
{
	// a valid atmosphere for the sky shader from the start, the tables stay black until the first update
	m_atmosphere.rayleigh = RAYLEIGH_SCATTERING;
	m_atmosphere.mie_scattering = AEROSOL_SPECTRUM * MIE_SCATTERING;
	m_atmosphere.mie_extinction = AEROSOL_SPECTRUM * MIE_EXTINCTION;
	m_atmosphere.ozone = OZONE_ABSORPTION;
	m_atmosphere.heights = glm::vec4{RAYLEIGH_SCALE_HEIGHT, MIE_SCALE_HEIGHT, OZONE_ALTITUDE, OZONE_HALF_WIDTH};
	m_atmosphere.ground = glm::vec4{Global.gfx_ambient_groundalbedo, MIE_ASYMMETRY, 0.f, 0.f};
	m_atmosphere.spectral_r = SPECTRAL_TO_R;
	m_atmosphere.spectral_g = SPECTRAL_TO_G;
	m_atmosphere.spectral_b = SPECTRAL_TO_B;
	m_atmosphere.sun_direction = glm::vec4{0.f, 1.f, 0.f, 0.f};
	m_atmosphere.bottom_radius = BOTTOM_RADIUS;
	m_atmosphere.top_radius = TOP_RADIUS;
	m_atmosphere.observer_radius = BOTTOM_RADIUS + 0.2f;
	m_atmosphere.skyview_exposure = 1.f;
}

void sky_light::update_sky_scale(float const Sunelevation)
{
	// the sun's part of the sky: the read back average once there is one, the estimate until then
	auto const sun{m_result ? luminance(glm::vec3{m_result->average} * m_white_balance) : sky_average_estimate(Sunelevation)};
	// the moon's part: the day sky it would make at its height, as much weaker as its light is, gone when it sets
	auto const &moon{simulation::Environment.moon()};
	auto const moonelevation{moon.getAngle()};
	auto const moonup{glm::smoothstep(-3.f, 3.f, moonelevation)};
	m_moon_radiance = sky_average_estimate(std::max(moonelevation, 0.f)) * MOON_TO_SUN * moon.getBrightness() * moonup;
	auto const total{sun + m_moon_radiance + NIGHT_SKY_RADIANCE};
	auto const drawn{ATMOSPHERE_SKY_SCALE * Global.gfx_atmosphere_sky * sky_brightness(total)};
	m_sky_scale = drawn / total;
	m_sky_drawn_average = drawn;
	m_moon_direction = glm::normalize(moon.getDirection());
}

sky_light::sh_coefficients sky_light::dome_directional(CSkyDome const &Skydome, glm::vec3 const &Ambient, glm::vec3 const &Sunlight, float const Sunheight) const
{
	sh_coefficients radiance{};
	auto const &vertices{Skydome.vertices()};
	auto const &colors{Skydome.colors()};
	// the vertices make a grid even in the elevation and the azimuth, so the solid angle of a vertex goes with the cosine of
	// its elevation. the bottom ring lies a little under the horizon and is taken as the horizon
	float totalweight{0.f};
	for (std::size_t i = 0; i < vertices.size() && i < colors.size(); ++i)
	{
		auto direction{glm::normalize(vertices[i])};
		// the dome darkens its colours towards the top and brightens the band above the horizon for the looks
		// (CSkyDome::RebuildColors), which would make the zenith almost black and the horizon the main source of light.
		// both are undone here, so the light comes from the sky the Perez model describes
		auto const height{std::max(direction.y, 0.05f)};
		auto const horizonband{std::clamp(1.f - height / 0.2f, 0.f, 1.f)};
		auto const looks{std::clamp(1.f - height * 0.75f, 0.25f, 1.f) * std::lerp(1.f, 1.5f + 0.75f * m_overcast, horizonband)};
		direction.y = std::max(direction.y, 0.f);
		direction = glm::normalize(direction);
		auto const weight{std::sqrt(std::max(1.f - direction.y * direction.y, 0.f))};
		auto const color{sky_shoulder(glm::pow(glm::max(colors[i] / looks, glm::vec3{0.f}), glm::vec3{2.2f}))};
		auto const basis{sh_basis(direction)};
		for (std::size_t c = 0; c < radiance.size(); ++c)
			radiance[c] += color * (basis[c] * weight);
		totalweight += weight;
	}
	if (totalweight <= 0.f)
		return {};
	for (auto &coefficient : radiance)
		coefficient *= 2.f * PI / totalweight;

	// the ground, lit by the sky and the sun, fills the lower hemisphere with uniform light (see atmosphere_ambient.comp).
	// the dome is in its own units: the light of the sky on an upward facing surface is taken to be the ambient light,
	// which converts the sunlight to them
	auto const up{sh_basis(glm::vec3{0.f, 1.f, 0.f})};
	glm::vec3 skyup{0.f};
	for (std::size_t c = 0; c < radiance.size(); ++c)
		skyup += radiance[c] * (SH_IRRADIANCE_BAND[SH_BAND[c]] * up[c]);
	auto const tosky{luminance(skyup) / std::max(luminance(Ambient), 1e-4f)};
	auto const groundradiance{Global.gfx_ambient_groundalbedo * (skyup + Sunlight * SHADING_SUN_TO_AMBIENT * std::max(Sunheight, 0.f) * tosky)};
	radiance[0] += groundradiance * (SH_Y00 * 2.f * PI);
	radiance[1] += groundradiance * (SH_Y1 * -PI);

	// to the irradiance, scaled so its average over all directions is the ambient light, channel by channel
	sh_coefficients directional{};
	glm::vec3 const average{radiance[0] * SH_Y00};
	auto const scale{Ambient / glm::max(average, glm::vec3{1e-4f})};
	for (std::size_t c = 1; c < radiance.size(); ++c)
		directional[c] = radiance[c] * SH_IRRADIANCE_BAND[SH_BAND[c]] * scale;
	return directional;
}

void sky_light::begin_update()
{
	m_overcast = std::clamp(Global.Overcast, 0.f, 1.f);
	m_weight = 0.f;
	m_sky_weight = 0.f;
}

bool sky_light::atmosphere_changed(glm::vec3 const &Sundirection, bool &Medium)
{
	// aerosols grow with the turbidity the weather sets (3 clear .. 9 overcast)
	auto const turbidity{std::clamp(Global.fTurbidity, 3.f, 9.f)};
	auto const miescale{Global.gfx_atmosphere_haze * (1.f + 0.5f * (turbidity - 3.f))};
	glm::vec4 const medium{miescale, Global.gfx_ambient_groundalbedo, Global.gfx_atmosphere_altitude, 0.f};
	auto const mediumchanged{!m_tables_medium || *m_tables_medium != medium};
	// the sky-view table follows the sun when it moved by more than about a twentieth of a degree
	auto const sunchanged{!m_tables_sun || glm::dot(*m_tables_sun, Sundirection) < 0.9999995f};
	// the night sky and the moon go to the shaders with the parameters of the atmosphere
	auto const sunelevation{glm::degrees(std::asin(std::clamp(Sundirection.y, -1.f, 1.f)))};
	update_sky_scale(sunelevation);
	glm::vec4 const moon{m_moon_direction, m_sky_scale * m_moon_radiance};
	glm::vec4 const night{NIGHT_SKY_COLOR * (m_sky_scale * NIGHT_SKY_RADIANCE / NIGHT_SKY_HORIZON_GAIN), 0.f};
	// changed: brightness by more than a percent, the moon by more than about a fifth of a degree
	auto const brighter = [](float const A, float const B) {
		return std::abs(A - B) > std::max(std::abs(B) * 0.01f, 1e-12f);
	};
	auto const nightchanged{
	    brighter(moon.w, m_atmosphere.moon.w) || brighter(night.b, m_atmosphere.night.b) ||
	    ((moon.w > 0.f) && (glm::dot(glm::vec3{moon}, glm::vec3{m_atmosphere.moon}) < 0.999994f))};
	Medium = mediumchanged;
	if (!mediumchanged && !sunchanged && !nightchanged)
		return false;

	m_atmosphere.moon = moon;
	m_atmosphere.night = night;
	// the table holds the radiance times this, about 0.03 / the average of the sky: values the 16 bits keep well
	m_atmosphere.skyview_exposure = 0.03f / sky_average_estimate(sunelevation);

	m_atmosphere.mie_scattering = AEROSOL_SPECTRUM * (MIE_SCATTERING * miescale);
	m_atmosphere.mie_extinction = AEROSOL_SPECTRUM * (MIE_EXTINCTION * miescale);
	m_atmosphere.ground = glm::vec4{Global.gfx_ambient_groundalbedo, MIE_ASYMMETRY, 0.f, 0.f};
	m_atmosphere.sun_direction = glm::vec4{Sundirection, 0.f};
	m_atmosphere.observer_radius = BOTTOM_RADIUS + std::max(Global.gfx_atmosphere_altitude, 0.f) * 0.001f + 0.001f;
	m_tables_medium = medium;
	m_tables_sun = Sundirection;
	return true;
}

void sky_light::end_update(basic_light &Sunlight, glm::vec3 &Fogcolor, CSkyDome const &Skydome, glm::vec3 const &Sundirection, float const Sunangle)
{
	// the light the simulation set, which is kept where the atmosphere is not used
	glm::vec3 const oldambient{Sunlight.ambient};
	glm::vec3 const olddiffuse{Sunlight.diffuse};
	glm::vec3 const oldfog{Fogcolor};

	if (Global.gfx_atmosphere && m_result)
	{
		m_weight = glm::smoothstep(ATMOSPHERE_FADE_START, ATMOSPHERE_FADE_END, Sunangle);
		m_sky_weight = 1.f;
	}

	sh_coefficients directional{};
	if (Global.gfx_ambient_sh)
	{
		// the dome changes with the sun, the weather and the light the simulation works out from them
		std::array<glm::vec4, 3> const key{glm::vec4{Sundirection, Sunlight.direction.y}, glm::vec4{oldambient, m_overcast}, glm::vec4{olddiffuse, Global.gfx_ambient_groundalbedo}};
		if (!m_dome_key || *m_dome_key != key)
		{
			m_dome_sh = dome_directional(Skydome, oldambient, olddiffuse, -Sunlight.direction.y);
			m_dome_key = key;
		}
		directional = m_dome_sh;
	}

	m_sunlight = olddiffuse;
	m_ambient = oldambient;
	m_fog = oldfog;
	m_sun_disc_color = glm::vec3{1.f};
	// the sky is the atmosphere's whenever there is a result; the light goes over to it by the height of the sun (m_weight)
	if (m_sky_weight > 0.f)
	{
		auto const &result{*m_result};
		// the camera's white balance: the sun high in the sky is white
		auto const balance{1.f / glm::max(glm::vec3{result.zenith_transmittance}, glm::vec3{1e-3f})};
		m_white_balance = balance / std::max(luminance(balance), 1e-4f);

		// the atmosphere has no clouds: the overcast takes away the sunlight, greys the sky and evens out its light
		// with the shadows of the clouds the sun is held back where they are, not everywhere
		auto const overcastsun{m_cloud_shadows ? 1.f : (1.f - 0.65f * m_overcast)};
		auto const sun{glm::vec3{result.sun_transmittance} * m_white_balance * (ATMOSPHERE_SUN_SCALE * Global.gfx_atmosphere_sun) * overcastsun};
		auto const ambientscale{ATMOSPHERE_AMBIENT_SCALE * Global.gfx_atmosphere_ambient};
		auto const ambient{desaturate(glm::vec3{result.radiance_sh[0]} * SH_Y00 * m_white_balance * ambientscale, 0.5f * m_overcast)};
		// the brightness the sky is drawn with, the read back average of this frame
		update_sky_scale(glm::degrees(std::asin(std::clamp(Sundirection.y, -1.f, 1.f))));
		auto const fog{desaturate(glm::vec3{result.horizon} * m_white_balance * m_sky_scale, 0.5f * m_overcast)};
		m_sky_flat_luminance = m_sky_drawn_average;

		m_sunlight = glm::mix(olddiffuse, sun, m_weight);
		m_ambient = glm::mix(oldambient, ambient, m_weight);
		m_fog = glm::mix(oldfog, fog, m_weight);
		auto const sundisc{glm::vec3{result.sun_transmittance} * m_white_balance};
		m_sun_disc_color = sundisc / std::max({sundisc.r, sundisc.g, sundisc.b, 1e-4f});

		if (Global.gfx_ambient_sh)
		{
			auto const evenness{1.f - 0.75f * m_overcast};
			for (std::size_t c = 1; c < directional.size(); ++c)
			{
				auto const atmosphere{desaturate(glm::vec3{result.radiance_sh[c]} * m_white_balance * (SH_IRRADIANCE_BAND[SH_BAND[c]] * ambientscale * evenness), 0.5f * m_overcast)};
				directional[c] = glm::mix(directional[c], atmosphere, m_weight);
			}
		}

		Sunlight.diffuse = glm::vec4{m_sunlight, Sunlight.diffuse.a};
		Sunlight.ambient = glm::vec4{m_ambient, Sunlight.ambient.a};
		Fogcolor = m_fog;
	}

	// directional part of the ambient light for the shading
	gl::sky_data skydata{};
	for (std::size_t c = 1; c < directional.size(); ++c)
		skydata.ambient_sh[c] = glm::vec4{directional[c] * Global.gfx_ambient_sh_strength, 0.f};
	m_sky_data = skydata;
}

void sky_light::sky_params(glm::vec4 &Param0, glm::vec4 &Param1) const
{
	// the volumetric clouds make the overcast sky themselves
	Param0 = glm::vec4{m_sky_weight, lut_scale(), m_clouds ? 0.f : 0.75f * m_overcast, m_sky_flat_luminance};
	Param1 = glm::vec4{m_white_balance, 0.f};
}

glm::vec3 sky_light::sun_disc_color(glm::vec3 const &Oldcolor) const
{
	return glm::mix(Oldcolor, m_sun_disc_color, m_weight);
}

void sky_light::draw_debug_ui() const
{
	ImGui::Checkbox("directional ambient light (gfx.ambient.sh)", &Global.gfx_ambient_sh);
	ImGui::SliderFloat("directional strength", &Global.gfx_ambient_sh_strength, 0.f, 2.f);
	ImGui::SliderFloat("ground albedo", &Global.gfx_ambient_groundalbedo, 0.f, 1.f);
	ImGui::Checkbox("GGX prefiltered reflections (gfx.envmap.ggx)", &Global.gfx_envmap_ggx);
	ImGui::Checkbox("physical atmosphere (gfx.atmosphere)", &Global.gfx_atmosphere);
	ImGui::SliderFloat("sky", &Global.gfx_atmosphere_sky, 0.f, 4.f);
	ImGui::SliderFloat("ambient", &Global.gfx_atmosphere_ambient, 0.f, 4.f);
	ImGui::SliderFloat("sun", &Global.gfx_atmosphere_sun, 0.f, 4.f);
	ImGui::SliderFloat("haze", &Global.gfx_atmosphere_haze, 0.f, 8.f);
	ImGui::SliderFloat("altitude (m)", &Global.gfx_atmosphere_altitude, 0.f, 3000.f);
	ImGui::Text("atmosphere share of the light %.2f, of the sky %.2f%s", m_weight, m_sky_weight, (Global.gfx_atmosphere && !m_result) ? " (waiting for the first read back)" : "");
	ImGui::Text("sky drawn %.2e (scale %.3g), moonlit sky %.2e, table exposure %.3g", m_sky_drawn_average, m_sky_scale, m_moon_radiance * m_sky_scale, m_atmosphere.skyview_exposure);
	ImGui::Text("sun %.3f %.3f %.3f", m_sunlight.r, m_sunlight.g, m_sunlight.b);
	ImGui::Text("ambient %.3f %.3f %.3f", m_ambient.r, m_ambient.g, m_ambient.b);
	ImGui::Text("fog %.3f %.3f %.3f", m_fog.r, m_fog.g, m_fog.b);
	ImGui::Text("up / down ambient %.3f / %.3f", luminance(m_ambient + glm::vec3{m_sky_data.ambient_sh[1]} * SH_Y1), luminance(m_ambient - glm::vec3{m_sky_data.ambient_sh[1]} * SH_Y1));
}

} // namespace gfx
