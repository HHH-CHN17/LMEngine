#version 120
attribute  vec4 a_position;
attribute  vec2 a_texturCoord;
attribute  vec3 a_normal;
attribute  vec3 a_tangent;
attribute  vec3 a_bitangent;

uniform  mat4 u_projectionMatrix;
uniform  mat4 u_viewMatrix;
uniform mat4 u_modelMatrix;

varying  vec4 vary_pos;
varying  vec2 vary_texCoord;
varying  mat3 vary_tbnMatrix;


void main(void)
{
    mat4 mv_matrix = u_viewMatrix * u_modelMatrix;
    gl_Position = u_projectionMatrix * mv_matrix * a_position;

    vary_texCoord = a_texturCoord;

    vary_pos = u_modelMatrix * a_position;

//    vec3 normal  = normalize(mat3(gl_ModelViewMatrixInverseTranspose) * a_normal);
//    vec3 tangent = normalize(mat3(gl_ModelViewMatrixInverseTranspose) * a_tangent);
//    tangent = normalize(tangent - dot(tangent, normal) * normal);
//    vec3 bitangent = cross(normal, tangent);
//    vary_tbnMatrix = mat3(tangent, bitangent, normal);

    //mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 normal = normalize(mat3(gl_ModelViewMatrixInverseTranspose) * a_normal);
    vec3 tangent = normalize(mat3(gl_ModelViewMatrixInverseTranspose) * a_tangent);
    vec3 bitangent = normalize(mat3(gl_ModelViewMatrixInverseTranspose) * a_bitangent);
    vary_tbnMatrix = mat3(tangent, bitangent, normal);

}
