// the dust on the poles and the fittings of the catenary (world/catenary_dust.h): brown copper dust from the wires and the
// track, and general dirt, laid by the place in the world, not by the texture coordinates. its patches come from a noise of
// the world position, so a pole and a fitting bolted to it get one layer which goes on from one to the other. how much of it:
// by the way the surface faces (most on the top, less on the sides, almost none underneath) and by the height above the rails
// of the pole the part stands on (the most on the foundation and the bottom of the pole). the materials which take it have
// param_dust (a multiplier; not given - 1 for the parts of the catenary, which know the rails of their pole, none for the
// rest; negative - none) and the input f_dust_rail of vertex.vert (instance_dust_rail() in common.glsl).
// the settings are in the scene block (dust_*, gl::scene_ubs, data/traction_dust.txt).
// the same in shaders/ogl46 and shaders/vk14
#ifndef DUST_GLSL
#define DUST_GLSL

// period of the noise in the world, metres: the camera position dust_origin is wrapped to it (catenary_dust::kPeriod), the
// noise repeats with it, so the wrap makes no jump. the cells of the octaves (4, 1 and 0.25 m) fit in it whole
const float DUST_PERIOD = 4096.0;

uint dust_hash(uvec3 v)
{
	v = v * 1664525u + 1013904223u;
	v.x += v.y * v.z;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	v ^= v >> 16u;
	v.x += v.y * v.z;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	return v.x ^ v.y ^ v.z;
}

// value 0-1 at a corner of the lattice, the corner wrapped to Period cells
float dust_corner(vec3 corner, float period)
{
	return float(dust_hash(uvec3(mod(corner, period))) & 0xffffu) * (1.0 / 65535.0);
}

// value noise 0-1 repeating every Period cells
float dust_value_noise(vec3 p, float period)
{
	vec3 cell = floor(p);
	vec3 f = p - cell;
	vec3 u = f * f * (3.0 - 2.0 * f);
	float c000 = dust_corner(cell, period);
	float c100 = dust_corner(cell + vec3(1.0, 0.0, 0.0), period);
	float c010 = dust_corner(cell + vec3(0.0, 1.0, 0.0), period);
	float c110 = dust_corner(cell + vec3(1.0, 1.0, 0.0), period);
	float c001 = dust_corner(cell + vec3(0.0, 0.0, 1.0), period);
	float c101 = dust_corner(cell + vec3(1.0, 0.0, 1.0), period);
	float c011 = dust_corner(cell + vec3(0.0, 1.0, 1.0), period);
	float c111 = dust_corner(cell + vec3(1.0, 1.0, 1.0), period);
	return mix(mix(mix(c000, c100, u.x), mix(c010, c110, u.x), u.y), mix(mix(c001, c101, u.x), mix(c011, c111, u.x), u.y), u.z);
}

// distances (metres from the camera) over which the finer octaves fade to their mean and then aren't computed at all: further
// away their cells are below a pixel anyway, and the noise is most of the cost of the dust (8 hashes per octave)
const vec2 DUST_FINE_FADE = vec2(25.0, 45.0);     // the 0.25 m cells
const vec2 DUST_MEDIUM_FADE = vec2(90.0, 160.0);  // the 1 m cells

// patches of the dust, -1 to 1, at a world position (relative to the start of the period), Range - distance from the camera
float dust_patches(vec3 world, float range)
{
	float noise = 0.5 * dust_value_noise(world * 0.25, DUST_PERIOD * 0.25);
	float medium = 0.5;
	if (range < DUST_MEDIUM_FADE.y)
		medium = mix(dust_value_noise(world, DUST_PERIOD), 0.5, smoothstep(DUST_MEDIUM_FADE.x, DUST_MEDIUM_FADE.y, range));
	float fine = 0.5;
	if (range < DUST_FINE_FADE.y)
		fine = mix(dust_value_noise(world * 4.0, DUST_PERIOD * 4.0), 0.5, smoothstep(DUST_FINE_FADE.x, DUST_FINE_FADE.y, range));
	noise += 0.3 * medium + 0.2 * fine;
	// the sum keeps close to the middle: stretched to the whole range
	return clamp((noise - 0.5) * 2.5, -1.0, 1.0);
}

// amount of dust by the height above the rails: the profile of eight points (height, amount), linear between them
float dust_profile_at(float height)
{
	vec2 points[8] = vec2[8](dust_profile[0].xy, dust_profile[0].zw, dust_profile[1].xy, dust_profile[1].zw,
		dust_profile[2].xy, dust_profile[2].zw, dust_profile[3].xy, dust_profile[3].zw);
	if (height <= points[0].x)
		return points[0].y;
	for (int index = 1; index < 8; ++index)
	{
		if (height <= points[index].x)
		{
			float span = points[index].x - points[index - 1].x;
			float share = (span > 1e-4 ? (height - points[index - 1].x) / span : 1.0);
			return mix(points[index - 1].y, points[index].y, share);
		}
	}
	return points[7].y;
}

// amount of dust 0-1 at a fragment: View its position in view space (f_pos), Normal its normal in view space, Rail the rails of
// the instance (f_dust_rail), Scale the param_dust of the material
float dust_amount(vec3 view, vec3 normal, vec2 rail, float scale)
{
	// param_dust not given: the parts of the catenary, which know the rails of their pole, take it whole
	if (scale == 0.0)
		scale = (rail.y > 0.5 ? 1.0 : 0.0);
	if ((dust_origin.w < 0.5) || (scale <= 0.0))
		return 0.0;
	// world axes around the camera
	vec3 world = mat3(inv_view) * view;
	vec3 up = normalize(mat3(inv_view) * normal);
	float height = (rail.y > 0.5 ? world.y - rail.x : dust_params.w);
	float facing = (up.y >= 0.0 ? mix(dust_params.x, 1.0, up.y) : mix(dust_params.x, dust_params.y, -up.y));
	float amount = dust_color.a * scale * facing * dust_profile_at(height);
	if (amount <= 0.0)
		return 0.0;
	float patches = (dust_params.z > 0.0 ? dust_patches(dust_origin.xyz + world, length(view)) : 0.0);
	return clamp(amount * (1.0 + dust_params.z * patches), 0.0, 1.0);
}

#endif
