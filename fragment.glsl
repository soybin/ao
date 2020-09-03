#version 430 core

layout(location = 0) in vec4 v_position;
layout(location = 0) out vec4 color;

#define PI 3.14159265

const float MIN_DIST = 0.01;
const int MAX_DIST = 512;
const int MAX_STEP = 256;

const float radius_surface = 6360e3;
const float radius_atmosphere = 6380e3;
const vec3 rayleigh_coefficient = vec3(58e-7, 135e-7, 331e-7);
const vec3 mie_coefficient_upper = vec3(2e-5);
const vec3 mie_coefficient_lower = mie_coefficient_upper * 1.1;
const float sun_intensity = 5.0;
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
const float low = 1e3, hi = 25e2;

float march(vec3 p, vec3 d, float R) {
    vec3 v = p - earth_center;
    float b = dot(v, d);
    float det = b * b - dot(v, v) + R*R;
    if (det < 0.) return -1.;
    det = sqrt(det);
    float t1 = -b - det, t2 = -b + det;
    return (t1 >= 0.) ? t1 : t2;
}

// integrate densities for rayleigh and
// mie scattering. (rayleigh, mie)
vec2 scatter_depth(vec3 origin, vec3 direction, float l, float steps) {
	// accumulate here
	vec2 depth = vec2(0.0);

	l /= steps;
	depth *= l;

	// iterate point
	for (int i = 1; i < steps; ++i) {
		depth += density(origin + direction * i);
	}

	return depth * l;
}

// some globals 一 they aren't that bad
vec2 total_depth;
vec3 intensity_rayleigh, intensity_mie;

void scatter_in(vec3 origin, vec3 direction, float l, float steps) {
	l /= steps;
	direction *= l;

	// iterate point
	for (int i = 0; i < steps; ++i) {
		vec3 p = origin + direction * i;		
		// get density
		vec2 d = density(p) * l;
		// add
		total_depth += d;

		vec2 depth_sum = total_depth + scatter_depth(p, sun_direction, march(p, sun_direction, radius_atmosphere), 4.0);

		vec3 a = exp(-rayleigh_coefficient * depth_sum.x - mie_coefficient_upper * depth_sum.y);

		intensity_rayleigh += a * d.x;
		intensity_mie += a * d.y;
	}
}

vec3 skydome(vec3 origin, vec3 direction, float l, vec3 lo) {
	total_depth = vec2(0.0);
	intensity_rayleigh = vec3(0.0);
	intensity_mie = vec3(0.0);
	scatter_in(origin, direction, l, 16.0);
	float mu = dot(direction, normalize(sun_direction));
	// lo extinction
	return 
		lo * exp(-rayleigh_coefficient * total_depth.x - mie_coefficient_upper * total_depth.y)
		+
		sun_intensity * (1.0 + mu * mu)
		*
		(intensity_rayleigh * rayleigh_coefficient * 0.0597 
		 +
		 intensity_mie * mie_coefficient_lower * 0.0196 / pow(1.58 - 1.52 * mu, 1.5)
	);
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
	col = skydome(O, D, L, col);
	color = vec4(sqrt(col), 1.0);
}

