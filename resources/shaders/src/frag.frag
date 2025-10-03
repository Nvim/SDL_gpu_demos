#version 450 core

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec3 FragPos;
layout(location = 3) in vec3 ViewPos;

layout(set = 2, binding = 0) uniform sampler2D TexDiffuse;
layout(set = 2, binding = 1) uniform sampler2D TexNormal;
layout(set = 2, binding = 2) uniform sampler2D TexMetalRough;

struct DirLight {
  vec3 direction; 
  vec3 ambient;
  vec3 diffuse;
  // vec3 specular;
};

const DirLight SunLight = {
    vec3(30, 20, 10),
    vec3(0.2, 0.2, 0.2f),
    vec3(0.8, 0.8, 0.8),
};

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(ViewPos - FragPos);

    // ambient
    vec3 ambient = SunLight.ambient * texture(TexDiffuse, uv).rgb;

    // diffuse
    vec3 lightDir = normalize(SunLight.direction);  
    float diff_fact = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = SunLight.diffuse * diff_fact * texture(TexDiffuse, uv).rgb;

    vec3 result = diffuse;

    FragColor = vec4(result, 1.0f);
}
