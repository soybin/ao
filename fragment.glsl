/*
 * MIT License
 * Copyright (c) 2020 Pablo Peñarroja
 */

/*
 * this fragment shader is differentiated into two
 * sections; although both parts work to accomplish
 * the same goal-atmospheric scattering-the level
 * of detail required for each section greatly
 * differs, thus a distinction is required for it
 * to work in real-time.
 *
 * ->atmosphere rendering:
 *   ->responsible for computing atmospheric
 *     scattering without accounting for any
 *     light-path modifier other than air
 *     itself with its common particles.
 *   ->what you would see on a clear sunny
 *     day.
 *
 * ->cloud rendering:
 *   ->renders scattering due to clouds-regions
 *     where crystalls of water scatter light-in
 *     the atmosphere.
 *   ->precomputed 3d worley-perlin noise textures
 *     are used to compute the scattering within 
 *     a specific region-cloud_location and
 *     cloud_volume-due to cloud interference
 *     along the view ray.
 *
 * the two alorithms share very few variables, so
 * they're mostly independent.
 */

#version 430 core

layout(location = 0) in vec4 v_position;
layout(location = 0) out vec4 out_color;

const float PI = 3.14159265;

// planet's atmosphere constants-earth by default
const float SCATTER_IN_STEP = 16.0;
const float SCATTER_DEPTH_STEP = 4.0;
const float radius_surface = 6360e3;
const float radius_atmosphere = 6380e3;
const float sun_intensity = 8.2;
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
uniform float cloud_shadowing_threshold;
uniform float cloud_absorption;
uniform float cloud_density_threshold;
uniform float cloud_density_multiplier;
uniform float cloud_noise_main_scale;
uniform float cloud_noise_detail_scale;
uniform float cloud_noise_detail_weight;
uniform vec3 cloud_location;
uniform vec3 cloud_volume;
uniform vec3 cloud_directional_light;

uniform vec3 wind_direction;

uniform sampler3D main_noise_texture;
uniform sampler3D detail_noise_texture;

// ---- clouds ---- declarations ---- //
float mie_density(vec3 position);
float beer_lambert(float x);
float henyey_greenstein(float x, float y);
float phase(float x);
float mie_in_scatter(vec3 position);
vec2 ray_to_cloud(vec3 origin, vec3 inverted_direction, vec3 vol_left_bound, vec3 vol_right_bound);
// ------------------------------- //

// ---- atmosphere ---- declarations ---- //
vec2 atmosphere_density(vec3 point);
float atmosphere_march(vec3 origin, vec3 direction, float radius);
vec3 atmosphere_scatter(vec3 direction, float l);
// ------------------------------------ //

void main() {

	// ---- ray direction ---- // 

	vec2 uv = gl_FragCoord.xy / resolution * 2.0 - 1.0;
	uv.x *= resolution.x / resolution.y;
	vec4 dir = vec4(normalize(vec3(uv, -2.0)), 1.0);
	dir = view_matrix * dir;

	// ---- rayleigh ---- //

	vec3 atmosphere_color = vec3(0.0);
	if (render_sky == 1) {
		float l = atmosphere_march(camera_location, dir.xyz, radius_atmosphere);
		atmosphere_color = atmosphere_scatter(dir.xyz, l);
	} else {
		atmosphere_color = background_color;
	}

	// ---- mie ---- //
	
	float transmittance = 1.0; // transparent
	vec3 light = vec3(0.0); // accumulated light
	
	vec2 march = ray_to_cloud(camera_location, 1.0 / dir.xyz, cloud_location - cloud_volume, cloud_location + cloud_volume);
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

	vec3 color = (atmosphere_color * transmittance) + (light * cloud_color);

	// return fragment color
	out_color = vec4(color, 1.0);
}

// --------------------- //
// -------- mie -------- //
// --------------------- //

float mie_density(vec3 position) {
	float time = frame / 1000.0;
	vec3 lower_bound = cloud_location - cloud_volume;
	vec3 upper_bound = cloud_location + cloud_volume;
	vec3 uvw = position * cloud_noise_main_scale;
	vec3 main_sample_location = uvw + wind_direction * time;

	const float container_fade_distance = 100.0;
	float distance_edge_x = min(container_fade_distance, min(position.x - lower_bound.x, lower_bound.x - position.x));
	float distance_edge_z = min(container_fade_distance, min(position.z - lower_bound.z, lower_bound.z - position.z));
	float edge_weight = min(distance_edge_x, distance_edge_z) / container_fade_distance;

	float main_noise_fbm = texture(main_noise_texture, main_sample_location).r;
	float density = max(0.0, main_noise_fbm - cloud_density_threshold);

	if (density > 0.0) {
		vec3 detail_sample_position = uvw * cloud_noise_detail_scale;
		float detail_noise_fbm = texture(detail_noise_texture, detail_sample_position).r;
		density -= detail_noise_fbm * cloud_noise_detail_weight;
		return density * cloud_density_multiplier;
	}

	return 0.0;
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
	// blend between parameters
	float blend = 0.5;
	float hg_blend = henyey_greenstein(x, a) * (1.0 - blend) + henyey_greenstein(x, -b) * blend;
	return hg_blend * d + c;
}

float mie_in_scatter(vec3 position) {
	vec3 direction = normalize(sun_direction);
	float distance_inside_volume = ray_to_cloud(position, 1 / direction, cloud_location - cloud_volume, cloud_location + cloud_volume).y;
	float step_size = distance_inside_volume / cloud_in_scatter_samples;
	position += direction * step_size * 0.5;
	float density = 0.0;
	for (int i = 0; i < cloud_in_scatter_samples; ++i) {
		density += max(0.0, mie_density(position)) * step_size;
		position += direction * step_size;
	}
	float transmittance = beer_lambert(density * cloud_absorption);
	return cloud_shadowing_threshold + transmittance * (1.0 - cloud_shadowing_threshold);
}

// from
// http://jcgt.org/published/0007/03/04/
// returns float2:
// 	x -> distance to cloud volume
// 	y -> distance across cloud volume
vec2 ray_to_cloud(vec3 origin, vec3 inverted_direction, vec3 vol_left_bound, vec3 vol_right_bound) {
	vec3 t0 = (vol_left_bound - origin) * inverted_direction;
	vec3 t1 = (vol_right_bound - origin) * inverted_direction;
	vec3 tmin = min(t0, t1);
	vec3 tmax = max(t0, t1);
	float dist_maxmin = max(max(tmin.x, tmin.y), tmin.z);
	float dist_minmax = min(tmax.x, min(tmax.y, tmax.z));
	float dist_to_volume = max(0.0, dist_maxmin);
	float dist_across_volume = max(0.0, dist_minmax - dist_to_volume);
	return vec2(dist_to_volume, dist_across_volume);
}

// ---------------------------- //
// -------- atmosphere -------- //
// ---------------------------- //

vec2 atmosphere_density(vec3 point) {
	float h = max(0.0, length(point - earth_center) - radius_surface);
	return vec2(exp(-h / 8e3), exp(-h / 12e2));
}

float atmosphere_march(vec3 origin, vec3 direction, float radius) {
	// origin - earth center
	vec3 v = origin - earth_center;
	float b = dot(v, direction);
	float d = b * b - dot(v, v) + radius * radius;
	if (d < 0.) return -1.;
	d = sqrt(d);
	float r1 = -b - d, r2 = -b + d;
	return (r1 >= 0.) ? r1 : r2;
}

vec3 atmosphere_scatter(vec3 direction, float l) {
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
			vec2 depth = atmosphere_density(point) * l_sin;
			total_depth += depth;

			// calculate scatter depth
			float l_sde = atmosphere_march(point, sun_direction, radius_atmosphere);
			vec2 depth_accumulation = vec2(0.0);
			{
				l_sde /= SCATTER_DEPTH_STEP;
				vec3 direction_sde = sun_direction * l_sde;
				// iterate point
				for (int j = 0; j < SCATTER_DEPTH_STEP; ++j) {
					depth_accumulation += atmosphere_density(point + direction_sde * j);
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

	return sqrt(sun_intensity * (1.0 + mu * mu) * (intensity_rayleigh * rayleigh_coefficient * 0.0597 + intensity_mie * mie_coefficient_lower * 0.0196 / pow(1.58 - 1.52 * mu, 1.5)));
}
