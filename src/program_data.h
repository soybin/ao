#include <cstring>

struct program_data {
	const char* compute_main = {
		"#version 430\n"
			"layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;\n"
			"layout(r8, location = 0) uniform image3D output_texture;\n"
			"\n"
			"// point grid buffers (SSBOs)\n"
			"layout(std430, binding = 1) buffer points_a {\n"
			"	vec4 p_a[];\n"
			"};\n"
			"\n"
			"layout(std430, binding = 2) buffer points_b {\n"
			"	vec4 p_b[];\n"
			"};\n"
			"\n"
			"layout(std430, binding = 3) buffer points_c {\n"
			"	vec4 p_c[];\n"
			"};\n"
			"\n"
			"// ---- vars ---- //\n"
			"// per axis resolution of the texture\n"
			"uniform int resolution;\n"
			"// tex + persistance * tex2 + persistance^2 * tex2\n"
			"uniform float persistance;\n"
			"// voronoi grid random points.\n"
			"// 3 channels.\n"
			"uniform int subdivisions_a;\n"
			"uniform int subdivisions_b;\n"
			"uniform int subdivisions_c;\n"
			"\n"
			"int min_component(ivec3 x) {\n"
			"	return min(x.x, min(x.y, x.z));\n"
			"}\n"
			"\n"
			"int max_component(ivec3 x) {\n"
			"	return max(x.x, max(x.y, x.z));\n"
			"}\n"
			"\n"
			"const ivec3 offsets[27] = ivec3[27](\n"
			"	// centre\n"
			"	ivec3(0,0,0),\n"
			"	// front face\n"
			"	ivec3(0,0,1),\n"
			"	ivec3(-1,1,1),\n"
			"	ivec3(-1,0,1),\n"
			"	ivec3(-1,-1,1),\n"
			"	ivec3(0,1,1),\n"
			"	ivec3(0,-1,1),\n"
			"	ivec3(1,1,1),\n"
			"	ivec3(1,0,1),\n"
			"	ivec3(1,-1,1),\n"
			"	// back face\n"
			"	ivec3(0,0,-1),\n"
			"	ivec3(-1,1,-1),\n"
			"	ivec3(-1,0,-1),\n"
			"	ivec3(-1,-1,-1),\n"
			"	ivec3(0,1,-1),\n"
			"	ivec3(0,-1,-1),\n"
			"	ivec3(1,1,-1),\n"
			"	ivec3(1,0,-1),\n"
			"	ivec3(1,-1,-1),\n"
			"	// ring around centre\n"
			"	ivec3(-1,1,0),\n"
			"	ivec3(-1,0,0),\n"
			"	ivec3(-1,-1,0),\n"
			"	ivec3(0,1,0),\n"
			"	ivec3(0,-1,0),\n"
			"	ivec3(1,1,0),\n"
			"	ivec3(1,0,0),\n"
			"	ivec3(1,-1,0)\n"
			");\n"
			"\n"
			"// computes a layer of the noise\n"
			"float compute_worley_layer(vec3 pos, int sub, int point_grid_index) {\n"
			"	ivec3 cell_id = ivec3(floor(pos * sub));\n"
			"	float min_dist = 1.0;\n"
			"	for (int offset_index = 0; offset_index < 27; ++offset_index) {\n"
			"		ivec3 adj_id = cell_id + offsets[offset_index];\n"
			"		// if adj cell is outside, blend texture to it ->\n"
			"		// repeat seemlessly\n"
			"		if (min_component(adj_id) == -1 || max_component(adj_id) == sub) {\n"
			"			ivec3 wrapped_id = ivec3(mod(adj_id + ivec3(sub), vec3(sub)));\n"
			"			// get index\n"
			"			int adj_index = wrapped_id.x + sub * (wrapped_id.y + wrapped_id.z * sub);\n"
			"			vec3 wrapped_point;\n"
			"			if (point_grid_index == 0) {\n"
			"				wrapped_point = p_a[adj_index].xyz;\n"
			"			} else if (point_grid_index == 1) {\n"
			"				wrapped_point = p_b[adj_index].xyz;\n"
			"			} else {\n"
			"				wrapped_point = p_c[adj_index].xyz;\n"
			"			}\n"
			"			for (int wrapped_offset_index = 0; wrapped_offset_index < 27; ++wrapped_offset_index) {\n"
			"				vec3 difference = (pos - (wrapped_point + vec3(offsets[wrapped_offset_index])));\n"
			"				min_dist = min(min_dist, dot(difference, difference));\n"
			"			}\n"
			"		} else {\n"
			"			// adj cell is inside known map\n"
			"			// calc distance from position to cell point\n"
			"			int adj_index = adj_id.x + sub * (adj_id.y + adj_id.z * sub);\n"
			"			vec3 difference = pos;\n"
			"			if (point_grid_index == 0) {\n"
			"				difference -= p_a[adj_index].xyz;\n"
			"			} else if (point_grid_index == 1) {\n"
			"				difference -= p_b[adj_index].xyz;\n"
			"			} else {\n"
			"				difference -= p_c[adj_index].xyz;\n"
			"			}\n"
			"			min_dist = min(min_dist, dot(difference, difference));\n"
			"		}\n"
			"	}\n"
			"	return sqrt(min_dist);\n"
			"}\n"
			"\n"
			"void main() {\n"
			"	vec3 position = vec3(gl_GlobalInvocationID) / resolution;\n"
			"	float layer_a = compute_worley_layer(position, subdivisions_a, 0);\n"
			"	float layer_b = compute_worley_layer(position, subdivisions_b, 1);\n"
			"	float layer_c = compute_worley_layer(position, subdivisions_c, 2);\n"
			"	// combine layers\n"
			"	float noise_sum = layer_a + (layer_b * persistance) + (layer_c * persistance * persistance);\n"
			"	// map to 0.0 - 1.0\n"
			"	noise_sum /= (1.0 + persistance + (persistance * persistance));\n"
			"	// invert\n"
			"	noise_sum = 1.0 - noise_sum;\n"
			"	// accentuate dark tones\n"
			"	noise_sum = noise_sum * noise_sum * noise_sum * noise_sum; // noise_sum^2\n"
			"	// write to texture\n"
			"	imageStore(output_texture, ivec3(gl_GlobalInvocationID), vec4(noise_sum));\n"
			"}\n"
	};

	const char* compute_weather = {
		"#version 430\n"
			"layout(local_size_x = 8, local_size_y = 8) in;\n"
			"layout(r8, location = 0) uniform image2D output_texture;\n"
			"\n"
			"// point grid buffers (SSBOs)\n"
			"layout(std430, binding = 1) buffer points_a {\n"
			"	vec4 p_a[];\n"
			"};\n"
			"\n"
			"layout(std430, binding = 2) buffer points_b {\n"
			"	vec4 p_b[];\n"
			"};\n"
			"\n"
			"layout(std430, binding = 3) buffer points_c {\n"
			"	vec4 p_c[];\n"
			"};\n"
			"\n"
			"// ---- vars ---- //\n"
			"// per axis resolution of the texture\n"
			"uniform int resolution;\n"
			"// tex + persistance * tex2 + persistance^2 * tex2\n"
			"uniform float persistance;\n"
			"// voronoi grid random points.\n"
			"// 3 channels.\n"
			"uniform int subdivisions_a;\n"
			"uniform int subdivisions_b;\n"
			"uniform int subdivisions_c;\n"
			"\n"
			"const ivec2 offsets[9] = ivec2[9](\n"
			"	// centre\n"
			"	ivec2(0,0),\n"
			"	// ring around centre\n"
			"	ivec2(-1,1),\n"
			"	ivec2(-1,0),\n"
			"	ivec2(-1,-1),\n"
			"	ivec2(0,1),\n"
			"	ivec2(0,-1),\n"
			"	ivec2(1,1),\n"
			"	ivec2(1,0),\n"
			"	ivec2(1,-1)\n"
			");\n"
			"\n"
			"// computes a layer of the noise\n"
			"float compute_worley_layer(vec2 pos, int sub, int point_grid_index) {\n"
			"	ivec2 cell_id = ivec2(floor(pos * sub));\n"
			"	float min_dist = 1.0;\n"
			"	for (int offset_index = 0; offset_index < 9; ++offset_index) {\n"
			"		ivec2 adj_id = cell_id + offsets[offset_index];\n"
			"		// if adj cell is outside, blend texture to it ->\n"
			"		// repeat seemlessly\n"
			"		if (min(adj_id.x, adj_id.y) == -1 || max(adj_id.x, adj_id.y) == sub) {\n"
			"			ivec2 wrapped_id = ivec2(mod(adj_id + ivec2(sub), vec2(sub)));\n"
			"			// get index\n"
			"			int adj_index = wrapped_id.x + sub * wrapped_id.y;\n"
			"			vec2 wrapped_point;\n"
			"			if (point_grid_index == 0) {\n"
			"				wrapped_point = p_a[adj_index].xy;\n"
			"			} else if (point_grid_index == 1) {\n"
			"				wrapped_point = p_b[adj_index].xy;\n"
			"			} else {\n"
			"				wrapped_point = p_c[adj_index].xy;\n"
			"			}\n"
			"			for (int wrapped_offset_index = 0; wrapped_offset_index < 9; ++wrapped_offset_index) {\n"
			"				vec2 difference = (pos - (wrapped_point + vec2(offsets[wrapped_offset_index])));\n"
			"				min_dist = min(min_dist, dot(difference, difference));\n"
			"			}\n"
			"		} else {\n"
			"			// adj cell is inside known map\n"
			"			// calc distance from position to cell point\n"
			"			int adj_index = adj_id.x + sub * adj_id.y;\n"
			"			vec2 difference = pos;\n"
			"			if (point_grid_index == 0) {\n"
			"				difference -= p_a[adj_index].xy;\n"
			"			} else if (point_grid_index == 1) {\n"
			"				difference -= p_b[adj_index].xy;\n"
			"			} else {\n"
			"				difference -= p_c[adj_index].xy;\n"
			"			}\n"
			"			min_dist = min(min_dist, dot(difference, difference));\n"
			"		}\n"
			"	}\n"
			"	return sqrt(min_dist);\n"
			"}\n"
			"\n"
			"void main() {\n"
			"	vec2 position = vec2(gl_GlobalInvocationID) / resolution;\n"
			"	float layer_a = compute_worley_layer(position, subdivisions_a, 0);\n"
			"	float layer_b = compute_worley_layer(position, subdivisions_b, 1);\n"
			"	float layer_c = compute_worley_layer(position, subdivisions_c, 2);\n"
			"	// combine layers\n"
			"	float noise_sum = layer_a + (layer_b * persistance) + (layer_c * persistance * persistance);\n"
			"	// map to 0.0 - 1.0\n"
			"	noise_sum /= (1.0 + persistance + (persistance * persistance));\n"
			"	// invert\n"
			"	noise_sum = 1.0 - noise_sum;\n"
			"	// accentuate dark tones\n"
			"	noise_sum = pow(noise_sum, 4); // noise^4\n"
			"	// write to texture\n"
			"	imageStore(output_texture, ivec2(gl_GlobalInvocationID), vec4(noise_sum));\n"
			"}\n"
	};

	const char* vertex = {
		"#version 430 core\n"
			"\n"
			"layout(location = 0) in vec2 vin_position;\n"
			"layout(location = 0) out vec4 vout_position;\n"
			"\n"
			"void main() {\n"
			"	gl_Position = vec4(vin_position, 0.0, 1.0);\n"
			"}\n"
	};

	const char* fragment = {
		"/*\n"
			" * MIT License\n"
			" * Copyright (c) 2020 Pablo Peñarroja\n"
			" */\n"
			"\n"
			"#version 430 core\n"
			"\n"
			"layout(location = 0) in vec4 v_position;\n"
			"layout(location = 0) out vec4 out_color;\n"
			"\n"
			"const float PI = 3.14159265;\n"
			"\n"
			"// planet's atmosphere constants-earth by default\n"
			"const float SCATTER_IN_STEP = 16.0;\n"
			"const float SCATTER_DEPTH_STEP = 4.0;\n"
			"const float radius_surface = 6360e3;\n"
			"const float radius_atmosphere = 6380e3;\n"
			"const float sun_intensity = 8.2;\n"
			"const vec3 rayleigh_coefficient = vec3(58e-7, 135e-7, 331e-7);\n"
			"const vec3 mie_coefficient_upper = vec3(2e-5);\n"
			"const vec3 mie_coefficient_lower = mie_coefficient_upper * 1.1;\n"
			"const vec3 earth_center = vec3(0.0, -radius_surface, 0.0);\n"
			"\n"
			"// ---------------------------- //\n"
			"// -------- parameters -------- //\n"
			"// ---------------------------- //\n"
			"\n"
			"uniform int frame;\n"
			"uniform int render_sky;\n"
			"uniform float noise_depth;\n"
			"uniform float noise_zoom;\n"
			"uniform vec2 resolution;\n"
			"uniform vec3 background_color;\n"
			"uniform vec3 box_size;\n"
			"uniform vec3 cloud_color;\n"
			"uniform vec3 light_direction;\n"
			"uniform vec3 inverse_light_direction;\n"
			"uniform vec3 camera_location;\n"
			"uniform mat4 view_matrix;\n"
			"\n"
			"// cloud\n"
			"uniform float cloud_absorption;\n"
			"uniform float cloud_density_threshold;\n"
			"uniform float cloud_density_multiplier;\n"
			"uniform float cloud_volume_edge_fade_distance;\n"
			"uniform vec3 cloud_location;\n"
			"uniform vec3 cloud_volume;\n"
			"\n"
			"// render\n"
			"uniform int render_volume_samples;\n"
			"uniform int render_in_scatter_samples;\n"
			"uniform float render_shadowing_max_distance;\n"
			"uniform float render_shadowing_weight;\n"
			"\n"
			"// noise\n"
			"uniform float noise_main_scale;\n"
			"uniform vec3 noise_main_offset;\n"
			"uniform float noise_weather_scale;\n"
			"uniform vec2 noise_weather_offset;\n"
			"uniform float noise_detail_scale;\n"
			"uniform float noise_detail_weight;\n"
			"uniform vec3 noise_detail_offset;\n"
			"uniform sampler3D noise_main_texture;\n"
			"uniform sampler2D noise_weather_texture;\n"
			"uniform sampler3D noise_detail_texture;\n"
			"\n"
			"uniform vec3 wind_vector;\n"
			"uniform float wind_main_weight;\n"
			"uniform float wind_weather_weight;\n"
			"uniform float wind_detail_weight;\n"
			"\n"
			"float remap(float value, float old_low, float old_high, float new_low, float new_high) {\n"
			"	return new_low + (value - old_low) * (new_high - new_low) / (old_high - old_low);\n"
			"}\n"
			"\n"
			"// ---- clouds ---- declarations ---- //\n"
			"float mie_density(vec3 position);\n"
			"float beer_powder(float x);\n"
			"float henyey_greenstein(float x, float y);\n"
			"float phase(float x);\n"
			"float mie_in_scatter(vec3 position);\n"
			"vec2 ray_to_cloud(vec3 origin, vec3 inverted_direction, vec3 vol_left_bound, vec3 vol_right_bound);\n"
			"// ------------------------------- //\n"
			"\n"
			"// ---- atmosphere ---- declarations ---- //\n"
			"vec2 atmosphere_density(vec3 point);\n"
			"float atmosphere_march(vec3 origin, vec3 direction, float radius);\n"
			"vec3 atmosphere_scatter(vec3 direction, float l);\n"
			"// ------------------------------------ //\n"
			"\n"
			"void main() {\n"
			"\n"
			"	// ---- ray direction ---- // \n"
			"\n"
			"	vec2 uv = gl_FragCoord.xy / resolution * 2.0 - 1.0;\n"
			"	uv.x *= resolution.x / resolution.y;\n"
			"	vec4 dir = vec4(normalize(vec3(uv, -2.0)), 1.0);\n"
			"	dir = view_matrix * dir;\n"
			"\n"
			"	// ---- rayleigh ---- //\n"
			"\n"
			"	vec3 atmosphere_color = vec3(0.0);\n"
			"	if (render_sky == 1) {\n"
			"		float l = atmosphere_march(camera_location, dir.xyz, radius_atmosphere);\n"
			"		atmosphere_color = atmosphere_scatter(dir.xyz, l);\n"
			"	} else {\n"
			"		atmosphere_color = background_color;\n"
			"	}\n"
			"\n"
			"	// ---- mie ---- //\n"
			"	\n"
			"	float transmittance = 1.0; // transparent\n"
			"	vec3 color_cloud = vec3(0.0); // accumulated light\n"
			"	\n"
			"	vec2 march = ray_to_cloud(camera_location, 1.0 / dir.xyz, cloud_location - cloud_volume, cloud_location + cloud_volume);\n"
			"	float distance_per_step = march.y / render_volume_samples;\n"
			"	float distance_travelled = 0.0;\n"
			"\n"
			"	// henyey greenstein phase function\n"
			"	// value for this ray's direction.\n"
			"	// -> provides silver lining when\n"
			"	//    looking towards sun.\n"
			"	float hg_constant = henyey_greenstein(0.2, dot(dir.xyz, light_direction));\n"
			"\n"
			"	// if ray hits cloud, compute amount of\n"
			"	// light that reaches the cloud's surface\n"
			"\n"
			"	for (; distance_travelled < march.y; distance_travelled += distance_per_step) {\n"
			"		vec3 ray_position = camera_location + dir.xyz * (march.x + distance_travelled);\n"
			"		// sample noise density at current\n"
			"		// ray position.\n"
			"		float density = mie_density(ray_position);\n"
			"		// extinguish transmittance using\n"
			"		// beer's law -> (e^(-d*deltaX)).\n"
			"		transmittance *= exp(-density * distance_per_step);\n"
			"		// avoid doing extra loops if it's\n"
			"		// already dark.\n"
			"		if (transmittance < 0.01) break;\n"
			"		// amount of light in-scattered to \n"
			"		// this point in the cloud;\n"
			"		// extinction coefficient when going\n"
			"		// through the volume toward the sun.\n"
			"		float in_light = mie_in_scatter(ray_position);\n"
			"		// add to cloud's surface color\n"
			"		color_cloud += density * distance_per_step * in_light * transmittance * hg_constant;\n"
			"	}\n"
			"\n"
			"	// return fragment color\n"
			"	out_color = vec4((atmosphere_color * transmittance) + color_cloud, 1.0);\n"
			"}\n"
			"\n"
			"// --------------------- //\n"
			"// -------- mie -------- //\n"
			"// --------------------- //\n"
			"\n"
			"float mie_density(vec3 position) {\n"
			"	float time = frame / 1000.0;\n"
			"	vec3 lower_bound = cloud_location - cloud_volume;\n"
			"	vec3 upper_bound = cloud_location + cloud_volume;\n"
			"\n"
			"	// edge weight.\n"
			"	// to not cut off the clouds abruptly\n"
			"	float distance_edge_x = min(cloud_volume_edge_fade_distance, min(position.x - lower_bound.x, upper_bound.x - position.x));\n"
			"	float distance_edge_z = min(cloud_volume_edge_fade_distance, min(position.z - lower_bound.z, upper_bound.z - position.z));\n"
			"	float edge_weight = min(distance_edge_x, distance_edge_z) / cloud_volume_edge_fade_distance;\n"
			"\n"
			"	// !!! -> round cloud based on height - probably not an efficient approach\n"
			"	// https://www.desmos.com/calculator/lg2fhwtxvo\n"
			"	float height = 1.0 - pow((position.y - lower_bound.y) / (2.0 * cloud_volume.y), 4);\n"
			"\n"
			"	// 2d worley noise to decide where can clouds be rendered\n"
			"	vec2 weather_sample_location = position.xz / noise_weather_scale + noise_weather_offset + wind_vector.xz * wind_weather_weight * time;\n"
			"	float weather = max(texture(noise_weather_texture, weather_sample_location).r, 0.0);\n"
			"	weather = max(weather - cloud_density_threshold, 0.0);\n"
			"\n"
			"	// main cloud shape noise\n"
			"	vec3 main_sample_location = position / noise_main_scale + noise_main_offset + wind_vector * wind_main_weight * time;\n"
			"	float main_noise_fbm = texture(noise_main_texture, main_sample_location).r;\n"
			"\n"
			"	// total density at current point obtained from these values\n"
			"	float density = max(0.0, main_noise_fbm * height * weather * edge_weight - cloud_density_threshold);\n"
			"\n"
			"	if (density > 0.0) {\n"
			"		// add detail to cloud's shape\n"
			"		vec3 detail_sample_location = position / noise_detail_scale + noise_detail_offset + wind_vector * wind_detail_weight * time;\n"
			"		float detail_noise_fbm = texture(noise_detail_texture, detail_sample_location).r;\n"
			"		density -= detail_noise_fbm * noise_detail_weight;\n"
			"		return max(0.0, density * cloud_density_multiplier);\n"
			"	}\n"
			"	return 0.0;\n"
			"}\n"
			"\n"
			"// approximation of a mie phase function\n"
			"// -> due to mie scattering's complexity, \n"
			"//    an approximation is used.\n"
			"// -> an even cheaper alternative, if \n"
			"//    needed, is be the Schlik phase \n"
			"//    function, which doesn't use pow.\n"
			"//\n"
			"float henyey_greenstein(float g, float angle_cos) {\n"
			"	float g2 = g * g;\n"
			"	return (1.0 - g2) / (pow(1 + g2 - 2 * g * angle_cos, 1.5));\n"
			"}\n"
			"\n"
			"float mie_in_scatter(vec3 position) {\n"
			"	float distance_inside_volume = ray_to_cloud(position, inverse_light_direction, cloud_location - cloud_volume, cloud_location + cloud_volume).y;\n"
			"	distance_inside_volume = min(render_shadowing_max_distance, distance_inside_volume);\n"
			"	float step_size = distance_inside_volume / float(render_in_scatter_samples);\n"
			"	float transparency = 1.0; // transparent\n"
			"	float total_density = 0.0;\n"
			"	for (int i = 0; i < render_in_scatter_samples; ++i) {\n"
			"		total_density += (mie_density(position) * step_size);\n"
			"		position += light_direction * step_size;\n"
			"	}\n"
			"	return (1.0 - render_shadowing_weight) + exp(-total_density * cloud_absorption) * render_shadowing_weight;\n"
			"}\n"
			"\n"
			"// from\n"
			"// http://jcgt.org/published/0007/03/04/\n"
			"// returns float2:\n"
			"// 	x -> distance to cloud volume\n"
			"// 	y -> distance across cloud volume\n"
			"vec2 ray_to_cloud(vec3 origin, vec3 inverted_direction, vec3 vol_left_bound, vec3 vol_right_bound) {\n"
			"	vec3 t0 = (vol_left_bound - origin) * inverted_direction;\n"
			"	vec3 t1 = (vol_right_bound - origin) * inverted_direction;\n"
			"	vec3 tmin = min(t0, t1);\n"
			"	vec3 tmax = max(t0, t1);\n"
			"	float dist_maxmin = max(max(tmin.x, tmin.y), tmin.z);\n"
			"	float dist_minmax = min(tmax.x, min(tmax.y, tmax.z));\n"
			"	float dist_to_volume = max(0.0, dist_maxmin);\n"
			"	float dist_across_volume = max(0.0, dist_minmax - dist_to_volume);\n"
			"	return vec2(dist_to_volume, dist_across_volume);\n"
			"}\n"
			"\n"
			"// ---------------------------- //\n"
			"// -------- atmosphere -------- //\n"
			"// ---------------------------- //\n"
			"\n"
			"vec2 atmosphere_density(vec3 point) {\n"
			"	float h = max(0.0, length(point - earth_center) - radius_surface);\n"
			"	return vec2(exp(-h / 8e3), exp(-h / 12e2));\n"
			"}\n"
			"\n"
			"float atmosphere_march(vec3 origin, vec3 direction, float radius) {\n"
			"	// origin - earth center\n"
			"	vec3 v = origin - earth_center;\n"
			"	float b = dot(v, direction);\n"
			"	float d = b * b - dot(v, v) + radius * radius;\n"
			"	if (d < 0.) return -1.;\n"
			"	d = sqrt(d);\n"
			"	float r1 = -b - d, r2 = -b + d;\n"
			"	return (r1 >= 0.) ? r1 : r2;\n"
			"}\n"
			"\n"
			"vec3 atmosphere_scatter(vec3 direction, float l) {\n"
			"	// scatter in\n"
			"	vec2 total_depth = vec2(0.0);\n"
			"	vec3 intensity_rayleigh = vec3(0.0);\n"
			"	vec3 intensity_mie = vec3(0.0);\n"
			"	{\n"
			"		float l_sin = l / SCATTER_IN_STEP;\n"
			"		vec3 direction_sin = direction * l_sin;\n"
			"\n"
			"		// iterate point\n"
			"		for (int i = 0; i < SCATTER_IN_STEP; ++i) {\n"
			"			vec3 point = camera_location + direction_sin * i;		\n"
			"			vec2 depth = atmosphere_density(point) * l_sin;\n"
			"			total_depth += depth;\n"
			"\n"
			"			// calculate scatter depth\n"
			"			float l_sde = atmosphere_march(point, light_direction, radius_atmosphere);\n"
			"			vec2 depth_accumulation = vec2(0.0);\n"
			"			{\n"
			"				l_sde /= SCATTER_DEPTH_STEP;\n"
			"				vec3 direction_sde = light_direction * l_sde;\n"
			"				// iterate point\n"
			"				for (int j = 0; j < SCATTER_DEPTH_STEP; ++j) {\n"
			"					depth_accumulation += atmosphere_density(point + direction_sde * j);\n"
			"				}\n"
			"				depth_accumulation *= l_sde;\n"
			"			}\n"
			"\n"
			"			vec2 depth_sum = total_depth + depth_accumulation;\n"
			"\n"
			"			vec3 a = exp(-rayleigh_coefficient * depth_sum.x - mie_coefficient_upper * depth_sum.y);\n"
			"\n"
			"			intensity_rayleigh += a * depth.x;\n"
			"			intensity_mie += a * depth.y;\n"
			"		}\n"
			"	}\n"
			"\n"
			"	float mu = dot(direction, light_direction);\n"
			"\n"
			"	return sqrt(sun_intensity * (1.0 + mu * mu) * (intensity_rayleigh * rayleigh_coefficient * 0.0597 + intensity_mie * mie_coefficient_lower * 0.0196 / pow(1.58 - 1.52 * mu, 1.5)));\n"
			"}\n"
	};
};
