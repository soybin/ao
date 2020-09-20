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
 * persistance = noisy doisy
 */
void update_noise(shader* compute, float persistance, int resolution, int subdivisions_a, int subdivisions_b, int subdivisions_c, char channel);

// -------- 青 一 a o -------- //

int main(int argc, char* argv[]) {

	// ---- init ao data ---- //

	int run = 1;
	int fullscreen = 0;
	int width = 1280;
	int height = 720;
	unsigned int vao;
	unsigned int vbo;
	shader* compute_shader;
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

	// ---- init shaders ---- //

	// compute shader
	compute_shader = new shader("./compute.glsl");
	// normal shader
	main_shader = new shader("./vertex.glsl", "./fragment.glsl");
	main_shader->bind();
	main_shader->set2f("resolution", width, height);
	main_shader->unbind();

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

	// ---- noise ---- texture ---- //

	int noise_resolution = 128;
	float zoom = 1.0f;
	float depth = 0.0f;

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
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, noise_resolution, noise_resolution, noise_resolution, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glBindImageTexture(0, 1, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);

	// build noise texture on compute shader
	update_noise(compute_shader, 0.8f, noise_resolution, 4, 6, 8, 'R');

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
	//	main_shader->set3f("box_size", box_size_x, box_size_y, box_size_z);
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

	delete compute_shader;;
	delete main_shader;

	return 0;
}

void compute_worley_grid(glm::vec4* points, int subdivision) {
	srand(time(0));
	float cell_size = 1.0f / (float)subdivision;
	for (int i = 0; i < subdivision; ++i) {
		for (int j = 0; j < subdivision; ++j) {
			for (int k = 0; k < subdivision; ++k) {
				float x = (float)rand()/(float)(RAND_MAX);
				float y = (float)rand()/(float)(RAND_MAX);
				float z = (float)rand()/(float)(RAND_MAX);
				glm::vec3 offset = glm::vec3(i, j, k) * cell_size;
				glm::vec3 corner = glm::vec3(x, y, z) * cell_size;
				points[i + subdivision * (j + k * subdivision)] = glm::vec4(offset + corner, 0.0f);
			}
		}
	}
}

void update_noise(shader* compute, float persistance, int resolution, int subdivisions_a, int subdivisions_b, int subdivisions_c, char channel) {
	// lay random points per each cell in
	// the grid.
	// for each layer
	glm::vec4* points_a = new glm::vec4[subdivisions_a * subdivisions_a * subdivisions_a];
	glm::vec4* points_b = new glm::vec4[subdivisions_b * subdivisions_b * subdivisions_b];
	glm::vec4* points_c = new glm::vec4[subdivisions_c * subdivisions_c * subdivisions_c];
	compute_worley_grid(points_a, subdivisions_a);
	compute_worley_grid(points_b, subdivisions_b);
	compute_worley_grid(points_c, subdivisions_c);

	// set shader variables
	compute->bind();
	compute->set1i("output_texture", 0);
	compute->set1i("resolution", resolution);
	compute->set1f("persistance", persistance);
	compute->set4i("channel_mask", channel == 'R', channel == 'G', channel == 'B', channel == 'A');
	compute->set1i("subdivisions_a", subdivisions_a);
	compute->set1i("subdivisions_b", subdivisions_b);
	compute->set1i("subdivisions_c", subdivisions_c);

	// pass random points - a
	unsigned int ssbo_a = 0;
	glGenBuffers(1, &ssbo_a);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_a);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 16 * subdivisions_a * subdivisions_a * subdivisions_a, points_a, GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo_a);
	delete[] points_a;

	// pass random points - b
	unsigned int ssbo_b = 0;
	glGenBuffers(1, &ssbo_b);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_b);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 16 * subdivisions_b * subdivisions_b * subdivisions_b, points_b, GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbo_b);
	delete[] points_b;

	// pass random points - c
	unsigned int ssbo_c = 0;
	glGenBuffers(1, &ssbo_c);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_c);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 16 * subdivisions_c * subdivisions_c * subdivisions_c, points_c, GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbo_c);
	delete[] points_c;
	
	// dispatch compute shader
	glDispatchCompute(resolution / 8, resolution / 8, resolution / 8);

	// wait till finished
	glMemoryBarrier(GL_ALL_BARRIER_BITS);

}
