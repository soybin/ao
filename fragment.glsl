#version 430 core

layout(location = 0) out vec4 color;

const float MIN_DIST = 0.001;
const int MAX_DIST = 512;
const int MAX_STEP = 256;
const int ITER = 5;
const float POWER = 5;
const int FOV = 60;

uniform int frames;
uniform vec2 resolution;

float sphereDE(vec3 point) {
	return length(point) - 0.3;
}

float smoothMin(float s1, float s2, float k) {
	float h = clamp(0.5 + 0.5 * (s2 - s1) / k, 0.0, 1.0);
	return mix(s2, s1, h) - k * h * (1.0 - h);
}

vec3 opRep(in vec3 p, in vec3 c) {
	return mod(p + c / 2.0, c) - (c / 2.0);
}

float march(vec3 pos, vec3 dir) {
	float td = 0.01;
	int step = 0;
	for (; step < MAX_STEP; ++step) {
		vec3 qq = vec3(1.0, 0.0, 1.0);
		float dist = sphereDE(pos + dir * td);
		if (dist < MIN_DIST) break;
		td += dist;
	}
	return 1.0 - (float(step)/float(MAX_STEP));
}

void main() {
	vec3 light_direction = vec3(-0.5, -0.3, 0.2);
	vec2 uv = gl_FragCoord.xy / resolution;
	vec3 pos = vec3(0.0, 0.0, 1.0);
	// calculate ray direction
	vec2 xy = gl_FragCoord.xy - resolution.xy / 2.0;
	float z = resolution.y / tan(radians(FOV) / 2.0);
	vec3 dir = normalize(vec3(xy, -z));
	color = vec4(vec3(march(pos, dir)), 1.0);
}
