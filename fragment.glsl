// beautiful paper which inspired this
// http://advances.realtimerendering.com/s2015/The%20Real-time%20Volumetric%20Cloudscapes%20of%20Horizon%20-%20Zero%20Dawn%20-%20ARTR.pdf

#version 430 core

layout(location = 0) in vec4 v_position;
layout(location = 0) out vec4 color;

const float PI = 3.14159265;
const float MIN_DIST = 0.01;
const float RAYMARCH_MIN_DIST = 1e-3;
const int RAYMARCH_MAX_STEP = 128;
const int MAX_STEP = 256;
const int MAX_DIST = 512;

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

uniform int samples_per_ray = 3;
uniform float density_threshold;
uniform float cloud_scale;
uniform vec3 cloud_location;
uniform vec3 cloud_volume;

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

// ---- clouds ---- declarations ---- //
float render_cloud_volumes(vec3 direction);
// ---------------------------------- //

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
	col += render_cloud_volumes(D.xyz);
	color = vec4(col, 1.0);
}

// ------------------------ //
// -------- clouds -------- //
// ------------------------ //

float sample_cloud_density(vec3 position) {
	position = position / 1e3;
	vec4 texture_value = texture(noise_texture, position);
	float density = max(0.0, texture_value.r - density_threshold);
	return density;
}

// signed distance function from:
// https://iquilezles.org/www/articles/distfunctions/distfunctions.htm
float cloud_sdf(vec3 point) {
	vec3 q = abs(point + cloud_location) - cloud_volume;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

// remap value with range to 0.0 - 1.0 range
float remap11(float x, float low, float high) {
	return (x - low) / (high - low);
}

float beer(float x) {
	return exp(-x);
}

float henyey_greenstein(float x, float y) {
	float y2 = y * y;
	return (1.0 - y2) / (4 * PI * pow(1 + y2 - 2 * y * x, 1.5));
}

float phase(float x) {
	// parameters
	float a = 1.0;
	float b = 1.0;
	float c = 1.0;
	float d = 1.0;
	// blend between parameters
	float blend = 0.5;
	float hg_blend = henyey_greenstein(x, a) * (1.0 - blend) + henyey_greenstein(x, -b) * blend;
	return hg_blend * d + c;
}

// from
// http://jcgt.org/published/0007/03/04/
// returns float2:
// 	x -> distance to cloud volume
// 	y -> cloud volume cross section
vec2 ray_volume_distance(vec3 direction, vec3 vol_left_bound, vec3 vol_right_bound) {
	vec3 inverted_direction = vec3(1.0) / (-direction);
	vec3 t0 = (vol_left_bound - camera_location) * inverted_direction;
	vec3 t1 = (vol_right_bound - camera_location) * inverted_direction;
	vec3 tmin = min(t0, t1);
	vec3 tmax = max(t0, t1);
	float dist_maxmin = max(max(tmin.x, tmin.y), tmin.z);
	float dist_minmax = min(tmax.x, min(tmax.y, tmax.z));
	float dist_to_volume = max(0.0, dist_maxmin);
	float dist_cross_section = max(0.0, dist_minmax - dist_to_volume);
	return vec2(dist_to_volume, dist_cross_section);
}

float render_cloud_volumes(vec3 direction) {
	// for volume in volumes
	vec2 march = ray_volume_distance(direction, cloud_location - cloud_volume, cloud_location + cloud_volume);
	if (march.y == 0.0) return 0.0;
	float distance_per_sample = march.y / samples_per_ray;
	float density = 0.0;
	for (int i = 0; i < samples_per_ray; ++i) {
		density += sample_cloud_density(vec3(direction * distance_per_sample * i * zoom));
	}
	return density / samples_per_ray;
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
