// beautiful paper which inspired this
// http://advances.realtimerendering.com/s2015/The%20Real-time%20Volumetric%20Cloudscapes%20of%20Horizon%20-%20Zero%20Dawn%20-%20ARTR.pdf

#version 430 core

layout(location = 0) in vec4 v_position;
layout(location = 0) out vec4 color;

const float PI = 3.14159265;
const float MIN_DIST = 0.01;
const int MAX_DIST = 512;
const int MAX_STEP = 256;

const float SCATTER_IN_STEP = 16.0;
const float SCATTER_DEPTH_STEP = 4.0;
const float radius_surface = 6360e3;
const float radius_atmosphere = 6380e3;
const float sun_intensity = 10.0;
const vec3 rayleigh_coefficient = vec3(58e-7, 135e-7, 331e-7);
const vec3 mie_coefficient_upper = vec3(2e-5);
const vec3 mie_coefficient_lower = mie_coefficient_upper * 1.1;
const vec3 earth_center = vec3(0.0, -radius_surface, 0.0);

// ---------------------------- //
// -------- parameters -------- //
// ---------------------------- //

uniform int frames;
uniform float noise_depth;
uniform float zoom;
uniform vec2 resolution;
uniform vec3 box_size;
uniform vec3 sun_direction;
uniform vec3 camera_location;
uniform mat4 view_matrix;

uniform sampler3D noise_texture;

// ----------------------- //
// -------- noise -------- //
// ----------------------- //

vec3 hash33(vec3 point) {
	point = vec3(
		dot(point,vec3(127.1,311.7, 74.7)),
		dot(point,vec3(269.5,183.3,246.1)),
		dot(point,vec3(113.5,271.9,124.6))
	);
	return fract(sin(point)*43758.5453123);
}

vec2 hash22(vec2 point) {
 	return fract(cos(point*mat2(-64.2,71.3,81.4,-29.8))*8321.3); 
}

float hash12(vec2 point) {
	return fract(sin(dot(point, vec2(12.9898, 78.233))) * 43758.5453123);
}

float voronoi(vec2 point) {
	float separation = 1.0;
	vec2 integer = floor(point);
	vec2 fractional = fract(point);
	for(int x = -1; x < 2; ++x){
		for(int y = -1; y < 2; ++y) {
			float s = distance(hash22(integer + vec2(x,y)) + vec2(x,y), fractional);
			separation = min(separation, s);
		}
	}
	return separation;
}

float perlin(vec2 point) {
	vec2 integer = floor(point);
	vec2 fractional = fract(point);

	float a = hash12(integer);
	float b = hash12(integer + vec2(1.0, 0.0));
	float c = hash12(integer + vec2(0.0, 1.0));
	float d = hash12(integer + vec2(1.0, 1.0));

	// interp
	vec2 u = fractional * fractional * (3.0 - 2.0 * fractional);

	// mix up
	return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

// ---- skybox ---- declarations ---- //
vec2 density(vec3 point);
float atmosphere_march(vec3 point, vec3 direction, float radius);
vec3 scatter(vec3 origin, vec3 direction, float l, vec3 lo);
// ---------------------------------- //

void main() {
	// normalize from -1.0 to 1.0
	vec2 uv = gl_FragCoord.xy / resolution * 2.0 - 1.0;
	uv.x *= resolution.x / resolution.y;
	vec3 O = camera_location;
	vec4 D = vec4(normalize(vec3(uv, -2.0)), 1.0);
	D = view_matrix * D;
	vec3 col = vec3(0.0);
	float L = atmosphere_march(O, vec3(D.x, D.y, D.z), radius_atmosphere);
	col = scatter(O, vec3(D.x, D.y, D.z), L, col);
	//color = vec4(texture(noise_texture, vec3(gl_FragCoord.xy / resolution.xx * zoom, noise_depth)).rgb + vec3(noise_depth), 1.0);
	color = vec4(texture(noise_texture, vec3(gl_FragCoord.xy / resolution.xx, noise_depth)).rrr, 1.0);
}

// ------------------------ //
// -------- skybox -------- //
// ------------------------ //

vec2 density(vec3 point) {
	float h = max(0.0, length(point - earth_center) - radius_surface);
	return vec2(exp(-h / 8e3), exp(-h / 12e2));
}

float atmosphere_march(vec3 point, vec3 direction, float radius) {
    vec3 v = point - earth_center;
    float b = dot(v, direction);
    float d = b * b - dot(v, v) + radius * radius;
    if (d < 0.) return -1.;
    d = sqrt(d);
    float r1 = -b - d, r2 = -b + d;
    return (r1 >= 0.) ? r1 : r2;
}

vec3 scatter(vec3 origin, vec3 direction, float l, vec3 lo) {
	// scatter in
	vec2 total_depth = vec2(0.0);
	vec3 intensity_rayleigh = vec3(0.0);
	vec3 intensity_mie = vec3(0.0);
	{
		float l_sin = l / SCATTER_IN_STEP;
		vec3 direction_sin = direction * l_sin;

		// iterate point
		for (int i = 0; i < SCATTER_IN_STEP; ++i) {
			vec3 point = origin + direction_sin * i;		
			vec2 depth = density(point) * l_sin;
			total_depth += depth;

			// calculate scatter depth
			float l_sde = atmosphere_march(point, sun_direction, radius_atmosphere);
			vec2 depth_accumulation = vec2(0.0);
			{
				l_sde /= SCATTER_DEPTH_STEP;
				vec3 direction_sde = sun_direction * l_sde;
				// iterate point
				for (int j = 0; j < SCATTER_DEPTH_STEP; ++j) {
					depth_accumulation += density(point + direction_sde * j);
				}
				depth_accumulation *= l_sde;
			}

			vec2 depth_sum = total_depth + depth_accumulation;

			vec3 a = exp(-rayleigh_coefficient * depth_sum.x - mie_coefficient_upper * depth_sum.y);

			intensity_rayleigh += a * depth.x;
			intensity_mie += a * depth.y;
		}
	}

	float mu = dot(direction, normalize(sun_direction));

	// lo extinction
	return sqrt(
		lo * exp(-rayleigh_coefficient * total_depth.x - mie_coefficient_upper * total_depth.y)
		+
		sun_intensity * (1.0 + mu * mu)
		*
		(intensity_rayleigh * rayleigh_coefficient * 0.0597 + intensity_mie * mie_coefficient_lower * 0.0196 / pow(1.58 - 1.52 * mu, 1.5)
	));
}
