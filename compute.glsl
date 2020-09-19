#version 430
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(rgba8ui, location = 0) uniform writeonly uimage3D volume;

void main() {
	imageStore(volume, ivec3(gl_GlobalInvocationID.xyz), ivec4(255));
}
