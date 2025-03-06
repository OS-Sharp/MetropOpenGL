#include"Window.h"
#include <iostream>

Window::Window(const char* name, const int width, const int height) : 
	width(width), height(height), name(name) {
	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    instance = glfwCreateWindow(width, height, name, NULL, NULL);
	if (instance == NULL) {
		std::cout << "Failed to create window" << std::endl;
		glfwTerminate();
	}
	glfwMakeContextCurrent(instance);
	glfwSwapInterval(0);
	gladLoadGL();
	glDisable(GL_CULL_FACE);
	glViewport(0, 0, width, height);
}

bool Window::ShouldClose()
{
	return glfwWindowShouldClose(instance);
}

void Window::SwapBuffers()
{
	glfwSwapBuffers(instance);
}

void Window::Delete()
{
	glfwDestroyWindow(instance);
}
