// fog between the camera and a fragment, see fog_volume.glsl and weather_common.glsl. a draw with a fog_density of 0 is not
// fogged; one with another density than the regular one (the glare of lamps) gets the optical depth scaled by the ratio
#include "fog_volume.glsl"

// rgb: light the fog adds, a: what it lets through
vec4 fog_at_fragment()
{
	if (fog_density <= 0.0)
		return vec4(0.0, 0.0, 0.0, 1.0);
	vec4 fog = fog_along_view(f_pos.xyz, gl_FragCoord.xy);
	float scale = fog_density / max(fog_light.w, 1e-12);
	if (abs(scale - 1.0) > 1e-3)
	{
		float transmittance = exp(log(max(fog.a, 1e-8)) * scale);
		fog.rgb *= (1.0 - transmittance) / max(1.0 - fog.a, 1e-4);
		fog.a = transmittance;
	}
	return fog;
}

vec3 get_fog_color()
{
	vec4 fog = fog_at_fragment();
	return fog.rgb / max(1.0 - fog.a, 1e-4);
}

float get_fog_amount()
{
	return 1.0 - fog_at_fragment().a;
}

// the air between the camera and a fragment (aerial perspective): the molecules of the air scatter the blue light the most, so
// the far land fades into the colour of the sky at the horizon and turns blue, more the farther it is, while the light coming
// from it is reddened. for a uniform layer of air the light of an infinitely long path is the light of the sky at the horizon
// (the colours of the far fog, fog_color_world), so a path of transmittance T adds that times 1 - T, channel by channel. the
// haze (aerosols) is the fog's (fog_haze), this is the air alone: rayleigh scattering at sea level, scale height 8 km. done
// where the sky is the atmosphere's (fog_volume_target.w: share of the old sky)
const vec3 AIR_RAYLEIGH = vec3(5.802e-6, 13.558e-6, 33.1e-6);
const float AIR_SCALE_HEIGHT = 8000.0;

vec3 apply_air(vec3 color, vec3 view)
{
	float share = 1.0 - fog_volume_target.w;
	float distance = length(view);
	if (share <= 0.0 || distance <= 1.0)
		return color;
	vec3 direction = mat3(inv_view) * (view / distance);
	// the path through the air reduced to its density at the camera: the air thins upwards
	float rise = direction.y * distance / AIR_SCALE_HEIGHT;
	float path = distance * ((abs(rise) > 1e-3) ? (1.0 - exp(-rise)) / rise : 1.0 - 0.5 * rise);
	vec3 transmittance = exp(-AIR_RAYLEIGH * (path * share));
	return color * transmittance + fog_color_world(direction) * (vec3(1.0) - transmittance);
}

vec3 apply_fog(vec3 color)
{
	vec4 fog = fog_at_fragment();
	if (fog_density > 0.0)
		color = apply_air(color, f_pos.xyz);
	return color * fog.a + fog.rgb;
}

vec3 add_fog(vec3 color, float amount)
{
	return color + fog_at_fragment().rgb * amount;
}
