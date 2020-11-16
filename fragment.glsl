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
uniform vec3 light_direction;
uniform vec3 inverse_light_direction;
uniform vec3 camera_location;
uniform mat4 view_matrix;

// cloud
uniform float cloud_absorption;
uniform float cloud_density_threshold;
uniform float cloud_density_multiplier;
uniform float cloud_volume_edge_fade_distance;
uniform vec3 cloud_location;
uniform vec3 cloud_volume;

// render
uniform int render_volume_samples;
uniform int render_in_scatter_samples;
uniform float render_shadowing_max_distance;
uniform float render_shadowing_weight;

// noise
uniform float noise_main_scale;
uniform vec3 noise_main_offset;
uniform float noise_weather_scale;
uniform float noise_weather_weight;
uniform vec2 noise_weather_offset;
uniform float noise_detail_scale;
uniform float noise_detail_weight;
uniform vec3 noise_detail_offset;
uniform sampler3D noise_main_texture;
uniform sampler2D noise_weather_texture;
uniform sampler3D noise_detail_texture;

uniform vec3 wind_vector;
uniform float wind_main_weight;
uniform float wind_weather_weight;
uniform float wind_detail_weight;

float remap(float value, float old_low, float old_high, float new_low, float new_high) {
	return new_low + (value - old_low) * (new_high - new_low) / (old_high - old_low);
}

// ---- clouds ---- declarations ---- //
float mie_density(vec3 position);
float beer_powder(float x);
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
	vec3 color_cloud = vec3(0.0); // accumulated light
	
	vec2 march = ray_to_cloud(camera_location, 1.0 / dir.xyz, cloud_location - cloud_volume, cloud_location + cloud_volume);
	float distance_per_step = march.y / render_volume_samples;
	float distance_travelled = 0.0;

	// henyey greenstein phase function
	// value for this ray's direction.
	// -> provides silver lining when
	//    looking towards sun.
	float hg_constant = henyey_greenstein(0.2, dot(dir.xyz, light_direction));

	// if ray hits cloud, compute amount of
	// light that reaches the cloud's surface

	for (; distance_travelled < march.y; distance_travelled += distance_per_step) {
		vec3 ray_position = camera_location + dir.xyz * (march.x + distance_travelled);
		// sample noise density at current
		// ray position.
		float density = mie_density(ray_position);
		// extinguish transmittance using
		// beer's law -> (e^(-d*deltaX)).
		transmittance *= exp(-density * distance_per_step);
		// avoid doing extra loops if it's
		// already dark.
		if (transmittance < 0.01) break;
		// amount of light in-scattered to 
		// this point in the cloud;
		// extinction coefficient when going
		// through the volume toward the sun.
		float in_light = mie_in_scatter(ray_position);
		// add to cloud's surface color
		color_cloud += density * distance_per_step * in_light * transmittance * hg_constant;
	}

	// return fragment color
	out_color = vec4((atmosphere_color * transmittance) + color_cloud, 1.0);
}

// --------------------- //
// -------- mie -------- //
// --------------------- //

float mie_density(vec3 position) {
	float time = frame / 1000.0;
	vec3 lower_bound = cloud_location - cloud_volume;
	vec3 upper_bound = cloud_location + cloud_volume;

	// edge weight.
	// to not cut off the clouds abruptly
	float distance_edge_x = min(cloud_volume_edge_fade_distance, min(position.x - lower_bound.x, upper_bound.x - position.x));
	float distance_edge_z = min(cloud_volume_edge_fade_distance, min(position.z - lower_bound.z, upper_bound.z - position.z));
	float edge_weight = min(distance_edge_x, distance_edge_z) / cloud_volume_edge_fade_distance;

	// !!! -> round cloud based on height - probably not an efficient approach
	// https://www.desmos.com/calculator/lg2fhwtxvo
	float height = 1.0 - pow((position.y - lower_bound.y) / (2.0 * cloud_volume.y), 4);

	// 2d worley noise to decide where can clouds be rendered
	vec2 weather_sample_location = position.xz / noise_weather_scale + noise_weather_offset + wind_vector.xz * wind_weather_weight * time;
	float weather = max(texture(noise_weather_texture, weather_sample_location).r, 0.0);
	weather = weather * noise_weather_weight + (1.0 - noise_weather_weight); // apply noise weight

	// main cloud shape noise
	vec3 main_sample_location = position / noise_main_scale + noise_main_offset + wind_vector * wind_main_weight * time;
	float main_noise_fbm = texture(noise_main_texture, main_sample_location).r;

	// total density at current point obtained from these values
	float density = max(0.0, main_noise_fbm * height * weather * edge_weight - cloud_density_threshold);

	if (density > 0.0) {
		// add detail to cloud's shape
		vec3 detail_sample_location = position / noise_detail_scale + noise_detail_offset + wind_vector * wind_detail_weight * time;
		float detail_noise_fbm = texture(noise_detail_texture, detail_sample_location).r;
		density -= detail_noise_fbm * noise_detail_weight;
		return max(0.0, density * cloud_density_multiplier);
	}
	return 0.0;
}

// approximation of a mie phase function
// -> due to mie scattering's complexity, 
//    an approximation is used.
// -> an even cheaper alternative, if 
//    needed, is be the Schlik phase 
//    function, which doesn't use pow.
//
float henyey_greenstein(float g, float angle_cos) {
	float g2 = g * g;
	return (1.0 - g2) / (pow(1 + g2 - 2 * g * angle_cos, 1.5));
}

float mie_in_scatter(vec3 position) {
	float distance_inside_volume = ray_to_cloud(position, inverse_light_direction, cloud_location - cloud_volume, cloud_location + cloud_volume).y;
	distance_inside_volume = min(render_shadowing_max_distance, distance_inside_volume);
	float step_size = distance_inside_volume / float(render_in_scatter_samples);
	float transparency = 1.0; // transparent
	float total_density = 0.0;
	for (int i = 0; i < render_in_scatter_samples; ++i) {
		total_density += (mie_density(position) * step_size);
		position += light_direction * step_size;
	}
	return (1.0 - render_shadowing_weight) + exp(-total_density * cloud_absorption) * render_shadowing_weight;
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
			float l_sde = atmosphere_march(point, light_direction, radius_atmosphere);
			vec2 depth_accumulation = vec2(0.0);
			{
				l_sde /= SCATTER_DEPTH_STEP;
				vec3 direction_sde = light_direction * l_sde;
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

	float mu = dot(direction, light_direction);

	return sqrt(sun_intensity * (1.0 + mu * mu) * (intensity_rayleigh * rayleigh_coefficient * 0.0597 + intensity_mie * mie_coefficient_lower * 0.0196 / pow(1.58 - 1.52 * mu, 1.5)));
}
