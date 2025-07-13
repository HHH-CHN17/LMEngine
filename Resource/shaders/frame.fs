#version 460 core
out vec4 FragColor;

in vec2 TexCoords;
uniform sampler2D frameTexture;

uniform vec2 pos;
in vec3 glpos;

void main()
{
    if ((pos.x - 0.05 <= glpos.x && glpos.x <= pos.x + 0.05)
        && (pos.y - 0.05 <= glpos.y && glpos.y <= pos.y + 0.05))
    {
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    }
    else
    {
        FragColor = texture2D(frameTexture, vec2(1.0 - TexCoords.x, TexCoords.y));
    }
}
