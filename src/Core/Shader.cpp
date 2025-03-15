#include"Shader.h"

std::string get_file_contents(const char* filename) {
	std::ifstream in(filename, std::ios::binary);
	if (in)
	{
		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		in.close();
		return(contents);
	}
	throw(errno);
}

Shader::Shader(const char* vertexFile, const char* fragmentFile) {

	std::string vertexSource = get_file_contents(vertexFile);
	std::string fragmentSource = get_file_contents(fragmentFile);

	const char* vertexShaderSource = vertexSource.c_str();
	const char* fragmentShaderSource = fragmentSource.c_str();

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);

	ID = glCreateProgram();
	glAttachShader(ID, vertexShader);
	glAttachShader(ID, fragmentShader);
	glLinkProgram(ID);

	GLint success;
	glGetProgramiv(ID, GL_LINK_STATUS, &success);
	if (!success) {
		char infoLog[512];
		glGetProgramInfoLog(ID, 512, NULL, infoLog);
		std::cerr << "Shader Linking Error:\n" << infoLog << std::endl;
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

Shader::Shader(const char* computeFile) {

	std::string computeSource = get_file_contents(computeFile);

	const char* computeShaderSource = computeSource.c_str();

	GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(computeShader, 1, &computeShaderSource, NULL);
	glCompileShader(computeShader);


	ID = glCreateProgram();
	glAttachShader(ID, computeShader);
	glLinkProgram(ID);

	GLint success;
	glGetProgramiv(ID, GL_LINK_STATUS, &success);
	if (!success) {
		char infoLog[512];
		glGetProgramInfoLog(ID, 512, NULL, infoLog);
		std::cerr << "Shader Linking Error:\n" << infoLog << std::endl;
	}

	glDeleteShader(computeShader);
}

void Shader::Delete() {
	glDeleteProgram(ID);
}

void Shader::SetParameterInt(int data, const char* uniform)
{
	GLuint loc = glGetUniformLocation(ID, uniform);
	glUniform1i(loc, data);
}

void Shader::SetParameterFloat(float data, const char* uniform)
{
	glUniform1f(glGetUniformLocation(ID, uniform), data);
}

void Shader::SetParameterColor(glm::vec3 data, const char* uniform)
{

	glUniform3f(glGetUniformLocation(ID, uniform), data.x, data.y, data.z);
}

void Shader::SetParameterDouble(double data, const char* uniform)
{
	glUniform1d(glGetUniformLocation(ID, uniform), data);
}

void Shader::SetParameterSampler(const char* uniform, int textureUnit)
{
	GLuint loc = glGetUniformLocation(ID, uniform);
	glUniform1i(loc, textureUnit);
}


void Shader::Activate(bool compute, GLuint gX, GLuint gY, GLuint gZ) {
	if(!compute) glUseProgram(ID);
	else {
		glUseProgram(ID);
		glDispatchCompute(gX, gY, gZ);
		glMemoryBarrier(GL_ALL_BARRIER_BITS);
	}
}

void Shader::Dispatch(GLuint gX, GLuint gY, GLuint gZ) {

	glDispatchCompute(gX, gY, gZ);
	glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void Shader::DeleteSSBOs() {
	for (auto& buffer : SSBOBuffers) {
		if(glIsBuffer(buffer)) glDeleteBuffers(1, &buffer);
	}

	SSBOBuffers.clear();
}