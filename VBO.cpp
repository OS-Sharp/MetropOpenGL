#include"VBO.h"
#include <vector>

VBO::VBO(std::vector<Vertex>& vertices) {
	glGenBuffers(1, &ID);
	glBindBuffer(GL_ARRAY_BUFFER, ID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
}

VBO::VBO() {
	glGenBuffers(1, &ID);
	glBindBuffer(GL_ARRAY_BUFFER, ID);
	glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_STATIC_DRAW);
}

void VBO::SetData(GLsizeiptr size, const void* data, GLenum usage) {
	Bind();
	glBufferData(GL_ARRAY_BUFFER, size, data, usage);
	Unbind();
}

void VBO::SetSubData(GLsizeiptr size, const void* data, GLenum usage) {
	Bind();
	glBufferData(GL_ARRAY_BUFFER, size, data, usage);
	Unbind();
}

void VBO::Bind() {
	glBindBuffer(GL_ARRAY_BUFFER, ID);
}

void VBO::Unbind() {
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VBO::Delete() {
	glDeleteBuffers(1, &ID);
}