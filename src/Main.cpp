#include<iostream>
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<stb/stb_image.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>

#include"Core/VAO.h"
#include"Core/VBO.h"
#include"Core/EBO.h"
#include"Core/Shader.h"
#include"Core/Texture.h"
#include"Window.h"
#include"Metro/ComputeStructures.h"
#include"Core/Camera.h"
#include "Metro/RayScene.h"



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