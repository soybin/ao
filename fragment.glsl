#version 430 core

layout(location = 0) in vec4 v_position;
layout(location = 0) out vec4 color;

#define PI 3.14159265

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

// parameters
uniform int frames;
uniform vec2 resolution;
uniform vec3 box_size;
uniform vec3 sun_direction;
uniform vec3 camera_angle;

vec2 density(vec3 point) {
	float h = max(0.0, length(point - earth_center) - radius_surface);
	return vec2(exp(-h / 8e3), exp(-h / 12e2));
}

float march(vec3 point, vec3 direction, float radius) {
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
			float l_sde = march(point, sun_direction, radius_atmosphere);
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

// box signed distance function
float box_sdf(vec3 point) {
	vec3 x = abs(point) - box_size;
	return length(max(x, 0.0)) + min(max(x.x, max(x.y, x.z)), 0.0);
}

void main() {
	// normalize from -1.0 to 1.0
	vec2 uv = gl_FragCoord.xy / resolution * 2.0 - 1.0;
	uv.x *= resolution.x / resolution.y;
	vec3 O = vec3(0.0, 0.0, 0.0);
	vec3 D = normalize(vec3(uv, -2.0));
	vec3 col = vec3(0.0);
	float L = march(O, D, radius_atmosphere);
	col = scatter(O, D, L, col);
	color = vec4(col, 1.0);
}

