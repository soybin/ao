#version 430 core

layout(location = 0) in vec4 v_position;
layout(location = 0) out vec4 color;

const float MIN_DIST = 0.01; const int MAX_DIST = 512;
const int MAX_STEP = 256;
const int ITER = 5;
const float POWER = 5;
const int FOV = 60;

uniform int frames;
uniform vec2 resolution;
uniform vec3 box_size;

#define PI 3.141592
#define iSteps 16
#define jSteps 8

uniform vec3 uSunPos = vec3(0, 0.5, -1.0);

vec3 skydome(vec3 dir) {
	return vec3(1.0);
}

// box signed distance function
float box_sdf(vec3 point) {
	vec3 x = abs(point) - box_size;
	return length(max(x, 0.0)) + min(max(x.x, max(x.y, x.z)), 0.0);
}

// march function:
// if hit:
// return traveled distance
// else:
// return -1.0

float march(vec3 origin, vec3 dir) {
	float td = 0.0;
	int step = 0;
	for (; step < MAX_STEP; ++step) {
		float dist = box_sdf(origin + dir * td);
		if (dist < MIN_DIST) break;
		td += dist;
	}
	if (step == MAX_STEP) {
		return -1.0;
	}
	return td;
}

void main() {
	vec3 light_direction = vec3(-0.5, -0.3, 0.2);
	vec2 uv = gl_FragCoord.xy / resolution;
	vec3 pos = vec3(0.0, 0.0, 10.0);
	// calculate ray direction
	vec2 xy = gl_FragCoord.xy - resolution.xy / 2.0;
	float z = resolution.y / tan(radians(FOV) / 2.0);
	vec3 dir = normalize(vec3(xy, -z));
	float marched = march(pos, dir);
	if (marched > 0.0) {
		vec3 c = skydome(dir);
		// Apply exposure.
		c = 1.0 - exp(-1.0 * c);
		color = vec4(c, 1.0);
	} else {
		color = vec4(vec3(1.0), 1.0);
	}
}

