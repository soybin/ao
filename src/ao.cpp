/*
 * MIT License
 * Copyright (c) 2020 Pablo Peñarroja
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <cstdio>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#include "shader.h"

#define IMGUI_IMPL_OPENGL_LOADER_GLEW

//-------- s h a d e r --------//

void init_shader() {
	
}

//-------- 青 一 a o --------//

int main(int argc, char* argv[]) {

	//---- init ao data ----//

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
	float sun_direction_x = 0.0f;
	float sun_direction_y = 0.5f;
	// camera
	float camera_pitch = 0.0f;
	float camera_roll = 0.0f;
	float camera_yaw = 0.0f;

	//---- init glfw ----//

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

	//---- init glew ----//

	if (glewInit() != GLEW_OK) {
		glfwTerminate();
		std::cout << "[-] GLEW initialization failed. Exiting ao." << std::endl;
		return 1;
	}

	//---- init shader ----//

	init_shader();

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

	//---- load quad ----//

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

	//---- work ----//

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
		ImGui::Begin("skybox");
		ImGui::SliderFloat("direction x", &sun_direction_x, -1.0f, 1.0f);
		ImGui::SliderFloat("direction y", &sun_direction_y, -1.0f, 1.0f);
		ImGui::End();
		ImGui::Begin("cloud");
		ImGui::SliderFloat("size x", &box_size_x, 0.0f, 5.0f);
		ImGui::SliderFloat("size y", &box_size_y, 0.0f, 5.0f);
		ImGui::SliderFloat("size z", &box_size_z, 0.0f, 5.0f);
		ImGui::End();
		ImGui::Begin("camera");
		ImGui::SliderFloat("pitch", &camera_pitch, 0.0f, 1.0f);
		ImGui::SliderFloat("yaw", &camera_yaw, 0.0f, 1.0f);
		ImGui::SliderFloat("roll", &camera_roll, 0.0f, 1.0f);
		ImGui::End();

		// render gui to frame
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		main_shader->set3f("box_size", box_size_x, box_size_y, box_size_z);
		main_shader->set3f("sun_direction", sun_direction_x, sun_direction_y, -1.0f);
		main_shader->set3f("camera_angle", camera_yaw * M_PI * 2.0f, camera_roll * M_PI * 2.0f, camera_pitch * M_PI * 2.0f);

		// update screen with new frame
		glfwSwapBuffers(window);

		std::chrono::duration<double, std::milli> millis_ellapsed(std::chrono::system_clock::now() - millis_start);
		int millis_remaining_per_frame = millis_per_frame - millis_ellapsed.count();

		if (millis_remaining_per_frame > 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(millis_remaining_per_frame));
		}
	}

	//---- cleanup ----//

	glfwTerminate();
	delete main_shader;

	return 0;
}

