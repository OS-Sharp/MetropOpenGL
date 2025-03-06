#pragma once
#include<glm/glm.hpp>

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 texUV;

	Vertex(glm::vec3 position, glm::vec3 normal, glm::vec3 color, glm::vec2 texUV) : position(position), normal(normal), color(color), texUV(texUV) {}
};