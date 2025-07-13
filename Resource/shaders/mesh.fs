#version 120


struct Light
{
    vec3 m_pos;
    vec3 m_ambient;
    vec3 m_diffuse;
    vec3 m_specular;

    float m_c;
    float m_l;
    float m_q;
};

uniform Light myLight;

uniform sampler2D   texture_diffuse;
uniform sampler2D   texture_normal;
uniform sampler2D   texture_specular;

uniform float       m_shiness;

uniform vec3       u_viewPos;

varying vec4 vary_pos;
varying vec2 vary_texCoord;
varying  mat3 vary_tbnMatrix;

void main(void)
{
    vec3 normal = texture2D(texture_normal,vary_texCoord).rgb;
    normal = normalize(normal * 2.0 - 1.0);
    normal = normalize(vary_tbnMatrix * normal);

    float dist = length(myLight.m_pos - vary_pos.xyz);

    float attenuation = 1.0f / (myLight.m_c + myLight.m_l * dist + myLight.m_q *dist * dist);

    //ambient
    vec3 ambient = myLight.m_ambient * vec3(texture2D(texture_diffuse , vary_texCoord).rgb);
    //diffuse
    vec3 lightDir = normalize(myLight.m_pos - vary_pos.xyz);
    float diff = max(dot(normal , lightDir) , 0.0f);
    vec3 diffuse = myLight.m_diffuse * diff * vec3(texture2D(texture_diffuse , vary_texCoord).rgb);

    //mirror reflect
    float specular_strength = 0.5;
    vec3 viewDir = normalize(u_viewPos - vary_pos.xyz);
    vec3 reflectDir = reflect(-lightDir , normal);

    float spec =  pow(max(dot(viewDir , reflectDir) , 0.0f) , m_shiness);

    vec3 sepcular = specular_strength* myLight.m_specular * spec;
    //vec3 sepcular = specular_strength* myLight.m_specular * spec * vec3(texture2D(texture_specular , vary_texCoord).rgb);

    vec3 result = ambient  + diffuse + sepcular ;
    gl_FragColor = vec4(result,1.0f) ;

}
