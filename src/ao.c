#include <cstdio>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"

#define IMGUI_IMPL_OPENGL_LOADER_GLEW

//---- global variables ----//

unsigned int width = 1280;
unsigned int height = 720;
GLFWwindow* window;

//---- entry point ----//

int main(int argc, char* argv[]) {

	//---- init glfw ----//

	if (!glfwInit()) {
		puts("[-] GLFW initialization failed. Exiting ao.");
		return 1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(width, height, "glsl", 0, 0);

	if (window == NULL) {
		glfwTerminate();
		puts("[-] GLFW window initialization failed. Exiting ao.");
		return 1;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	//---- init glew ----//

	if (glewInit() != GLEW_OK) {
		glfwTerminate();
		puts("[-] GLEW initialization failed. Exiting ao.");
		return 1;
	}

	//---- init imgui ----//

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();

	// initialize appropiate bindings
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 420");

	// change color theme
	ImGui::StyleColorsDark();

	//---- work ----//

	for (;;) {
		glfwPollEvents();
		glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
		glClear(GL_COLOR_BUFFER_BIT);

		// begin new frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// compute gui
		ImGui::Begin("Demo window");
		ImGui::Button("Hello!");
		ImGui::End();

		// render gui to window
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glfwSwapBuffers(window);
	}

	return 0;
}

