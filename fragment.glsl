/*
 * MIT License
 * Copyright (c) 2020 Pablo Peñarroja
 */

/*
 * although the algorithms used here are physically based, many assumptions and
 * approximations have to be made to accomplish real-time cloud rendering.
 *
 * i've divided the fragment shader in two well-differentiated sections:
 *
 * ->rayleigh scattering:
 *   ->particles smaller than light wavelength
 *   ->responsible for skydome
 *
 * ->mie scattering:
 *   ->particles bigger than light wavelength (water molecules)
 *   ->responsible for clouds
 *   ->a precomputed random 3d texture is used as a cloud density map
 */

#version 430 core

layout(location = 0) in vec4 v_position;
layout(location = 0) out vec4 out_color;

// rendering constants
const float MIN_DIST = 0.01;
const float RAYMARCH_MIN_DIST = 1e-3;
const int RAYMARCH_MAX_STEP = 128;
const int MAX_STEP = 256;
const int MAX_DIST = 512;

// physical constants
const float PI = 3.14159265;
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

uniform int frame;
uniform int render_sky;
uniform float noise_depth;
uniform float noise_zoom;
uniform vec2 resolution;
uniform vec3 background_color;
uniform vec3 box_size;
uniform vec3 cloud_color;
uniform vec3 sun_direction;
uniform vec3 camera_location;
uniform mat4 view_matrix;

uniform int cloud_volume_samples;
uniform int cloud_in_scatter_samples;
uniform float cloud_darkness_threshold;
uniform float cloud_absorption;
uniform float cloud_density_threshold;
uniform float cloud_density_multiplier;
uniform float cloud_scale;
uniform vec3 cloud_location;
uniform vec3 cloud_volume;

uniform vec3 wind_direction;

uniform sampler3D noise_texture;

// ---- mie ---- declarations ---- //
float mie_density(vec3 position);
float beer_lambert(float x);
float henyey_greenstein(float x, float y);
float phase(float x);
float mie_in_scatter(vec3 position);
vec2 ray_volume_distance(vec3 origin, vec3 inverted_direction, vec3 vol_left_bound, vec3 vol_right_bound);
// ------------------------------- //

// ---- rayleigh ---- declarations ---- //
vec2 rayleigh_density(vec3 point);
float rayleigh_march(vec3 origin, vec3 direction, float radius);
vec3 rayleigh_scatter(vec3 direction, float l);
// ------------------------------------ //

void main() {

	// ---- ray direction ---- // 

	vec2 uv = gl_FragCoord.xy / resolution * 2.0 - 1.0;
	uv.x *= resolution.x / resolution.y;
	vec4 dir = vec4(normalize(vec3(uv, -2.0)), 1.0);
	dir = view_matrix * dir;

	// ---- rayleigh ---- //

	vec3 rayleigh_color = vec3(0.0);
	if (render_sky == 1) {
		float l = rayleigh_march(camera_location, dir.xyz, radius_atmosphere);
		rayleigh_color = rayleigh_scatter(dir.xyz, l);
	} else {
		rayleigh_color = background_color;
	}

	// ---- mie ---- //
	
	float transmittance = 1.0; // transparent
	vec3 light = vec3(0.0); // accumulated light
	
	vec2 march = ray_volume_distance(camera_location, -1.0 / dir.xyz, cloud_location - cloud_volume, cloud_location + cloud_volume);
	float distance_per_step = march.y / cloud_volume_samples;
	float distance_travelled = 0.0;

	// if ray hits cloud, compute lighting through it

	for (; distance_travelled < march.y; distance_travelled += distance_per_step) {
		vec3 ray_position = camera_location + dir.xyz * (march.x + distance_travelled);
		float density = mie_density(ray_position);
		if (density > 0.0) {
			float light_transmittance = mie_in_scatter(ray_position);
			light += density * distance_per_step * transmittance * light_transmittance;
			transmittance *= exp(-density * distance_per_step * cloud_absorption);
			if (transmittance < 0.001) {
				break;
			}
		}
	}

	vec3 color = (rayleigh_color * transmittance) + (light * cloud_color);

	// return fragment color
	out_color = vec4(color, 1.0);
}

// --------------------- //
// -------- mie -------- //
// --------------------- //

float mie_density(vec3 position) {
	vec4 texture_value = texture(noise_texture, position * noise_zoom);
	float density = max(0.0, texture_value.r - cloud_density_threshold) * cloud_density_multiplier;
	return density;
}

float beer_lambert(float x) {
	return exp(-x);
}

// approximation of a mie phase function
// -> due to mie scattering's complexity, 
//    an approximation is used.
// -> a cheaper alternative, if needed, 
//    would be the Schlik phase function,
//    which doesn't use pow()
float henyey_greenstein(float x, float y) {
	float y2 = y * y;
	return (1.0 - y2) / (4 * PI * pow(1 + y2 - 2 * y * x, 1.5));
}

// balanced blend
float phase(float x) {
	// parameters
	float a = 1.0;
	float b = 1.0;
	float c = 1.0;
	float d = 1.0;
	// blend between parametars
	float blend = 0.5;
	float hg_blend = henyey_greenstein(x, a) * (1.0 - blend) + henyey_greenstein(x, -b) * blend;
	return hg_blend * d + c;
}

float mie_in_scatter(vec3 position) {
	vec3 direction = -(sun_direction);
	vec3 lower_bound = cloud_location - cloud_volume;
	vec3 upper_bound = cloud_location + cloud_volume;
	float distance_inside_volume = ray_volume_distance(position, - 1.0 / direction, lower_bound, upper_bound).y;
	float step_size = distance_inside_volume / cloud_in_scatter_samples;
	position += direction * step_size * 0.5;
	float density = 0.0;
	for (int i = 0; i < cloud_in_scatter_samples; ++i) {
		density += max(0.0, mie_density(position) * step_size);
		position += direction * step_size;
	}
	float transmittance = beer_lambert(density * cloud_absorption);
	return cloud_darkness_threshold + transmittance * (1.0 - cloud_darkness_threshold);
}

// from
// http://jcgt.org/published/0007/03/04/
// returns float2:
// 	x -> distance to cloud volume
// 	y -> distance across cloud volume
vec2 ray_volume_distance(vec3 origin, vec3 inverted_direction, vec3 vol_left_bound, vec3 vol_right_bound) {
	vec3 t0 = (vol_left_bound - camera_location) * inverted_direction;
	vec3 t1 = (vol_right_bound - camera_location) * inverted_direction;
	vec3 tmin = min(t0, t1);
	vec3 tmax = max(t0, t1);
	float dist_maxmin = max(max(tmin.x, tmin.y), tmin.z);
	float dist_minmax = min(tmax.x, min(tmax.y, tmax.z));
	float dist_to_volume = max(0.0, dist_maxmin);
	float dist_across_volume = max(0.0, dist_minmax - dist_to_volume);
	return vec2(dist_to_volume, dist_across_volume);
}

// -------------------------- //
// -------- rayleigh -------- //
// -------------------------- //

vec2 rayleigh_density(vec3 point) {
	float h = max(0.0, length(point - earth_center) - radius_surface);
	return vec2(exp(-h / 8e3), exp(-h / 12e2));
}

float rayleigh_march(vec3 origin, vec3 direction, float radius) {
	// origin - earth center
	vec3 v = origin - earth_center;
	float b = dot(v, direction);
	float d = b * b - dot(v, v) + radius * radius;
	if (d < 0.) return -1.;
	d = sqrt(d);
	float r1 = -b - d, r2 = -b + d;
	return (r1 >= 0.) ? r1 : r2;
}

vec3 rayleigh_scatter(vec3 direction, float l) {
	// scatter in
	vec2 total_depth = vec2(0.0);
	vec3 intensity_rayleigh = vec3(0.0);
	vec3 intensity_mie = vec3(0.0);
	{
		float l_sin = l / SCATTER_IN_STEP;
		vec3 direction_sin = direction * l_sin;

		// iterate point
		for (int i = 0; i < SCATTER_IN_STEP; ++i) {
			vec3 point = camera_location + direction_sin * i;		
			vec2 depth = rayleigh_density(point) * l_sin;
			total_depth += depth;

			// calculate scatter depth
			float l_sde = rayleigh_march(point, sun_direction, radius_atmosphere);
			vec2 depth_accumulation = vec2(0.0);
			{
				l_sde /= SCATTER_DEPTH_STEP;
				vec3 direction_sde = sun_direction * l_sde;
				// iterate point
				for (int j = 0; j < SCATTER_DEPTH_STEP; ++j) {
					depth_accumulation += rayleigh_density(point + direction_sde * j);
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
		//lo * exp(-rayleigh_coefficient * total_depth.x - mie_coefficient_upper * total_depth.y) // cancels to zero because of color == vec3(0.0)
		//+
		sun_intensity * (1.0 + mu * mu)
		*
		(intensity_rayleigh * rayleigh_coefficient * 0.0597 + intensity_mie * mie_coefficient_lower * 0.0196 / pow(1.58 - 1.52 * mu, 1.5)
	));
}
