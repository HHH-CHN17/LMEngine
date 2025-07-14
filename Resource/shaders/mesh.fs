#version 460 core
out vec4 FragColor;

// 本节中引入了法线贴图，所以计算用的法线坐标并非VBO中传入的法线坐标，而是法线贴图中的法线坐标
struct Material 
{
	sampler2D	diffuse;		// 材质的漫反射贴图，由于环境光颜色在几乎所有情况下都等于漫反射颜色，所以移除了amibent
	sampler2D	specular;		// 材质的镜面反射贴图
	sampler2D	normal;			// 材质的法线贴图
	float 		shininess;		// 材质的高光的散射/半径
};

struct Light {
    vec3 position;

    vec3 ambient;		// 光源的环境光强度（光在环境光的条件下，颜色不一定是vec3(1.0f)，其他同理）
    vec3 diffuse;       // 光源的漫反射强度
    vec3 specular;      // 光源的镜面反射强度
};

uniform Material material;
uniform Light light;
uniform vec3 viewPos;

in vec3 FragPos;
in vec2 TexCoords;
in mat3 TBN;

void main()
{
	// 法向量
	vec3 normal = texture(material.normal, TexCoords).rgb;
	normal = normalize(normal * 2.0 - 1.0);		// 法线坐标在rgb中值域为[0, 1], 需要转换为NDC，即[-1, 1]
	normal = normalize(TBN * normal); 
	
	// 纹理颜色
	vec3 diffuseColor = texture(material.diffuse, TexCoords).rgb;
	vec3 specularColor = texture(material.specular, TexCoords).rgb;
	
	// 计算环境光
    vec3 ambientLight = diffuseColor * light.ambient;
	
	// 计算漫反射光
	vec3 lightDir = normalize(light.position - FragPos);				// 注意这里算出来的方向与实际的光源方向相反
	float deltaDiffuse = max(dot(lightDir, normal), 0.0f);
	vec3 diffuseLight = diffuseColor * light.diffuse * deltaDiffuse;
	
	// 计算镜面反射光
	vec3 viewDir = normalize(viewPos - FragPos);						// 注意这里算出来的方向与实际的观察方向相反
	vec3 reflectDir = reflect(-lightDir, normal);
	vec3 halfwayDir = normalize(lightDir + viewDir);  
	float deltaSpecular = pow(max(dot(normal, halfwayDir), 0.0f), material.shininess);
	vec3 specularLight = specularColor * light.specular * deltaSpecular;

    //vec3 result = ambientLight + diffuseLight + specularLight;
    vec3 result = diffuseColor;???
    FragColor = vec4(result, 1.0);
};