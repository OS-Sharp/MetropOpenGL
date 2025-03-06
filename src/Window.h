#ifndef WINDOW_CLASS_H
#define WINDOW_CLASS_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

class Window {
	public:
		GLFWwindow* instance;
		const int width;
		const int height;
		const char* name;

		Window(const char* name, const int width, const int height);
		bool ShouldClose();
		void SwapBuffers();
		void Delete();
};

#endif