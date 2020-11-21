#version 430 core

layout(location = 0) in vec2 vin_position;
layout(location = 0) out vec4 vout_position;

void main() {
	gl_Position = vec4(vin_position, 0.0, 1.0);
}
