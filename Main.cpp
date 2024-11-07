#include<iostream>
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<stb/stb_image.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>

#include"VAO.h"
#include"VBO.h"
#include"EBO.h"
#include"Shader.h"
#include"Texture.h"
#include"Window.h"
#include"ComputeStructures.h"
#include"Camera.h"
#include "RayScene.h"



int main() 
{
	Window win("Metropolis", 1400, 800);
	RayScene scene(win);

	scene.OnWindowLoad(win);

	while(!win.ShouldClose())
	{
		scene.OnBufferSwap(win);
		win.SwapBuffers();
		glfwPollEvents();
	}

	scene.OnWindowClose(win);
	win.Delete();
	glfwTerminate();

	return 0;
}