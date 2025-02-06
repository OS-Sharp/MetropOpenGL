#ifndef VBO_CLASS_H
#define VBO_CLASS_H

#include <glad/glad.h>
#include <vector>
#include "Vertex.h"

class VBO {
public:
	GLuint ID;
	VBO(std::vector<Vertex>& vertices);
	VBO();
	void SetData(GLsizeiptr size, const void* data, GLenum usage);
	void SetSubData(GLsizeiptr size, const void* data, GLenum usage);
	void Bind();
	void Unbind();
	void Delete();
};

#endif