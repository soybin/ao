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

// -------- c l o u d s --------//

struct cloud {
	int altitude;
	int noise_resolution;
};

// based on
// https://www.weather.gov

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
	int cloud_volume_samples = 32;
	int cloud_in_scatter_samples = 8;
	float cloud_shadowing_threshold = 0.2f;
	float cloud_absorption = 0.2f;
	float cloud_density_threshold = 0.72f;
	float cloud_density_multiplier = 5.0f;
	float cloud_noise_main_scale = 0.1f;
	float cloud_noise_detail_scale = 5.0f;
	float cloud_noise_detail_weight = 0.1f;
	float cloud_color[3] = { 1.0f, 1.0f, 1.0f };
	float cloud_location[3] = { 0.0f, 10.0f, 0.0f };
	float cloud_volume[3] = { 20.0f, 2.0f, 20.0f };
	// wind
	float wind_direction[3] = {0.1f, 0.0f, 0.12f};
	// skydome
	bool render_sky = 1;
	float time = 15.0f; // 6:00am - 18:00am
	float background_color[3] = { 0.0f, 0.0f, 0.0f };
	// camera
	glm::vec3 camera_location = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::mat4 view_matrix = glm::lookAt(camera_location, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	float camera_pitch = 90.0f;
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
	glfwSwapInterval(10);

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


	// ---- noise ---- lowres ----- // 
	
	unsigned int lowres_noise_id;
	unsigned int lowres_noise_resolution = 32;

	glGenTextures(1, &lowres_noise_id);
	glActiveTexture(GL_TEXTURE0 + lowres_noise_id);
	glBindTexture(GL_TEXTURE_3D, lowres_noise_id);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, lowres_noise_resolution, lowres_noise_resolution, lowres_noise_resolution, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glBindImageTexture(0, lowres_noise_id, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);

	// build noise texture on compute shader
	update_noise(compute_shader, 0.6f, lowres_noise_resolution, 2, 4, 5, 'R');

	main_shader->bind();
	main_shader->set1i("detail_noise_texture", lowres_noise_id);

	glEnable(GL_TEXTURE_3D);

	// ---- noise ---- highres ---- //

	unsigned int highres_noise_id;
	unsigned int highres_noise_resolution = 512;

	glGenTextures(1, &highres_noise_id);
	glActiveTexture(GL_TEXTURE0 + highres_noise_id);
	glBindTexture(GL_TEXTURE_3D, highres_noise_id);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, highres_noise_resolution, highres_noise_resolution, highres_noise_resolution, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glBindImageTexture(0, highres_noise_id, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);

	// build noise texture on compute shader
	update_noise(compute_shader, 0.8f, highres_noise_resolution, 2, 4, 5, 'R');

	main_shader->bind();
	main_shader->set1i("main_noise_texture", highres_noise_id);

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

		// draw imgui
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// get gui input
		if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {
			if (ImGui::BeginTabItem("rendering")) {
				ImGui::SliderInt("fps", &fps, 10, 60);
				if (ImGui::Button("apply changes")) {
					millis_per_frame = 1000 / fps;
				}
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("sky")) {
				// physically accurate sky or just bg color?
				ImGui::Checkbox("render sky", &render_sky);
				if (render_sky) {
					ImGui::SliderFloat("time", &time, 6.0f, 18.0f);
				} else {
					ImGui::ColorEdit3("background color", &background_color[0]);
				}
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("cloud")) {
				ImGui::Text("basic parameters");
				ImGui::SliderFloat("absorption", &cloud_absorption, 0.0f, 1.0f);
				ImGui::SliderFloat("shadowing threshold", &cloud_shadowing_threshold, 0.0f, 1.0f);
				ImGui::ColorEdit3("color", &cloud_color[0]);
				ImGui::InputFloat3("location", &cloud_location[0]);
				ImGui::InputFloat3("volume", &cloud_volume[0]);
				ImGui::Text("advanced parameters");
				ImGui::SliderFloat("main noise scale", &cloud_noise_main_scale, 0.0f, 1.0f);
				ImGui::SliderFloat("detail noise scale", &cloud_noise_detail_scale, 1.0f, 10.0f);
				ImGui::SliderFloat("detail noise weight", &cloud_noise_detail_weight, 0.0f, 1.0f);
				ImGui::SliderInt("samples per volume ray", &cloud_volume_samples, 32, 128);
				ImGui::SliderInt("samples per in scatter light ray", &cloud_in_scatter_samples, 8, 64);
				ImGui::SliderFloat("density threshold", &cloud_density_threshold, 0.0f, 1.0f);
				ImGui::InputFloat("density multiplier", &cloud_density_multiplier);
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("wind")) {
				ImGui::InputFloat3("direction", &wind_direction[0]);
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("camera")) {
				ImGui::SliderFloat("pitch", &camera_pitch, 0.0f, 360.0f);
				ImGui::SliderFloat("yaw", &camera_yaw, 0.0f, 360.0f);
				ImGui::InputFloat3("camera location", &camera_location.x, 3);
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

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

		// compute angles
		glm::mat4 view = view_matrix;
		view = glm::rotate(view, glm::radians(camera_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
		view = glm::rotate(view, glm::radians(camera_pitch), glm::vec3(1.0f, 0.0f, 0.0f));

		// update frame counter
		main_shader->set1i("frame", frame);

		// camera
		main_shader->set3f("camera_location", camera_location.x, camera_location.y, camera_location.z);
		main_shader->set_mat4fv("view_matrix", view);

		// rendering
		main_shader->set1i("cloud_volume_samples", cloud_volume_samples);
		main_shader->set1i("cloud_in_scatter_samples", cloud_in_scatter_samples);

		// cloud
		main_shader->set1f("cloud_absorption", 1.0f - cloud_absorption);
		main_shader->set1f("cloud_shadowing_threshold", cloud_shadowing_threshold);
		main_shader->set1f("cloud_noise_main_scale", cloud_noise_main_scale);
		main_shader->set1f("cloud_noise_detail_scale", cloud_noise_detail_scale);
		main_shader->set1f("cloud_noise_detail_weight", cloud_noise_detail_weight);
		main_shader->set1f("cloud_density_threshold", cloud_density_threshold);
		main_shader->set1f("cloud_density_multiplier", cloud_density_multiplier);
		main_shader->set3f("cloud_color", cloud_color[0], cloud_color[1], cloud_color[2]);
		main_shader->set3f("cloud_location", cloud_location[0], cloud_location[1], cloud_location[2]);
		main_shader->set3f("cloud_volume", cloud_volume[0], cloud_volume[1], cloud_volume[2]);

		// wind
		main_shader->set3f("wind_direction", wind_direction[0], wind_direction[1], wind_direction[2]);

		// skydome
		main_shader->set1i("render_sky", render_sky);
		main_shader->set3f("background_color", background_color[0], background_color[1], background_color[2]);
		main_shader->set3f("sun_direction", 0.0f, sun_y, sun_z);

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

// -------------------------------- //
// -------- user interface -------- //
// -------------------------------- //

inline void update_ui() {

}

// ------------------------------- //
// -------- noise texture -------- //
// ------------------------------- //

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

	// pass random points a to shader storage buffer object
	unsigned int ssbo_a = 0;
	glGenBuffers(1, &ssbo_a);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_a);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 16 * subdivisions_a * subdivisions_a * subdivisions_a, points_a, GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo_a);
	delete[] points_a;

	// pass random points b to shader storage buffer object
	unsigned int ssbo_b = 0;
	glGenBuffers(1, &ssbo_b);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_b);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 16 * subdivisions_b * subdivisions_b * subdivisions_b, points_b, GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbo_b);
	delete[] points_b;

	// pass random points c to shader storage buffer object
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
