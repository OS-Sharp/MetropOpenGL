#ifndef TEXTURE_CLASS_H
#define TEXTURE_CLASS_H

#include<glad/glad.h>
#include<stb/stb_image.h>

#include"Shader.h"

class Texture
{
public:
	GLuint ID;
	const char* type;
	GLuint unit;

	Texture(const char* image, const char* texType, GLuint slot);
	Texture(GLuint width, GLuint height, GLuint slot, GLuint binding, GLenum access = GL_READ_WRITE, GLenum format = GL_RGBA32F);

	// Assigns a texture unit to a texture
	void texUnit(Shader& shader, const char* uniform);
	// Binds a texture
	void Bind();
	// Unbinds a texture
	void Unbind();
	// Deletes a texture
	void Delete();
};
#endif