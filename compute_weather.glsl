#version 430
layout(local_size_x = 8, local_size_y = 8) in;
layout(r8, location = 0) uniform image2D output_texture;

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
uniform int resolution;
// tex + persistance * tex2 + persistance^2 * tex2
uniform float persistance;
// voronoi grid random points.
// 3 channels.
uniform int subdivisions_a;
uniform int subdivisions_b;
uniform int subdivisions_c;

const ivec2 offsets[9] = ivec2[9](
	// centre
	ivec2(0,0),
	// ring around centre
	ivec2(-1,1),
	ivec2(-1,0),
	ivec2(-1,-1),
	ivec2(0,1),
	ivec2(0,-1),
	ivec2(1,1),
	ivec2(1,0),
	ivec2(1,-1)
);

// computes a layer of the noise
float compute_worley_layer(vec2 pos, int sub, int point_grid_index) {
	ivec2 cell_id = ivec2(floor(pos * sub));
	float min_dist = 1.0;
	for (int offset_index = 0; offset_index < 9; ++offset_index) {
		ivec2 adj_id = cell_id + offsets[offset_index];
		// if adj cell is outside, blend texture to it ->
		// repeat seemlessly
		if (min(adj_id.x, adj_id.y) == -1 || max(adj_id.x, adj_id.y) == sub) {
			ivec2 wrapped_id = ivec2(mod(adj_id + ivec2(sub), vec2(sub)));
			// get index
			int adj_index = wrapped_id.x + sub * wrapped_id.y;
			vec2 wrapped_point;
			if (point_grid_index == 0) {
				wrapped_point = p_a[adj_index].xy;
			} else if (point_grid_index == 1) {
				wrapped_point = p_b[adj_index].xy;
			} else {
				wrapped_point = p_c[adj_index].xy;
			}
			for (int wrapped_offset_index = 0; wrapped_offset_index < 9; ++wrapped_offset_index) {
				vec2 difference = (pos - (wrapped_point + vec2(offsets[wrapped_offset_index])));
				min_dist = min(min_dist, dot(difference, difference));
			}
		} else {
			// adj cell is inside known map
			// calc distance from position to cell point
			int adj_index = adj_id.x + sub * adj_id.y;
			vec2 difference = pos;
			if (point_grid_index == 0) {
				difference -= p_a[adj_index].xy;
			} else if (point_grid_index == 1) {
				difference -= p_b[adj_index].xy;
			} else {
				difference -= p_c[adj_index].xy;
			}
			min_dist = min(min_dist, dot(difference, difference));
		}
	}
	return sqrt(min_dist);
}

void main() {
	vec2 position = vec2(gl_GlobalInvocationID) / resolution;
	float layer_a = compute_worley_layer(position, subdivisions_a, 0);
	float layer_b = compute_worley_layer(position, subdivisions_b, 1);
	float layer_c = compute_worley_layer(position, subdivisions_c, 2);
	// fractal brownian motion - combine layers
	float noise_sum = layer_a + (layer_b * persistance) + (layer_c * persistance * persistance);
	// map to 0.0 - 1.0
	noise_sum /= (1.0 + persistance + (persistance * persistance));
	// invert
	noise_sum = 1.0 - noise_sum;
	// accentuate dark tones
	noise_sum = pow(noise_sum, 4); // noise^4
	// write to texture
	imageStore(output_texture, ivec2(gl_GlobalInvocationID), vec4(noise_sum));
}
