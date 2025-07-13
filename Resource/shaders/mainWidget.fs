#version 460 core
out vec4 FragColor;

in vec2 TexCoords;
uniform sampler2D mainTexture;

void main()
{
    FragColor = texture2D(mainTexture, TexCoords);
}
