/*
 * MIT License
 * Copyright (c) 2020 Pablo Peñarroja
 */

#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <string>
#include <iostream>
#include <chrono>
#include <thread>
#include <cstdio>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#include "shader.h"

#define IMGUI_IMPL_OPENGL_LOADER_GLEW

// -------- n o i s e -------- //

/*
 * worley noise function:
 * data_ptr = pointer to ubytes array (0 - 255)
 * width, height, depth = dimensions
 * persistance = noisy doisy
 * channel = |0 = r, 1 = g, 2 = b, 3 = a|
 */
void update_noise(unsigned char* data_ptr, float persistance, int axis_resolution, int subdiv_a, int subdiv_b, int subdiv_c, char channel);

// -------- 青 一 a o -------- //

int main(int argc, char* argv[]) {

	// ---- init ao data ---- //

	int run = 1;
	int fullscreen = 0;
	int width = 1280;
	int height = 720;
	unsigned int vao;
	unsigned int vbo;
	shader* main_shader;
	GLFWwindow* window;
	// rendering
	int fps = 60;
	int millis_per_frame = 1000 / fps;
	// noise / clouds
	float box_size_x = 1.0f;
	float box_size_y = 1.0f;
	float box_size_z = 1.0f;
	// skydome
	float time = 6.0f;
	// camera
	glm::vec3 camera_location = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::mat4 view_matrix = glm::lookAt(camera_location, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	float camera_pitch = 0.0f;
	float camera_roll = 0.0f;
	float camera_yaw = 0.0f;

	// ---- init glfw ---- //

	if (!glfwInit()) {
		std::cout << "[-] GLFW initialization failed. Exiting ao." << std::endl;
		return 1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(width, height, "glsl", 0, 0);

	if (window == NULL) {
		glfwTerminate();
		std::cout << "[-] GLFW window initialization failed. Exiting ao." << std::endl;
		return 1;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	// ---- init glew ---- //

	if (glewInit() != GLEW_OK) {
		glfwTerminate();
		std::cout << "[-] GLEW initialization failed. Exiting ao." << std::endl;
		return 1;
	}

	// ---- init shader ---- //

	main_shader = new shader("./vertex.glsl", "./fragment.glsl");
	main_shader->bind();
	main_shader->set2f("resolution", width, height);

	//---- init imgui ----//

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();

	// initialize appropiate bindings
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 430");

	// change color theme
	ImGui::StyleColorsDark();

	// ---- load quad ---- //

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	// the vertices array won't be further needed
	{
		float vertices[] = { 
			1.0f, 1.0f,
			-1.0f, 1.0f,
			1.0f, -1.0f,
			-1.0f, -1.0f,
			-1.0f, 1.0f,
			1.0f, -1.0f 
		};
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices[0], GL_STATIC_DRAW);
	}
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

	// ---- init noise compute shader ---- //

	// read compute.glsl data
	std::string data = "";
	{
		std::ifstream file("./compute.glsl");
		std::string line;
		for (; std::getline(file, line); data += '\n') {
			data += line;
		}
	}
	const char* compute_shader_data = data.c_str();
	// create shader
	unsigned int compute_shader = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(compute_shader, 1, &compute_shader_data, NULL);
	glCompileShader(compute_shader);
	unsigned int compute_program = glCreateProgram();
	glAttachShader(compute_program, compute_shader);
	glLinkProgram(compute_program);
	// check if it compiled
	int res;
	glGetShaderiv(compute_shader, GL_COMPILE_STATUS, &res);
	if (!res) {
		// compilation failed. exception handling
		int len;
		glGetShaderiv(compute_shader, GL_INFO_LOG_LENGTH, &len);
		char* message = (char*)alloca(len * sizeof(char));
		glGetShaderInfoLog(compute_shader, len, &len, message);
		std::cout << "Failed to compile compute shader" << std::endl << message << std::endl;
	}

	// ---- noise ---- texture ---- //

	float zoom = 1.0f;
	float depth = 0.0f;

	// volume texture - 3d - rgba channels
	int noise_resolution = 16;
	unsigned char* noise_data = new unsigned char[noise_resolution * noise_resolution * noise_resolution * 4];
	//update_noise(noise_data, 0.2f, noise_resolution, 8, 64, 64, 0);

	unsigned int noise_id;
	glEnable(GL_TEXTURE_3D);
	glGenTextures(1, &noise_id);
	glActiveTexture(GL_TEXTURE0 + noise_id);
	glBindTexture(GL_TEXTURE_3D, noise_id);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8UI, noise_resolution, noise_resolution, noise_resolution, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	// edit texture
	glUseProgram(compute_program);
	int location = glGetUniformLocation(compute_program, "volume");
	if (location == -1) {
		std::cout << "bad " << std::endl;
	}
	glUniform1i(location, noise_id);
	glBindImageTexture(0, noise_id, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA);
	glDispatchCompute(noise_resolution, noise_resolution, 1);
	glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA);
	glUseProgram(0);
	glMemoryBarrier(GL_ALL_BARRIER_BITS);

	delete[] noise_data;
	main_shader->bind();
	main_shader->set1i("noise_texture", noise_id);

	// ---- work ---- //

	std::chrono::system_clock::time_point millis_start = std::chrono::system_clock::now();

	for (unsigned long long frame = 0; run; ++frame) {

		millis_start= std::chrono::system_clock::now();

		glClearColor(0.0f, 0.0f, 0.0f, 1.00f);
		glClear(GL_COLOR_BUFFER_BIT);

		glfwPollEvents();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			run = 0;
			continue;
		}

		// draw shader to screen
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		// refresh imgui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// get gui input
		ImGui::Begin("rendering");
		ImGui::SliderInt("fps", &fps, 10, 60);
		if (ImGui::Button("apply changes")) {
			millis_per_frame = 1000 / fps;
		}
		ImGui::End();
		ImGui::Begin("noise");
		ImGui::InputFloat("zoom", &zoom);
		ImGui::SliderFloat("depth", &depth, 0.0f, 1.0f);
		ImGui::End();
		ImGui::Begin("sky");
		ImGui::SliderFloat("time", &time, 6.0f, 18.0f);
		ImGui::End();
		ImGui::Begin("cloud");
		ImGui::SliderFloat("size x", &box_size_x, 0.0f, 5.0f);
		ImGui::SliderFloat("size y", &box_size_y, 0.0f, 5.0f);
		ImGui::SliderFloat("size z", &box_size_z, 0.0f, 5.0f);
		ImGui::End();
		ImGui::Begin("camera");
		ImGui::SliderFloat("pitch", &camera_pitch, 0.0f, 360.0f);
		ImGui::SliderFloat("yaw", &camera_yaw, 0.0f, 360.0f);
		ImGui::InputFloat3("camera location", &camera_location.x, 3);
		ImGui::End();

		// render gui to frame
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// calculate variables based on input
		//
		// sunrise - 6:00am
		// sunset  - 6:00pm
		float sun_y = (time - 6.0f) / 6.0f;
		float sun_z = sun_y - 1.0f;
		if (time > 12.0f) {
			sun_y = 2.0f - sun_y;
		}

		glm::mat4 view = view_matrix;
		view = glm::rotate(view, glm::radians(camera_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
		view = glm::rotate(view, glm::radians(camera_pitch), glm::vec3(1.0f, 0.0f, 0.0f));

		main_shader->set1f("zoom", zoom);
		main_shader->set1f("noise_depth", depth);
		main_shader->set3f("box_size", box_size_x, box_size_y, box_size_z);
		main_shader->set3f("sun_direction", 0.0f, sun_y, sun_z);
		main_shader->set3f("camera_location", camera_location.x, camera_location.y, camera_location.z);
		main_shader->set_mat4fv("view_matrix", view);

		// update screen with new frame
		glfwSwapBuffers(window);

		std::chrono::duration<double, std::milli> millis_ellapsed(std::chrono::system_clock::now() - millis_start);
		int millis_remaining_per_frame = millis_per_frame - millis_ellapsed.count();

		if (millis_remaining_per_frame > 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(millis_remaining_per_frame));
		}
	}

	// ---- cleanup ---- //

	glfwTerminate();
	delete main_shader;

	return 0;
}

void compute_worley_grid(glm::vec3* points, int subdivision) {
	float cell_size = 1.0f / (float)subdivision;
	for (int i = 0; i < subdivision; ++i) {
		for (int j = 0; j < subdivision; ++j) {
			for (int k = 0; k < subdivision; ++k) {
				float x = (float)rand()/(float)(RAND_MAX);
				float y = (float)rand()/(float)(RAND_MAX);
				float z = (float)rand()/(float)(RAND_MAX);
				glm::vec3 offset = glm::vec3(i, j, k) * cell_size;
				glm::vec3 corner = glm::vec3(x, y, z) * cell_size;
				points[i + subdivision * (j + k * subdivision)] = corner + offset;
			}
		}
	}
}

static const glm::ivec3 offsets[] = {
	// centre
	{0,0,0},
	// front face
	{0,0,1},
	{-1,1,1},
	{-1,0,1},
	{-1,-1,1},
	{0,1,1},
	{0,-1,1},
	{1,1,1},
	{1,0,1},
	{1,-1,1},
	// back face
	{0,0,-1},
	{-1,1,-1},
	{-1,0,-1},
	{-1,-1,-1},
	{0,1,-1},
	{0,-1,-1},
	{1,1,-1},
	{1,0,-1},
	{1,-1,-1},
	// ring around centre
	{-1,1,0},
	{-1,0,0},
	{-1,-1,0},
	{0,1,0},
	{0,-1,0},
	{1,1,0},
	{1,0,0},
	{1,-1,0}
};

inline int max_component_i3(glm::ivec3 v) {
	return glm::max(v.x, glm::max(v.y, v.z));
}

inline int min_component_i3(glm::ivec3 v) {
	return glm::min(v.x, glm::min(v.y, v.z));
}

float compute_worley_noise(glm::vec3* points, glm::vec3 position, int subdivision) {
	glm::ivec3 cell_id = glm::ivec3(std::floor(position.x * subdivision), std::floor(position.y * subdivision), std::floor(position.z * subdivision));
	float min_dist = 1.0f;
	for (int offset_index = 0; offset_index < 27; ++offset_index) {
		glm::ivec3 adj_id = cell_id + offsets[offset_index];
		// in case adj cell is outside, blend with it
		if (min_component_i3(adj_id) == -1 || max_component_i3(adj_id) == subdivision) {
			glm::ivec3 wrap_id = glm::mod(glm::vec3(adj_id + glm::ivec3(subdivision)), glm::vec3(subdivision));
			// get cell index to access point
			int adj_index = wrap_id.x + subdivision * (wrap_id.y + wrap_id.z * subdivision);
			glm::vec3 wrapped_point = points[adj_index];
			for (int wrapped_offset_index = 0; wrapped_offset_index < 27; ++wrapped_offset_index) {
				glm::vec3 offset = (position - (wrapped_point + glm::vec3(offsets[wrapped_offset_index])));
				min_dist = std::min(min_dist, glm::dot(offset, offset));
			}
		} else {
			// adj cell is inside map.
			// calculate distance from position to cell point 
			int adj_index = adj_id.x + subdivision * (adj_id.y + adj_id.z * subdivision);
			glm::vec3 offset = position - points[adj_index];
			min_dist = std::min(min_dist, glm::dot(offset, offset));
		}
	}
	return std::sqrt(min_dist);
}

void update_noise(unsigned char* data_ptr, float persistance, int axis_resolution, int subdiv_a, int subdiv_b, int subdiv_c, char channel) {
	// lay random points per each cell in
	// the grid.
	// for each layer
	glm::vec3* points_a = new glm::vec3[subdiv_a * subdiv_a * subdiv_a];
	glm::vec3* points_b = new glm::vec3[subdiv_b * subdiv_b * subdiv_b];
	glm::vec3* points_c = new glm::vec3[subdiv_c * subdiv_c * subdiv_c];
	compute_worley_grid(points_a, subdiv_a);
	compute_worley_grid(points_b, subdiv_b);
	compute_worley_grid(points_c, subdiv_c);

	// create texture.
	// compute each layer and combine them
	// for each pixel in the 3d texture
	int slice_size = axis_resolution * axis_resolution;
	for (int z = 0; z < axis_resolution; ++z) {
		std::cout << "slice" << z << std::endl;
		for (int y = 0; y < axis_resolution; ++y) {
			for (int x = 0; x < axis_resolution; ++x) {
				glm::vec3 position = glm::vec3(z, y, x) / (float)axis_resolution;
				float layer_a = compute_worley_noise(points_a, position, subdiv_a);
				float layer_b = compute_worley_noise(points_b, position, subdiv_b);
				float layer_c = compute_worley_noise(points_c, position, subdiv_c);
				// add layers
				float noise_sum = layer_a + (layer_b * persistance) + (layer_c * persistance * persistance);
				// max possible value for noise
				float max_value = 1.0f + persistance + (persistance * persistance);
				// map to (float) 0.0 - 1.0
				noise_sum /= max_value;
				// invert
				noise_sum = 1.0f - noise_sum;
				// map to (int) 0 - 255
				int result = (int)(noise_sum * 255.0f);
				// store value
				data_ptr[(z * slice_size + y * axis_resolution + x) * 4 + channel] = result;
			}
		}
	}

	// free up heap storage
	delete[] points_a;
	delete[] points_b;
	delete[] points_c;
}
