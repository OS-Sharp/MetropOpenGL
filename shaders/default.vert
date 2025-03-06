#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNorm;
layout (location = 2) in vec3 aCol;
layout (location = 3) in vec2 aTex;

out vec3 color;
out vec2 texCoord;
out vec3 normal;

void main()
{
	color = aCol;
	texCoord = aTex;
	normal = aNorm;
	gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
}