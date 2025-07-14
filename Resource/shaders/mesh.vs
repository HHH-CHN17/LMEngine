#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

uniform mat4 Model;
uniform mat4 View;
uniform mat4 Projection;

out vec3 FragPos; // 此处的片段坐标是世界空间下的片段坐标
out vec2 TexCoords;
out mat3 TBN;

void main()
{
	gl_Position = Projection * View * Model * vec4(aPos, 1.0f);
	
	mat3 normalMatrix = mat3(transpose(inverse(Model)));
	vec3 T = normalize(normalMatrix * aTangent);
	vec3 B = normalize(normalMatrix * aBitangent);
	vec3 N = normalize(normalMatrix * aNormal);
	
	FragPos = vec3(Model * vec4(aPos, 1.0f));
	TexCoords = aTexCoords;
	TBN = mat3(T, B, N);
};