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
#include <vector>
#include <map>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#include "shader.h"

#define IMGUI_IMPL_OPENGL_LOADER_GLEW

static void imgui_help_marker(const char* desc);

// -------- n o i s e -------- //

/*
 * worley noise function:
 * persistance = noisy doisy
 */

void bake_noise(unsigned int &texture_id, shader* compute, int resolution, float persistance, int subdivisions_a, int subdivisions_b, int subdivisions_c);

// -------- c l o u d s --------//

const char* cloud_models[] = { "Custom", "Cumulus", "Stratocumulus", "Stratus", "Cumulonimbus", "Altomumulus", "Cirrostratus", "Cirrocumulus", "Cirrus" };
static const char* cloud_model_current = "Cumulus";

struct cloud {
	int altitude;
	int noise_resolution;
};

// based on
// https://www.weather.gov

// -------- 青 一 a o -------- //

int cursor_old_x = 0;
int cursor_old_y = 0;
int cursor_delta_x = 0;
int cursor_delta_y = 0;
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);

int main(int argc, char* argv[]) {

	// ---- init ao data ---- //

	int run = 1;
	int fullscreen = 0;
	int width = 1280;
	int height = 720;
	float cursor_sensitivity = 5.0f;
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
	float cloud_shadowing_max_distance = 5.0f;
	float cloud_shadowing_weight = 0.9f;
	float cloud_absorption = 0.1f;
	float cloud_density_threshold = 0.72f;
	float cloud_density_multiplier = 5.0f;
	float cloud_noise_main_scale = 0.1f;
	float cloud_noise_detail_scale = 5.0f;
	float cloud_noise_detail_weight = 0.1f;
	float cloud_color[3] = { 1.0f, 1.0f, 1.0f };
	float cloud_location[3] = { 0.0f, 20.0f, 0.0f };
	float cloud_volume[3] = { 30.0f, 3.0f, 30.0f };
	// wind
	float wind_direction[3] = { 0.1f, 0.0f, 0.0f };
	// skydome
	bool render_sky = 1;
	bool light_any_direction = 0;
	float time = 15.0f; // 6:00am - 18:00pm
	float light_direction[3] = { 0.0f, 0.707107f, 0.707107f };
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
	glfwSwapInterval(1);

	// set cursor position callback function up
	glfwSetCursorPosCallback(window, cursor_position_callback);
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

	// load custom theme
  ImGuiStyle& style = ImGui::GetStyle();
	style.FramePadding.x = 4.0f;
	style.FramePadding.y = 4.0f;
	style.WindowPadding.x = 10.0f;
	style.WindowPadding.y = 10.0f;
	style.GrabMinSize = 20.0f;
	style.ScrollbarSize = 20.0f;
	style.WindowBorderSize = 0.0f;
	style.FrameBorderSize = 0.0f;
	style.PopupBorderSize = 0.0f;
	style.WindowRounding = 10.0f;
	style.ChildRounding = 10.0f;
	style.FrameRounding = 10.0f;
	style.PopupRounding = 10.0f;
	style.ScrollbarRounding = 10.0f;
	style.GrabRounding = 10.0f;
	style.WindowTitleAlign.x = 0.5f;
	style.WindowTitleAlign.y = 0.5f;
	// color palette
	{
		// indexes for colors imgui:
		// 0 - text
		// 1 - text disabled
		// 2 - window bg
		// 3 - child bg
		// 4 - popup bg
		// 7 - frame bg
		// 8 - frame bg hovered
		// 9 - frame bg active
		// 10 - title bg
		// 11 - title bg active
		// 12 - title bg collapsed
		// 13 - menu bar bg
		// 14 - scrollbar bg
		// 15 - scrollbar grab
		// 16 - scrollbar grab hovered
		// 17 - scrollbar grab active
		// 18 - checkmark
		// 19 - slider grab
		// 20 - slider grab active
		// 21 - button
		// 22 - button hovered
		// 23 - button active
		// 24 - header 
		// 25 - header hovered
		// 26 - header active
		// 27 - separator
		// 28 - separator hovered
		// 29 - separator active
		// 30 - resize grip
		// 31 - resize grip hovered
		// 32 - resize grip active
		// key: color, data: imgui color indexes to apply to
		std::map<std::vector<int>, std::vector<int>> colors;
		// color palette: https://coolors.co/2b2628-f4978e-f8ad9d-fbc4ab-ffdab9
		colors[{ 43, 38, 40 }]    = { 0, 1 }; // dark - text
		colors[{ 255, 218, 185 }] = { 2, 13, 14, 18 }; // light - backgrounds - special features
		colors[{ 251, 196, 171}]  = { 3, 4, 7, 10, 15, 20, 21, 24, 27, 30 }; // a bit light - default
		colors[{ 248, 173, 157 }] = {       8, 11, 16    , 22, 25, 28, 31 }; // medium - hovered
		colors[{ 244, 151, 142}]  = {       9, 12, 17, 19, 23, 26, 29, 32 }; // a bit dark - pressed
		for (auto& [color, indices] : colors) {
			for (auto& index : indices) {
				style.Colors[index].x = (float)color[0] / 255.0f;
				style.Colors[index].y = (float)color[1] / 255.0f;
				style.Colors[index].z = (float)color[2] / 255.0f;
				style.Colors[index].w = 0.95f;
			}
		}
	}

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

	// ---- noise ---- //
	glEnable(GL_TEXTURE_3D);

	// main
	unsigned int main_noise_id;
	int main_noise_resolution = 128;
	int main_noise_subdivisions_a = 3;
	int main_noise_subdivisions_b = 6;
	int main_noise_subdivisions_c = 9;
	float main_noise_persistence = 0.8f;
	bake_noise(main_noise_id, compute_shader, main_noise_resolution, main_noise_persistence, main_noise_subdivisions_a, main_noise_subdivisions_b, main_noise_subdivisions_c);
	main_shader->bind();
	main_shader->set1i("main_noise_texture", main_noise_id);

	// detail
	unsigned int detail_noise_id;
	int detail_noise_resolution = 32;
	int detail_noise_subdivisions_a = 2;
	int detail_noise_subdivisions_b = 4;
	int detail_noise_subdivisions_c = 6;
	float detail_noise_persistence = 0.6f;
	bake_noise(detail_noise_id, compute_shader, detail_noise_resolution, detail_noise_persistence, detail_noise_subdivisions_a, detail_noise_subdivisions_b, detail_noise_subdivisions_c);
	main_shader->bind();
	main_shader->set1i("detail_noise_texture", detail_noise_id);

	// ---- work ---- //

	std::chrono::system_clock::time_point millis_start = std::chrono::system_clock::now();

	for (unsigned long long frame = 0; run; ++frame) {

		millis_start = std::chrono::system_clock::now();

		glClearColor(0.0f, 0.0f, 0.0f, 1.00f);
		glClear(GL_COLOR_BUFFER_BIT);

		glfwPollEvents();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			run = 0;
			continue;
		}

		// draw fragment to screen if required to.
		// otherwise, draw last fragment
		if (frame % 1 == 0) {
			glBindVertexArray(vao);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		// --------------- //
		// ---- imgui ---- //
		// --------------- //

		// check for ever rendered window
		bool imgui_window_is_focused = false;

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::ShowDemoWindow();

		// ---- clouds ---- //

		ImGui::Begin("clouds", NULL, 0);
		ImGui::Text("cloud model preset");
		if (ImGui::BeginCombo("##cloud model", cloud_model_current)) {
			for (int n = 0; n < IM_ARRAYSIZE(cloud_models); ++n) {
				bool is_selected = (cloud_model_current == cloud_models[n]);
				if (ImGui::Selectable(cloud_models[n], is_selected)) {
					cloud_model_current = cloud_models[n];
				}
				if (is_selected) {
					ImGui::SetItemDefaultFocus();	
				}
			}
			ImGui::EndCombo();
		}
		ImGui::Separator();
		ImGui::Text("basic properties");
		ImGui::Separator();
		ImGui::SliderFloat("absorption", &cloud_absorption, 0.0f, 1.0f);
		ImGui::ColorEdit3("color", &cloud_color[0]);
		ImGui::InputFloat3("location", &cloud_location[0]);
		ImGui::InputFloat3("volume", &cloud_volume[0]);
		ImGui::Separator();
		ImGui::Text("shadowing");
		ImGui::Separator();
		ImGui::SliderFloat("weight", &cloud_shadowing_weight, 0.0f, 1.0f);
		ImGui::InputFloat("maximum distance", &cloud_shadowing_max_distance);
		ImGui::Separator();
		ImGui::Text("rendering");
		ImGui::Separator();
		ImGui::SliderFloat("main noise scale", &cloud_noise_main_scale, 0.0f, 0.512f);
		ImGui::SliderFloat("detail noise scale", &cloud_noise_detail_scale, 1.6f, 10.0f);
		ImGui::SliderFloat("detail noise weight", &cloud_noise_detail_weight, 0.0f, 1.0f);
		ImGui::SliderInt("volume samples", &cloud_volume_samples, 32, 128);
		ImGui::SliderInt("in-scatter samples", &cloud_in_scatter_samples, 8, 64);
		ImGui::SliderFloat("density threshold", &cloud_density_threshold, 0.0f, 1.0f);
		ImGui::InputFloat("density multiplier", &cloud_density_multiplier);
		if (ImGui::CollapsingHeader("noise")) {
			ImGui::Text("main noise");
			ImGui::InputInt("resolution(% 8 == 0)##1", &main_noise_resolution);
			ImGui::InputInt("first layer subdivisions##1", &main_noise_subdivisions_a);
			ImGui::InputInt("second layer subdivisions##1", &main_noise_subdivisions_b);
			ImGui::InputInt("third layer subdivisions##1", &main_noise_subdivisions_c);
			ImGui::SliderFloat("layer mix persistence##1", &main_noise_persistence, 0.0f, 1.0f);
			if (ImGui::Button("rebake##1")) {
				main_shader->unbind();
				bake_noise(main_noise_id, compute_shader, main_noise_resolution, main_noise_persistence, main_noise_subdivisions_a, main_noise_subdivisions_b, main_noise_subdivisions_c);
				main_shader->bind();
				main_shader->set1i("main_noise_texture", main_noise_id);
			}
			ImGui::Separator();
			ImGui::Text("detail noise");
			ImGui::InputInt("resolution (% 8 == 0)##2", &detail_noise_resolution);
			ImGui::InputInt("first layer subdivisions##2", &detail_noise_subdivisions_a);
			ImGui::InputInt("second layer subdivisions##2", &detail_noise_subdivisions_b);
			ImGui::InputInt("third layer subdivisions##2", &detail_noise_subdivisions_c);
			ImGui::SliderFloat("layer mix persistence##2", &detail_noise_persistence, 0.0f, 1.0f);
			if (ImGui::Button("rebake##2")) {
				main_shader->unbind();
				bake_noise(detail_noise_id, compute_shader, detail_noise_resolution, detail_noise_persistence, detail_noise_subdivisions_a, detail_noise_subdivisions_b, detail_noise_subdivisions_c);
				main_shader->bind();
				main_shader->set1i("detail_noise_texture", detail_noise_id);
			}
		}

		imgui_window_is_focused |= ImGui::IsWindowFocused();
		ImGui::End();

		// ---- sky ----//

		ImGui::Begin("atmosphere", NULL, 0);
		ImGui::Checkbox("physically accurate atmosphere", &render_sky);
		ImGui::Separator();
		ImGui::Text("light direction");
		ImGui::Checkbox("any direction", &light_any_direction); ImGui::SameLine();
		imgui_help_marker("you can modify the exact light direction or\n"
											"you can set the time of the day to resemble\n"
											"the light's angle at that time.");
		bool light_direction_modified = false;
		if (light_any_direction) {
			light_direction_modified |= ImGui::SliderFloat("x", &light_direction[0], -1.0f, 1.0f);
			light_direction_modified |= ImGui::SliderFloat("y", &light_direction[1], -1.0f, 1.0f);
			light_direction_modified |= ImGui::SliderFloat("z", &light_direction[2], -1.0f, 1.0f);
		} else {
			if (ImGui::SliderFloat("time", &time, 6.0f, 18.0f)) {
				// sunrise - 6:00am
				// sunset  - 6:00pm
				light_direction[0] = 0.0f;
				light_direction[1] = (time - 6.0f) / 6.0f;
				light_direction[2] = light_direction[1] - 1.0f;
				if (time > 12.0f) {
					light_direction[1] = 2.0f - light_direction[1];
				}
				light_direction_modified = true;
			}
		}
		// normalize light direction and update shader
		if (light_direction_modified) {
			// normalize light direction and update shader
			float light_direction_module = std::sqrt(light_direction[0] * light_direction[0] + light_direction[1] * light_direction[1] + light_direction[2] * light_direction[2]);
			light_direction[0] /= light_direction_module;
			light_direction[1] /= light_direction_module;
			light_direction[2] /= light_direction_module;
		}
		if (render_sky) {
		} else {
			ImGui::ColorEdit3("background color", &background_color[0]);
		}

		imgui_window_is_focused |= ImGui::IsWindowFocused();
		ImGui::End();

		// ---- settings ---- //

		if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {
			if (ImGui::BeginTabItem("rendering")) {
				ImGui::SliderInt("fps", &fps, 10, 60);
				if (ImGui::Button("apply changes")) {
					millis_per_frame = 1000 / fps;
				}
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("cloud")) {
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
		imgui_window_is_focused |= ImGui::IsWindowFocused();

		// check for mouse drag input (don't move this block of code out of imgui rendering scope)
		if (!imgui_window_is_focused && (cursor_delta_x || cursor_delta_y)) {
			camera_yaw += (float)cursor_delta_x * cursor_sensitivity / 10.0f;
			camera_pitch += (float)cursor_delta_y * cursor_sensitivity / 10.0f;
			// make sure there's no excess rotation
			if (camera_yaw > 360.0f) {
				camera_yaw -= 360.0f;
			} else if (camera_yaw < 0.0f) {
				camera_yaw += 360.0f;
			}
			if (camera_pitch > 360.0f) {
				camera_pitch -= 360.0f;
			} else if (camera_pitch < 0.0f) {
				camera_pitch += 360.0f;
			}
			cursor_delta_x = 0;
			cursor_delta_y = 0;
		}

		// render gui to frame
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

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
		main_shader->set1f("cloud_shadowing_max_distance", cloud_shadowing_max_distance);
		main_shader->set1f("cloud_shadowing_weight", cloud_shadowing_weight);
		main_shader->set1f("cloud_noise_main_scale", cloud_noise_main_scale);
		main_shader->set1f("cloud_noise_detail_scale", cloud_noise_detail_scale);
		main_shader->set1f("cloud_noise_detail_weight", cloud_noise_detail_weight);
		main_shader->set1f("cloud_density_threshold", cloud_density_threshold);
		main_shader->set1f("cloud_density_multiplier", cloud_density_multiplier);
		main_shader->set3f("cloud_color", cloud_color[0], cloud_color[1], cloud_color[2]);
		main_shader->set3f("cloud_location", cloud_location[0], cloud_location[1], cloud_location[2]);
		main_shader->set3f("cloud_volume", cloud_volume[0] / 2.0f, cloud_volume[1] / 2.0f, cloud_volume[2] / 2.0f);

		// wind
		main_shader->set3f("wind_direction", wind_direction[0], wind_direction[1], wind_direction[2]);

		// skydome
		main_shader->set1i("render_sky", render_sky);
		main_shader->set3f("background_color", background_color[0], background_color[1], background_color[2]);
		main_shader->set3f("light_direction", light_direction[0], light_direction[1], light_direction[2]);

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

	delete compute_shader;
	delete main_shader;

	return 0;
}

// cursor logic
void cursor_position_callback(GLFWwindow* window, double x, double y) {
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
		cursor_delta_x = (int)x - cursor_old_x;
		cursor_delta_y = (int)y - cursor_old_y;
	}
	cursor_old_x = (int)x;
	cursor_old_y = (int)y;
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

void bake_noise(unsigned int &texture_id, shader* compute, int resolution, float persistance, int subdivisions_a, int subdivisions_b, int subdivisions_c) {
	// first time generating texture
	if (glIsTexture(texture_id)) {
		glDeleteTextures(1, &texture_id);
		std::cout << "[+] baking new noise texture" << std::endl;
	}
	glGenTextures(1, &texture_id);
	glActiveTexture(GL_TEXTURE0 + texture_id);
	glBindTexture(GL_TEXTURE_3D, texture_id);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, resolution, resolution, resolution, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
	glBindImageTexture(0, texture_id, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8);

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

	// delete buffers
	glDeleteBuffers(1, &ssbo_a);
	glDeleteBuffers(1, &ssbo_b);
	glDeleteBuffers(1, &ssbo_c);
}


static void imgui_help_marker(const char* desc) {
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}
