/*
 * MIT License
 * Copyright (c) 2020 Pablo Peñarroja Millán
 */

/* ~~~~~~~~~~~~~~~~~ 青 ~~~~~~~~~~~~~~~~~ */

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

#include <opencv2/videoio.hpp>
#include <opencv2/opencv.hpp>

#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"

#include "shader.h"

#include "program_data.h"

#define IMGUI_IMPL_OPENGL_LOADER_GLEW

static void imgui_help_marker(const char* desc, bool warning = false);

// -------- n o i s e -------- //

/*
 * worley noise function:
 * persistance = noisy doisy
 */

void bake_noise_main(unsigned int &texture_id, shader* compute, int resolution, float persistance, int subdivisions_a, int subdivisions_b, int subdivisions_c);

void bake_noise_weather(unsigned int &texture_id, shader* compute, int resolution, float persistance, int subdivisions_a, int subdivisions_b, int subdivisions_c);

// -------- c l o u d s --------//

const char* cloud_models[] = { "cumulus", "stratocumulus", "stratus", "altocumulus", };
static const char* cloud_model_current = "cumulus";

struct cloud {
	// cloud
	float cloud_absorption;
	float cloud_density_threshold;
	float cloud_density_multiplier;
	float cloud_location[3];
	float cloud_volume[3];
	// noise - main
	int noise_main_subdivisions_a;
	int noise_main_subdivisions_b;
	int noise_main_subdivisions_c;
	float noise_main_persistence;
	float noise_main_scale;
	// noise - weather
	int noise_weather_subdivisions_a;
	int noise_weather_subdivisions_b;
	int noise_weather_subdivisions_c;
	float noise_weather_persistence;
	float noise_weather_scale;
	// noise - detail
	int noise_detail_subdivisions_a;
	int noise_detail_subdivisions_b;
	int noise_detail_subdivisions_c;
	float noise_detail_persistence;
	float noise_detail_scale;
	float noise_detail_weight;
};

cloud clouds[] = {
	// ---- cumulus ---- //
	{
		0.92f, // absorption
		0.309f, // threshold
		8.0f, // density multiplier
		{ 0.0f, 100.0f, 0.0f }, // location
		{ 100.0f, 10.0f, 100.0f }, // volume
		// main noise
		4,
		16,
		64,
		1.0f,
		64.0f,
		// weather noise
		4,
		32,
		128,
		0.72f,
		128.0f,
		// detail noise
		3,
		6,
		9,
		1.0f,
		12.0f,
		0.144f // weight
	},
	// ---- stratocumulus ---- // 
	{
		0.9f, // absorption
		0.32f, // threshold
		4.0f, // density multiplier
		{ 0.0f, 105.0f, 0.0f }, // location
		{ 100.0f, 5.0f, 100.0f }, // volume
		// main noise
		8,
		32,
		128,
		0.6f,
		32.0f,
		// weather noise
		8,
		16,
		32,
		0.8f,
		64.0f,
		// detail noise
		3,
		6,
		9,
		1.0f,
		12.0f,
		0.14f // weight
	},
	// ---- stratus ---- //
	{
		0.7f, // absorption
		0.128f, // threshold
		4.0f, // density multiplier
		{ 0.0f, 100.0f, 0.0f }, // location
		{ 100.0f, 3.0f, 100.0f }, // volume
		// main noise
		4,
		16,
		64,
		1.0f,
		64.0f,
		// weather noise
		3,
		9,
		27,
		1.0f,
		128.0f,
		// detail noise
		3,
		6,
		9,
		1.0f,
		32.0f,
		0.256f // weight
	},
	// ---- altocumulus ---- //
	{
		0.8f, // absorption
		0.316f, // threshold
		4.0f, // density multiplier
		{ 0.0f, 100.0f, 0.0f }, // location
		{ 100.0f, 4.0f, 100.0f }, // volume
		// main noise
		16,
		24,
		32,
		1.0f,
		48.0f,
		// weather noise
		12,
		24,
		36,
		0.5f,
		64.0f,
		// detail noise
		3,
		6,
		9,
		1.0f,
		12.0f,
		0.256f // weight
	}
	// -> must implement different kind of noise for these.
	// ---- cirrocumulus ---- //
	// ---- cirrus ---- //
};

// -------- i n p u t -------- //

int cursor_old_x = 0;
int cursor_old_y = 0;
int cursor_delta_x = 0;
int cursor_delta_y = 0;
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);

// -------- 青 一 a o -------- //

int main(int argc, char* argv[]) {

	// ---- init ao data ---- //

	bool run = 1;
	bool fullscreen = 0;
	int resolution[2] = { 1280, 720 };
	float cursor_sensitivity = 5.0f;
	unsigned int vao;
	unsigned int vbo;
	shader* compute_shader_main;
	shader* compute_shader_weather;
	shader* main_shader;
	GLFWwindow* window;
	// export
	bool recording = 0;
	int recording_fps = 60;
	unsigned long long recording_start_frame;
	cv::VideoWriter recording_output;
	std::chrono::system_clock::time_point recording_timer;
	// clouds
	float cloud_absorption;
	float cloud_density_threshold;
	float cloud_density_multiplier;
	float cloud_volume_edge_fade_distance = 8.0f;
	float cloud_location[3];
	float cloud_volume[3];
	// noise - main
	int noise_main_resolution = 256;
	int noise_main_subdivisions_a;
	int noise_main_subdivisions_b;
	int noise_main_subdivisions_c;
	float noise_main_persistence;
	float noise_main_scale;
	float noise_main_offset[3] = { 0.0f, 0.0f, 0.0f };
	// noise - weather
	int noise_weather_resolution = 2048;
	int noise_weather_subdivisions_a;
	int noise_weather_subdivisions_b;
	int noise_weather_subdivisions_c;
	float noise_weather_persistence;
	float noise_weather_scale;
	float noise_weather_offset[2] = { 0.0f, 0.0f };
	// noise - detail
	int noise_detail_resolution = 128;
	int noise_detail_subdivisions_a;
	int noise_detail_subdivisions_b;
	int noise_detail_subdivisions_c;
	float noise_detail_persistence;
	float noise_detail_scale;
	float noise_detail_weight;
	float noise_detail_offset[3] = { 0.0f, 0.0f, 0.0f };

	// load model
	{
		int load = 0;
		cloud model = clouds[load];
		cloud_absorption = model.cloud_absorption;
		cloud_density_threshold = model.cloud_density_threshold;
		cloud_density_multiplier = model.cloud_density_multiplier;
		cloud_location[0] = model.cloud_location[0];
		cloud_location[1] = model.cloud_location[1];
		cloud_location[2] = model.cloud_location[2];
		cloud_volume[0] = model.cloud_volume[0];
		cloud_volume[1] = model.cloud_volume[1];
		cloud_volume[2] = model.cloud_volume[2];
		noise_main_subdivisions_a = model.noise_main_subdivisions_a;
		noise_main_subdivisions_b = model.noise_main_subdivisions_b;
		noise_main_subdivisions_c = model.noise_main_subdivisions_c;
		noise_main_persistence = model.noise_main_persistence;
		noise_main_scale = model.noise_main_scale;
		noise_weather_subdivisions_a = model.noise_weather_subdivisions_a;
		noise_weather_subdivisions_b = model.noise_weather_subdivisions_b;
		noise_weather_subdivisions_c = model.noise_weather_subdivisions_c;
		noise_weather_persistence = model.noise_weather_persistence;
		noise_weather_scale = model.noise_weather_scale;
		noise_detail_subdivisions_a = model.noise_detail_subdivisions_a;
		noise_detail_subdivisions_b = model.noise_detail_subdivisions_b;
		noise_detail_subdivisions_c = model.noise_detail_subdivisions_c;
		noise_detail_persistence = model.noise_detail_persistence;
		noise_detail_scale = model.noise_detail_scale;
		noise_detail_weight = model.noise_detail_weight;
	}

	// wind
	float wind_direction[3];
	{
		srand(time(0));
		// set random direction
		float x = (float)rand()/(float)(RAND_MAX);
		if (rand() % 2 == 0) {
			x = -x;
		}
		//srand(time(0) + 1);
		float y = (float)rand()/(float)(RAND_MAX);
		if (rand() % 2 == 0) {
			y = -y;
		}
		//srand(time(0) + 2);
		float z = (float)rand()/(float)(RAND_MAX);
		if (rand() % 2 == 0) {
			z = -z;
		}
		wind_direction[0] = x;
		wind_direction[1] = y;
		wind_direction[2] = z;
	}
	float wind_speed = 1.0f;
	float wind_main_weight = 1.0f;
	float wind_weather_weight = 1.0f;
	float wind_detail_weight = 1.0f;
	// skydome
	bool render_sky = 1;
	bool light_any_direction = 0;
	float time = 15.0f; // 6:00am - 18:00pm
	float light_direction[3];
	float inverse_light_direction[3];
	{
		// set light direction
		float yz = std::sqrt(2.0f) / 2.0f;
		light_direction[0] = 0.0f;
		light_direction[1] = yz;
		light_direction[2] = yz;
		inverse_light_direction[0] = 1.0f;
		inverse_light_direction[1] = 1.0f / light_direction[1];
		inverse_light_direction[2] = 1.0f / light_direction[2];
	}
	float background_color[3] = { 0.0f, 0.0f, 0.0f };
	// camera
	glm::vec3 camera_location = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::mat4 view_matrix = glm::lookAt(camera_location, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	float camera_pitch = 90.0f;
	float camera_yaw = 0.0f;
	// rendering
	int fps = 60;
	int millis_per_frame = 1000 / fps;
	float last_fps = fps;
	int render_volume_samples = 32;
	int render_in_scatter_samples = 8;
	float render_shadowing_max_distance = 8.0f;
	float render_shadowing_weight = 0.64;
	// export


	// ---- init glfw ---- //

	if (!glfwInit()) {
		std::cout << "[-] GLFW initialization failed. Exiting ao." << std::endl;
		return 1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(resolution[0], resolution[1], "glsl", 0, 0);

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
	
	// allocate shader data in memory
	program_data* data = new program_data();

	// compute shaders
	compute_shader_main = new shader(data->compute_main, false);
	compute_shader_weather = new shader(data->compute_weather, false);

	// normal shader
	std::cout << "what";
	main_shader = new shader(data->vertex, data->fragment, false);
	main_shader->bind();
	main_shader->set3f("light_direction", light_direction[0], light_direction[1], light_direction[2]);
	main_shader->set3f("inverse_light_direction", inverse_light_direction[0], inverse_light_direction[1], inverse_light_direction[2]);
	main_shader->set2f("resolution", resolution[0], resolution[1]);
	main_shader->set1i("render_volume_samples", render_volume_samples);
	main_shader->set1i("render_in_scatter_samples", render_in_scatter_samples);
	main_shader->unbind();

	// deallocate shader data from memory
	delete data;

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
		for (auto& color_index : colors) {
			for (auto& index : color_index.second) {
				style.Colors[index].x = (float)color_index.first[0] / 255.0f;
				style.Colors[index].y = (float)color_index.first[1] / 255.0f;
				style.Colors[index].z = (float)color_index.first[2] / 255.0f;
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
	unsigned int noise_main_id;
	bake_noise_main(noise_main_id, compute_shader_main, noise_main_resolution, noise_main_persistence, noise_main_subdivisions_a, noise_main_subdivisions_b, noise_main_subdivisions_c);
	main_shader->bind();
	main_shader->set1i("noise_main_texture", noise_main_id);
	main_shader->unbind();

	// weather
	unsigned int noise_weather_id;
	bake_noise_weather(noise_weather_id, compute_shader_weather, noise_weather_resolution, noise_weather_persistence, noise_weather_subdivisions_a, noise_weather_subdivisions_b, noise_weather_subdivisions_c);
	main_shader->bind();
	main_shader->set1i("noise_weather_texture", noise_weather_id);
	main_shader->unbind();

	// detail
	unsigned int noise_detail_id;
	bake_noise_main(noise_detail_id, compute_shader_main, noise_detail_resolution, noise_detail_persistence, noise_detail_subdivisions_a, noise_detail_subdivisions_b, noise_detail_subdivisions_c);
	main_shader->bind();
	main_shader->set1i("noise_detail_texture", noise_detail_id);
	main_shader->unbind();

	// ---- work ---- //

	std::chrono::system_clock::time_point millis_start = std::chrono::system_clock::now();

	for (unsigned long long frame = 0; run; ++frame) {

		millis_start = std::chrono::system_clock::now();

		glClearColor(0.0f, 0.0f, 0.0f, 1.00f);
		glClear(GL_COLOR_BUFFER_BIT);

		glfwPollEvents();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			run = false;
			continue;
		}

		// draw fragment to screen if required to.
		// otherwise, draw last fragment
		if (frame % 1 == 0) {
			main_shader->bind();
			glBindVertexArray(vao);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		// write to video buffer if the user is recording
		if (recording) {
			cv::Mat pixels(resolution[1], resolution[0], CV_8UC3);
			glReadPixels(0, 0, resolution[0], resolution[1], GL_RGB, GL_UNSIGNED_BYTE, pixels.data);
			cv::Mat cv_pixels(resolution[1], resolution[0], CV_8UC3);
			for( int y = 0; y < resolution[1]; ++y) {
				for(int x = 0; x < resolution[0]; ++x) {
					cv_pixels.at<cv::Vec3b>(y, x)[2] = pixels.at<cv::Vec3b>(resolution[1] - y - 1, x)[0];
					cv_pixels.at<cv::Vec3b>(y, x)[1] = pixels.at<cv::Vec3b>(resolution[1] - y - 1, x)[1];
					cv_pixels.at<cv::Vec3b>(y, x)[0] = pixels.at<cv::Vec3b>(resolution[1] - y - 1, x)[2];
				}
			}
			recording_output << cv_pixels;
		}

		// --------------- //
		// ---- imgui ---- //
		// --------------- //

		// check for ever rendered window
		bool imgui_window_is_focused = false;

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// ImGui::ShowDemoWindow();

		// ---- clouds ---- //

		ImGui::Begin("ao", NULL, 0);
		ImGui::PushItemWidth(-105);
		ImGui::Text("preset: "); ImGui::SameLine();
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
		ImGui::SameLine();
		if (ImGui::Button("load")) {
			int i = 0;
			for (; i < IM_ARRAYSIZE(cloud_models); ++i) {
				if (cloud_model_current == cloud_models[i]) {
					break;
				}
			}
			cloud model = clouds[i];
			cloud_absorption = model.cloud_absorption;
			cloud_density_threshold = model.cloud_density_threshold;
			cloud_density_multiplier = model.cloud_density_multiplier;
			cloud_location[0] = model.cloud_location[0];
			cloud_location[1] = model.cloud_location[1];
			cloud_location[2] = model.cloud_location[2];
			cloud_volume[0] = model.cloud_volume[0];
			cloud_volume[1] = model.cloud_volume[1];
			cloud_volume[2] = model.cloud_volume[2];
			noise_main_subdivisions_a = model.noise_main_subdivisions_a;
			noise_main_subdivisions_b = model.noise_main_subdivisions_b;
			noise_main_subdivisions_c = model.noise_main_subdivisions_c;
			noise_main_persistence = model.noise_main_persistence;
			noise_main_scale = model.noise_main_scale;
			noise_weather_subdivisions_a = model.noise_weather_subdivisions_a;
			noise_weather_subdivisions_b = model.noise_weather_subdivisions_b;
			noise_weather_subdivisions_c = model.noise_weather_subdivisions_c;
			noise_weather_persistence = model.noise_weather_persistence;
			noise_weather_scale = model.noise_weather_scale;
			noise_detail_subdivisions_a = model.noise_detail_subdivisions_a;
			noise_detail_subdivisions_b = model.noise_detail_subdivisions_b;
			noise_detail_subdivisions_c = model.noise_detail_subdivisions_c;
			noise_detail_persistence = model.noise_detail_persistence;
			noise_detail_scale = model.noise_detail_scale;
			noise_detail_weight = model.noise_detail_weight;
			main_shader->unbind();
			bake_noise_main(noise_main_id, compute_shader_main, noise_main_resolution, noise_main_persistence, noise_main_subdivisions_a, noise_main_subdivisions_b, noise_main_subdivisions_c);
			bake_noise_weather(noise_weather_id, compute_shader_weather, noise_weather_resolution, noise_weather_persistence, noise_weather_subdivisions_a, noise_weather_subdivisions_b, noise_weather_subdivisions_c);
			bake_noise_main(noise_detail_id, compute_shader_main, noise_detail_resolution, noise_detail_persistence, noise_detail_subdivisions_a, noise_detail_subdivisions_b, noise_detail_subdivisions_c);
			main_shader->bind();
			main_shader->set1i("noise_main_texture", noise_main_id);
			main_shader->set1i("noise_weather_texture", noise_weather_id);
			main_shader->set1i("noise_detail_texture", noise_detail_id);
			main_shader->unbind();
		}
		if (ImGui::CollapsingHeader("cloud")) {
			ImGui::InputFloat3("volume", &cloud_volume[0]); ImGui::SameLine();
			imgui_help_marker("size of the volume considered to render.");
			ImGui::InputFloat3("location##1", &cloud_location[0]); ImGui::SameLine();
			imgui_help_marker("coordinates of the center of the rendering\nvolume");
			ImGui::SliderFloat("absorption", &cloud_absorption, 0.0f, 1.0f); ImGui::SameLine();
			imgui_help_marker("amount of light that will by retained by\nthe cloud.");
			ImGui::SliderFloat("edge fade", &cloud_volume_edge_fade_distance, 1.0f, 100.0f); ImGui::SameLine();
			imgui_help_marker("distance from the edges of the volume\nalong which the noise sampling will\ngradually decrease in order to avoid\nabrupt rendering breakups.");
			ImGui::Separator();
			ImGui::Text("density");
			ImGui::SliderFloat("threshold", &cloud_density_threshold, 0.0f, 1.0f); ImGui::SameLine();
			imgui_help_marker("minimum amount of density required\nanywhere in the volume to consider\ncomputing that point.");
			ImGui::InputFloat("multiplier", &cloud_density_multiplier); ImGui::SameLine();
			imgui_help_marker("factor by which a point's density, if\nabove the threshold, will be multiplied.");
		}

		// ---- noise ---- //

		if (ImGui::CollapsingHeader("noise")) {
			ImGui::Text("main");
			ImGui::SliderFloat("scale##1", &noise_main_scale, 1.0f, 128.0f);
			ImGui::SliderFloat3("offset##1", &noise_main_offset[0], 0.0f, 1.0f);
			ImGui::Text("weather");
			ImGui::SliderFloat("scale##2", &noise_weather_scale, 1.0f, 512.0f);
			ImGui::SliderFloat2("offset##2", &noise_weather_offset[0], 0.0f, 1.0f);
			ImGui::Text("detail");
			ImGui::SliderFloat("scale##3", &noise_detail_scale, 1.0f, 32.0f);
			ImGui::SliderFloat("weight##2", &noise_detail_weight, 0.0f, 1.0f);
			ImGui::SliderFloat3("offset##3", &noise_detail_offset[0], 0.0f, 1.0f);
			ImGui::Separator();
			ImGui::Text("rebake noise textures");
			if (ImGui::TreeNode("main##1")) {
				ImGui::Text("three dimensional worley noise texture\nused to define the shape of the clouds.");
				ImGui::InputInt("resolution##1", &noise_main_resolution); ImGui::SameLine();
				imgui_help_marker("should be a multiple of eight to avoid\npossible artifacts.", true);
				ImGui::SliderFloat("persistence##1", &noise_main_persistence, 0.0f, 1.0f); ImGui::SameLine();
				imgui_help_marker(	"factor by which layers are mixed up.\n"
						"the higher the value, the more mixed up\nthey'll be.\n"
						"if the value is zero, only the first\nlayer will be accounted for.");
				ImGui::Text("subdivisions per texture layer"); ImGui::SameLine();
				imgui_help_marker("cube root of the number of cell divisions\nfor each layer of the texture.");
				ImGui::InputInt("A##1", &noise_main_subdivisions_a);
				ImGui::InputInt("B##1", &noise_main_subdivisions_b);
				ImGui::InputInt("C##1", &noise_main_subdivisions_c);
				if (ImGui::Button("bake##1")) {
					main_shader->unbind();
					bake_noise_main(noise_main_id, compute_shader_main, noise_main_resolution, noise_main_persistence, noise_main_subdivisions_a, noise_main_subdivisions_b, noise_main_subdivisions_c);
					main_shader->bind();
					main_shader->set1i("noise_main_texture", noise_main_id);
					main_shader->unbind();
				}
				ImGui::SameLine();
				imgui_help_marker("bake at your own risk.\nbig values may take some time to compute\nor may freeze your computer.", true);
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("weather##1")) {
				ImGui::Text("two dimensional worley noise texture\nused to define where can clouds exist.");
				ImGui::InputInt("resolution##2", &noise_weather_resolution); ImGui::SameLine();
				imgui_help_marker("should be a multiple of eight to avoid\npossible artifacts.", true);
				ImGui::SliderFloat("persistence##2", &noise_weather_persistence, 0.0f, 1.0f); ImGui::SameLine();
				imgui_help_marker(	"factor by which layers are mixed up.\n"
						"the higher the value, the more mixed up\nthey'll be.\n"
						"if the value is zero, only the first\nlayer will be accounted for.");
				ImGui::Text("subdivisions per texture layer"); ImGui::SameLine();
				imgui_help_marker("square root of the number of cell divisions\nfor each layer of the texture.");
				ImGui::InputInt("A##2", &noise_weather_subdivisions_a);
				ImGui::InputInt("B##2", &noise_weather_subdivisions_b);
				ImGui::InputInt("C##2", &noise_weather_subdivisions_c);
				if (ImGui::Button("bake##2")) {
					main_shader->unbind();
					bake_noise_weather(noise_weather_id, compute_shader_weather, noise_weather_resolution, noise_weather_persistence, noise_weather_subdivisions_a, noise_weather_subdivisions_b, noise_weather_subdivisions_c);
					main_shader->bind();
					main_shader->set1i("noise_weather_texture", noise_weather_id);
					main_shader->unbind();
				}
				ImGui::SameLine();
				imgui_help_marker("bake at your own risk.\nbig values may take some time to compute\nor may freeze your computer.", true);
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("detail##1")) {
				ImGui::Text("three dimensional worley noise texture\nused to add detail to the shape of the\nclouds.");
				ImGui::InputInt("resolution##3", &noise_detail_resolution); ImGui::SameLine();
				imgui_help_marker("should be a multiple of eight to avoid\npossible artifacts.", true);
				ImGui::SliderFloat("persistence##3", &noise_detail_persistence, 0.0f, 1.0f); ImGui::SameLine();
				imgui_help_marker("factor by which layers are mixed up.\n"
						"the higher the value, the more mixed up\nthey'll be.\n"
						"if the value is zero, only the first\nlayer will be accounted for.");
				ImGui::Text("subdivisions per texture layer"); ImGui::SameLine();
				imgui_help_marker("cube root of the number of cell divisions\nfor each layer of the texture.");
				ImGui::InputInt("A##3", &noise_detail_subdivisions_a);
				ImGui::InputInt("B##3", &noise_detail_subdivisions_b);
				ImGui::InputInt("C##3", &noise_detail_subdivisions_c);
				if (ImGui::Button("bake##3")) {
					main_shader->unbind();
					bake_noise_main(noise_detail_id, compute_shader_main, noise_detail_resolution, noise_detail_persistence, noise_detail_subdivisions_a, noise_detail_subdivisions_b, noise_detail_subdivisions_c);
					main_shader->bind();
					main_shader->set1i("noise_detail_texture", noise_detail_id);
					main_shader->unbind();
				}
				ImGui::SameLine();
				imgui_help_marker("bake at your own risk.\nbig values may take some time to compute\nor may freeze your computer.", true);
				ImGui::TreePop();
			}
		}

		// ---- lighting ---- //

		if (ImGui::CollapsingHeader("lighting")) {
			ImGui::Checkbox("physically accurate sky", &render_sky);
			if (!render_sky) {
				ImGui::ColorEdit3("background color", &background_color[0]);
			}
			ImGui::Separator();
			ImGui::Text("light direction");
			bool light_direction_method_modified = ImGui::Checkbox("any direction", &light_any_direction); ImGui::SameLine();
			imgui_help_marker("you can modify the exact light direction or\n"
					"you can set the time of the day to resemble\n"
					"the light's angle at that time.");
			bool light_direction_modified = false;
			if (light_any_direction) {
				light_direction_modified |= ImGui::SliderFloat("x", &light_direction[0], -1.0f, 1.0f);
				light_direction_modified |= ImGui::SliderFloat("y", &light_direction[1], -1.0f, 1.0f);
				light_direction_modified |= ImGui::SliderFloat("z", &light_direction[2], -1.0f, 1.0f);
			} else if (light_direction_method_modified || ImGui::SliderFloat("time", &time, 6.0f, 18.0f)) {
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
			// normalize light direction and update shader
			if (light_direction_modified) {
				// normalize light direction and update shader
				float light_direction_module = std::sqrt(light_direction[0] * light_direction[0] + light_direction[1] * light_direction[1] + light_direction[2] * light_direction[2]);
				light_direction[0] /= light_direction_module;
				light_direction[1] /= light_direction_module;
				light_direction[2] /= light_direction_module;
				// update in shader
				main_shader->set3f("light_direction", light_direction[0], light_direction[1], light_direction[2]);
				if (light_direction[0] == 0.0f) {
					inverse_light_direction[0] = 1.0f;
				} else {
					inverse_light_direction[0] = 1.0f / light_direction[0];
				}
				if (light_direction[1] == 0.0f) {
					inverse_light_direction[1] = 1.0f;
				} else {
					inverse_light_direction[1] = 1.0f / light_direction[1];
				}
				if (light_direction[2] == 0.0f) {
					inverse_light_direction[2] = 1.0f;
				} else {
					inverse_light_direction[2] = 1.0f / light_direction[2];
				}
				main_shader->set3f("inverse_light_direction", inverse_light_direction[0], inverse_light_direction[1], inverse_light_direction[2]);
			}
		}

		// ---- wind ---- //

		if (ImGui::CollapsingHeader("wind")) {
			ImGui::SliderFloat3("direction##2", &wind_direction[0], -1.0f, 1.0f);
			ImGui::InputFloat("speed", &wind_speed);
			ImGui::Separator();
			ImGui::Text("how much does wind affect different noise layers");
			ImGui::SliderFloat("main##3", &wind_main_weight, 0.0f, 1.0f);
			ImGui::SliderFloat("weather##3", &wind_weather_weight, 0.0f, 1.0f);
			ImGui::SliderFloat("detail##3", &wind_detail_weight, 0.0f, 1.0f);
		}

		// ---- camera ----//

		if (ImGui::CollapsingHeader("camera")) {
			ImGui::SliderFloat("pitch", &camera_pitch, 0.0f, 360.0f);
			ImGui::SliderFloat("yaw", &camera_yaw, 0.0f, 360.0f);
			ImGui::InputFloat3("location##2", &camera_location.x, 3);
		}

		// ---- rendering ---- //

		if (ImGui::CollapsingHeader("rendering")) {
			ImGui::Text("application");
			ImGui::Checkbox("full screen", &fullscreen);
			ImGui::InputInt2("resolution", &resolution[0]);
			ImGui::SliderInt("target fps", &fps, 10, 244);
			if (ImGui::Button("apply")) {
				GLFWmonitor *monitor = glfwGetPrimaryMonitor();
				const GLFWvidmode *mode = glfwGetVideoMode(monitor);
				if (fullscreen) {
					glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
				} else {
					glfwSetWindowMonitor(window, nullptr, 0, 0, resolution[0], resolution[1], mode->refreshRate);
				}
				millis_per_frame = 1000 / fps;
				// update in shader
				main_shader->bind();
				main_shader->set2f("resolution", resolution[0], resolution[1]);
				main_shader->unbind();
			}
			ImGui::Separator();
			ImGui::Text("number of samples taken");
			ImGui::SliderInt("per ray", &render_volume_samples, 8, 128); ImGui::SameLine();
			imgui_help_marker("number of noise samples taken along the\nmain ray");
			ImGui::SliderInt("in scatter", &render_in_scatter_samples, 4, 64); ImGui::SameLine();
			imgui_help_marker("number of noise samples taken to compute\nthe in scattered light for each sample\nof the primary ray.");
			ImGui::Text("number of calls to the noise sampling function: %d", render_volume_samples * render_in_scatter_samples);
			ImGui::Separator();
			ImGui::Text("shadowing");
			ImGui::InputFloat("distance", &render_shadowing_max_distance); ImGui::SameLine();
			imgui_help_marker("maximum distance at which shadows will\nbe casted.");
			ImGui::SliderFloat("weight", &render_shadowing_weight, 0.0f, 1.0f);
		}

		// ---- export ---- //

		if (ImGui::CollapsingHeader("export")) {
			if (!recording) {
				ImGui::InputInt("output fps##recording", &recording_fps);
				if (ImGui::Button("start recording")) {
					recording = true;
					recording_output.open("render.avi", cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), recording_fps, cv::Size(resolution[0], resolution[1]));
					recording_timer = std::chrono::system_clock::now();
					recording_start_frame = frame;
				}
			} else {
				// info about recording session
				ImGui::Text("recording framerate: %d fps", recording_fps);
				std::chrono::duration<double, std::milli> millis_ellapsed(std::chrono::system_clock::now() - recording_timer);
				unsigned long long frames_ellapsed = frame - recording_start_frame;
				ImGui::Text("frames written:      %d frames", frames_ellapsed);
				ImGui::Text("real time ellapsed:  %.2f seconds", millis_ellapsed.count() / 1000.0f);
				ImGui::Text("video time recorded: %.2f seconds", (float)frames_ellapsed / recording_fps);
				// stop?
				if (ImGui::Button("stop recording")) {
					recording = false;
					recording_output.release();
				}
			}
		}

		imgui_window_is_focused |= ImGui::IsWindowFocused();
		ImGui::End();

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

		// ---- overlay ---- //

		const float distance = 10.0f;
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
		ImGui::SetNextWindowPos( { distance, distance }, ImGuiCond_Always, { 0.0f, 0.0f });
		ImGui::SetNextWindowBgAlpha(0.8f);
		if (ImGui::Begin("ao by soybin", NULL, window_flags)) {
			ImGui::Text("ao by soybin");
			ImGui::Text("~~~~~~~~~~~~");
			ImGui::Text("fps    -> %.3f", last_fps);
			ImGui::Text("angles -> %.1f | %.1f", camera_pitch, camera_yaw);
		}
		ImGui::End();

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

		// cloud
		main_shader->set1f("cloud_volume_edge_fade_distance", cloud_volume_edge_fade_distance);
		main_shader->set1f("cloud_absorption", cloud_absorption);
		main_shader->set1f("cloud_density_threshold", cloud_density_threshold);
		main_shader->set1f("cloud_density_multiplier", cloud_density_multiplier);
		main_shader->set3f("cloud_location", cloud_location[0], cloud_location[1], cloud_location[2]);
		main_shader->set3f("cloud_volume", cloud_volume[0] / 2.0f, cloud_volume[1] / 2.0f, cloud_volume[2] / 2.0f);

		// rendering
		main_shader->set1i("render_volume_samples", render_volume_samples);
		main_shader->set1i("render_in_scatter_samples", render_in_scatter_samples);
		main_shader->set1f("render_shadowing_max_distance", render_shadowing_max_distance);
		main_shader->set1f("render_shadowing_weight", render_shadowing_weight);

		// noise
		main_shader->set1f("noise_main_scale", noise_main_scale);
		main_shader->set3f("noise_main_offset", noise_main_offset[0], noise_main_offset[1], noise_main_offset[2]);
		main_shader->set1f("noise_weather_scale", noise_weather_scale);
		main_shader->set2f("noise_weather_offset", noise_weather_offset[0], noise_weather_offset[1]);
		main_shader->set1f("noise_detail_scale", noise_detail_scale);
		main_shader->set1f("noise_detail_weight", noise_detail_weight);
		main_shader->set3f("noise_detail_offset", noise_detail_offset[0], noise_detail_offset[1], noise_detail_offset[2]);

		// wind
		main_shader->set3f("wind_vector", wind_direction[0] * wind_speed, wind_direction[1] * wind_speed, wind_direction[2] * wind_speed);
		main_shader->set1f("wind_main_weight", wind_main_weight);
		main_shader->set1f("wind_weather_weight", wind_weather_weight);
		main_shader->set1f("wind_detail_weight", wind_detail_weight);

		// skydome
		main_shader->set1i("render_sky", render_sky);
		main_shader->set3f("background_color", background_color[0], background_color[1], background_color[2]);

		// update screen with new frame
		glfwSwapBuffers(window);

		// how long did this frame take to render
		std::chrono::duration<double, std::milli> millis_ellapsed(std::chrono::system_clock::now() - millis_start);
		int millis_remaining_per_frame = std::max((int)(millis_per_frame - millis_ellapsed.count()), 0);

		// update current fps
		last_fps = 1000.0f / (millis_ellapsed.count() + millis_remaining_per_frame);

		// wait for remaining time
		std::this_thread::sleep_for(std::chrono::milliseconds(millis_remaining_per_frame));
	}

	// ---- cleanup ---- //

	glfwTerminate();

	delete compute_shader_main;
	delete compute_shader_weather;
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

void compute_worley_grid(glm::vec4* points, int subdivision, bool weather = false) {
	float cell_size = 1.0f / (float)subdivision;
	for (int i = 0; i < subdivision; ++i) {
		for (int j = 0; j < subdivision; ++j) {
			if (!weather) { // main noise
				for (int k = 0; k < subdivision; ++k) {
					float x = (float)rand()/(float)(RAND_MAX);
					float y = (float)rand()/(float)(RAND_MAX);
					float z = (float)rand()/(float)(RAND_MAX);
					glm::vec3 offset = glm::vec3(i, j, k) * cell_size;
					glm::vec3 corner = glm::vec3(x, y, z) * cell_size;
					points[i + subdivision * (j + k * subdivision)] = glm::vec4(offset + corner, 0.0f);
				}
			} else { // weather map
				float x = (float)rand() / (float)(RAND_MAX);
				float y = (float)rand() / (float)(RAND_MAX);
				glm::vec2 offset = glm::vec2(i, j) * cell_size;
				glm::vec2 corner = glm::vec2(x, y) * cell_size;
				points[i + j * subdivision] = glm::vec4(offset + corner, 0.0f, 0.0f);
			}
		}
	}
}

// ---- 3d worley FBM ---- //
void bake_noise_main(unsigned int &texture_id, shader* compute, int resolution, float persistance, int subdivisions_a, int subdivisions_b, int subdivisions_c) {
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

// ---- 2d worley FBM ---- //
void bake_noise_weather(unsigned int &texture_id, shader* compute, int resolution, float persistance, int subdivisions_a, int subdivisions_b, int subdivisions_c) {
	// first time generating texture
	if (glIsTexture(texture_id)) {
		glDeleteTextures(1, &texture_id);
		std::cout << "[+] baking new weather texture" << std::endl;
	}
	glGenTextures(1, &texture_id);
	glActiveTexture(GL_TEXTURE0 + texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, resolution, resolution, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
	glBindImageTexture(0, texture_id, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8);

	// lay random points per each cell in
	// the grid.
	// for each layer
	glm::vec4* points_a = new glm::vec4[subdivisions_a * subdivisions_a];
	glm::vec4* points_b = new glm::vec4[subdivisions_b * subdivisions_b];
	glm::vec4* points_c = new glm::vec4[subdivisions_c * subdivisions_c];
	compute_worley_grid(points_a, subdivisions_a, true);
	compute_worley_grid(points_b, subdivisions_b, true);
	compute_worley_grid(points_c, subdivisions_c, true);

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
	glBufferData(GL_SHADER_STORAGE_BUFFER, 16 * subdivisions_a * subdivisions_a, points_a, GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo_a);
	delete[] points_a;

	// pass random points b to shader storage buffer object
	unsigned int ssbo_b = 0;
	glGenBuffers(1, &ssbo_b);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_b);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 16 * subdivisions_b * subdivisions_b, points_b, GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbo_b);
	delete[] points_b;

	// pass random points c to shader storage buffer object
	unsigned int ssbo_c = 0;
	glGenBuffers(1, &ssbo_c);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_c);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 16 * subdivisions_c * subdivisions_c, points_c, GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbo_c);
	delete[] points_c;
	
	// dispatch compute shader
	glDispatchCompute(resolution / 8, resolution / 8, 1);

	// wait till finished
	glMemoryBarrier(GL_ALL_BARRIER_BITS);

	// delete buffers
	glDeleteBuffers(1, &ssbo_a);
	glDeleteBuffers(1, &ssbo_b);
	glDeleteBuffers(1, &ssbo_c);
}

static void imgui_help_marker(const char* desc, bool warning) {
	ImGui::TextDisabled(warning ? "(!)" : "(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}
