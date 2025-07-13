#version 460 core
out vec4 FragColor;

uniform sampler2D texY;
uniform sampler2D texU;
uniform sampler2D texV;

in vec2 TexCoords;

void main(void)
{
    vec3 yuv;
    vec3 rgb;
    yuv.x = texture2D(texY, TexCoords).r;
    yuv.y = texture2D(texU, TexCoords).r - 0.5;
    yuv.z = texture2D(texV, TexCoords).r - 0.5;
    rgb = mat3( 1,1,1, 0,-0.39465,2.03211,1.13983,-0.58060,0) * yuv;
    FragColor = vec4(rgb, 1.0f);
}
