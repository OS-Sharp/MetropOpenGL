#pragma once

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/rotate_vector.hpp>
#include<glm/gtx/vector_angle.hpp>

#include"Shader.h"

class Camera
{
	public:
		glm::vec3 Position;
		glm::vec3 Orientation = glm::vec3(0.0f, 0.0f, 1.0f);
		glm::vec3 Up = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::mat4 cameraMatrix = glm::mat4(1.0f);

		int width;
		int height;

		float moveSensitivity = 0.01f;
		float rotateSensitivty = 100.0f;

		bool firstClick;

		Camera(int width, int height, glm::vec3 position);

		void Matrix(Shader& shader, const char* uniform);
		void UpdateMatrix(float FOVdeg, float nearPlane, float farPlane);
		bool Inputs(GLFWwindow* window);
};
