#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;
out vec3 glpos;

uniform mat4 view;
uniform mat4 projection;

void main()
{
	TexCoords = aTexCoords;
	glpos = aPos;
	gl_Position = projection * view * vec4(aPos, 1.0f);
}
