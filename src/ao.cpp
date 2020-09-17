/*
 * MIT License
 * Copyright (c) 2020 Pablo Peñarroja
 */

#include <stdio.h>
#include <stdlib.h>

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

void build_worley_perlin_texture(unsigned char* data_ptr, int width, int height, int depth);

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

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
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
	ImGui_ImplOpenGL3_Init("#version 420");

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

	// volume texture - 3d - rgba channels
	int noise_width, noise_height, noise_depth, noise_size;
	noise_width = noise_height = noise_depth = 256;
	noise_size = noise_width * noise_height;
	unsigned char* noise_data = new unsigned char[noise_width * noise_height * noise_depth * 4];
	build_worley_perlin_texture(noise_data, noise_width, noise_height, noise_depth);

	glEnable(GL_TEXTURE_3D);
	unsigned int noise_id;
	glGenTextures(1, &noise_id);
	glBindTexture(GL_TEXTURE_3D, noise_id);
	glActiveTexture(GL_TEXTURE0 + noise_id);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, noise_width, noise_height, noise_depth, 0, GL_RGBA, GL_UNSIGNED_BYTE, &noise_data[0]);

	delete[] noise_data;
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

void build_worley_perlin_texture(unsigned char* data_ptr, int width, int height, int depth) {
	int slice_size = width * height;
	for (int z = 0; z < depth; ++z) {
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				data_ptr[(z * slice_size + y * width + x) * 4 + 0] = rand() % 255;
				data_ptr[(z * slice_size + y * width + x) * 4 + 1] = rand() % 255;
				data_ptr[(z * slice_size + y * width + x) * 4 + 2] = rand() % 255;
				data_ptr[(z * slice_size + y * width + x) * 4 + 3] = rand() % 25;
			}
		}
	}
}
