#ifndef SHADER_CLASS_H
#define SHADER_CLASS_H

#include<glad/glad.h>
#include<string>
#include<fstream>
#include<sstream>
#include<iostream>
#include<cerrno>
#include<vector>
#include<unordered_map>
#include<glm/glm.hpp>

std::string get_file_contents(const char* filename);

class Shader {
public:
	GLuint ID;
	std::vector<GLuint> SSBOBuffers;

	Shader(const char* vertexFile, const char* fragmentFile);
	Shader(const char* computeFile);

	void Activate(bool compute = false, GLuint gX = 0, GLuint gY = 0, GLuint gZ = 0);
	void Dispatch(GLuint gX = 0, GLuint gY = 0, GLuint gZ = 0);

	void Delete();
	template <class T>
	GLuint StoreSSBO(std::vector<T> data, int binding, bool toBeDeleted = true);
	template <class T>
	GLuint StoreSSBO(T data, int binding, bool toBeDeleted = true);
	template <class T>
	GLuint StoreSSBOWithLength(std::vector<T> data, int binding);
	template <class T>
	GLuint UpdateSSBO(std::vector<T> data, int binding);
	template <class T>
	GLuint UpdateSSBO(T data, int binding);

	void SetParameterInt(int data, const char* uniform);
	void SetParameterFloat(float data, const char* uniform);
	void SetParameterDouble(double data, const char* uniform);
	void SetParameterColor(glm::vec3 data, const char* uniform);
	void DeleteSSBOs();
};

template <class T>
GLuint Shader::StoreSSBO(std::vector<T> data, int binding, bool toBeDeleted) {
	GLuint ssbo;

	// Generate and bind the SSBO
	glGenBuffers(1, &ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

	// Allocate and initialize the buffer data
	glBufferData(GL_SHADER_STORAGE_BUFFER, data.size() * sizeof(T), data.data(), GL_DYNAMIC_COPY);

	// Bind the SSBO to the binding point 0, as per the compute shader 
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, ssbo);

	if (toBeDeleted) SSBOBuffers.push_back(ssbo);

	return ssbo;
}

template <class T>
GLuint Shader::StoreSSBO(T data, int binding, bool toBeDeleted) {
	GLuint ssbo;

	// Generate and bind the SSBO
	glGenBuffers(1, &ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

	// Allocate and initialize the buffer data
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(T), &data, GL_DYNAMIC_COPY);

	// Bind the SSBO to the binding point 0, as per the compute shader
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, ssbo);

	if(toBeDeleted) SSBOBuffers.push_back(ssbo);

	return ssbo;
}

template <class T>
GLuint Shader::UpdateSSBO(std::vector<T> data, int binding) {

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, binding);
	glBufferData(GL_SHADER_STORAGE_BUFFER, data.size() * sizeof(T), data.data(), GL_DYNAMIC_COPY);

	SSBOBuffers.push_back(binding);

	return binding;
}

template <class T>
GLuint Shader::UpdateSSBO(T data, int binding) {

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, binding);
	glBufferData(GL_SHADER_STORAGE_BUFFER,  sizeof(T), &data, GL_DYNAMIC_COPY);

	SSBOBuffers.push_back(binding);

	return binding;
}

template <class T>
GLuint Shader::StoreSSBOWithLength(std::vector<T> data, int binding) {
	GLuint ssbo;
	GLuint length = static_cast<GLuint>(data.size());

	// Generate and bind the SSBO
	glGenBuffers(1, &ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

	// Calculate the total size for length + array data
	size_t totalSize = sizeof(GLuint) + data.size() * sizeof(T);

	glBufferData(GL_SHADER_STORAGE_BUFFER, totalSize, nullptr, GL_DYNAMIC_COPY);

	// Allocate and initialize the buffer data
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &length);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint), data.size() * sizeof(T), data.data() );

	// Bind the SSBO to the binding point 0, as per the compute shader
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, ssbo);
	SSBOBuffers.push_back(ssbo);

	return ssbo;
}
#endif // !SHADER_CLASS_H