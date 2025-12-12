#version 450 core

layout(location = 0) out vec4 OutFragColor;
layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inViewPos;
layout(location = 2) in vec3 inFragPos;
layout(location = 3) in vec3 inNormal;

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

// layout(std140, set = 3, binding = 0) uniform uDirLight {
//     DirLight sun;
// };

void main()
{
    const DirLight sun =
        {
            vec3(-10.f, 10.f, -10.f),
            vec3(.3f, .3f, .3f),
            vec3(1.f, 1.f, 1.f),
            vec3(.6f, .6f, .6f)
        };
    vec3 normal = normalize(inNormal);

    vec3 ambient = sun.ambient * inColor;

    vec3 light_dir = normalize(-sun.direction);
    float diffuse_factor = max(dot(normal, light_dir), 0.f);
    vec3 diffuse = sun.diffuse * diffuse_factor * inColor;

    vec3 view_dir = normalize(inViewPos - inFragPos);
    vec3 specular_dir = reflect(-light_dir, normal);
    float specular_factor = pow(max(dot(view_dir, specular_dir), 0.f), 32);
    vec3 specular = sun.specular * specular_factor * inColor;

    vec3 result = ambient + diffuse + specular;
    OutFragColor = vec4(result, 1.0);
}
