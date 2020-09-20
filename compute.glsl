#version 430
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
layout(rgba8, location = 0) uniform image3D output_texture;

// point grid buffers (SSBOs)
layout(std430, binding = 1) buffer points_a {
	vec4 p_a[];
};

layout(std430, binding = 2) buffer points_b {
	vec4 p_b[];
};

layout(std430, binding = 3) buffer points_c {
	vec4 p_c[];
};

// ---- vars ---- //
// per axis resolution of the texture
uniform int resolution = 256;
// tex + persistance * tex2 + persistance^2 * tex2
uniform float persistance = 0.0;
// to change working channel
uniform ivec4 channel_mask = ivec4(0, 0, 0, 0);
// voronoi grid random points.
// 3 channels.
uniform int subdivisions_a = 8;
uniform int subdivisions_b = 12;
uniform int subdivisions_c = 16;

int min_component(ivec3 x) {
	return min(x.x, min(x.y, x.z));
}

int max_component(ivec3 x) {
	return max(x.x, max(x.y, x.z));
}

const ivec3 offsets[27] = ivec3[27](
	// centre
	ivec3(0,0,0),
	// front face
	ivec3(0,0,1),
	ivec3(-1,1,1),
	ivec3(-1,0,1),
	ivec3(-1,-1,1),
	ivec3(0,1,1),
	ivec3(0,-1,1),
	ivec3(1,1,1),
	ivec3(1,0,1),
	ivec3(1,-1,1),
	// back face
	ivec3(0,0,-1),
	ivec3(-1,1,-1),
	ivec3(-1,0,-1),
	ivec3(-1,-1,-1),
	ivec3(0,1,-1),
	ivec3(0,-1,-1),
	ivec3(1,1,-1),
	ivec3(1,0,-1),
	ivec3(1,-1,-1),
	// ring around centre
	ivec3(-1,1,0),
	ivec3(-1,0,0),
	ivec3(-1,-1,0),
	ivec3(0,1,0),
	ivec3(0,-1,0),
	ivec3(1,1,0),
	ivec3(1,0,0),
	ivec3(1,-1,0)
);

// computes a layer of the noise
float compute_worley_layer(vec3 pos, int sub, int point_grid_index) {
	ivec3 cell_id = ivec3(floor(pos * sub));
	float min_dist = 1.0;
	for (int offset_index = 0; offset_index < 27; ++offset_index) {
		ivec3 adj_id = cell_id + offsets[offset_index];
		// if adj cell is outside, blend texture to it ->
		// repeat seemlessly
		if (min_component(adj_id) == -1 || max_component(adj_id) == sub) {
			ivec3 wrapped_id = ivec3(mod(adj_id + ivec3(sub), vec3(sub)));
			// get index
			int adj_index = wrapped_id.x + sub * (wrapped_id.y + wrapped_id.z * sub);
			vec3 wrapped_point;
			if (point_grid_index == 0) {
				wrapped_point = p_a[adj_index].xyz;
			} else if (point_grid_index == 1) {
				wrapped_point = p_b[adj_index].xyz;
			} else {
				wrapped_point = p_c[adj_index].xyz;
			}
			for (int wrapped_offset_index = 0; wrapped_offset_index < 27; ++wrapped_offset_index) {
				vec3 difference = (pos - (wrapped_point + vec3(offsets[wrapped_offset_index])));
				min_dist = min(min_dist, dot(difference, difference));
			}
		} else {
			// adj cell is inside known map
			// calc distance from position to cell point
			int adj_index = adj_id.x + sub * (adj_id.y + adj_id.z * sub);
			vec3 difference = pos;
			if (point_grid_index == 0) {
				difference -= p_a[adj_index].xyz;
			} else if (point_grid_index == 1) {
				difference -= p_b[adj_index].xyz;
			} else {
				difference -= p_c[adj_index].xyz;
			}
			min_dist = min(min_dist, dot(difference, difference));
		}
	}
	return sqrt(min_dist);
}

void main() {
	vec3 position = vec3(gl_GlobalInvocationID) / resolution;
	float layer_a = compute_worley_layer(position, subdivisions_a, 0);
	float layer_b = compute_worley_layer(position, subdivisions_b, 1);
	float layer_c = compute_worley_layer(position, subdivisions_c, 2);
	// add layers
	float noise_sum = layer_a + (layer_b * persistance) + (layer_c * persistance * persistance);
	// map to 0.0 - 1.0
	noise_sum /= (1.0 + persistance + (persistance * persistance));
	// invert
	noise_sum = 1.0 - noise_sum;
	// accentuate dark tones
	noise_sum = noise_sum * noise_sum; // noise_sum^2
	// apply mask
	vec4 result = channel_mask * noise_sum;
	// write to texture
	imageStore(output_texture, ivec3(gl_GlobalInvocationID), result);
}
