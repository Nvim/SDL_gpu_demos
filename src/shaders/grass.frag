#version 450 core

layout(location = 0) out vec4 OutFragColor;
layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inViewPos;
layout(location = 2) in vec3 inFragPos;
layout(location = 3) in vec3 inNormal;

struct DirLight {
    vec3 direction;
    float pad_1;
    vec3 ambient;
    float pad_2;
    vec3 diffuse;
    float pad_3;
    vec3 specular;
};

layout(std140, set = 3, binding = 0) uniform uDirLight {
    DirLight sun;
};

void main()
{
    vec3 normal = normalize(inNormal);

    float den = 1.5f;
    float halfden = (1.f / 1.5f) / 2.f;
    float y = inFragPos.y / den; // [0,1] -> [0,0.5]
    float color_fact = y - halfden; // [0,0.5] -> [-0.25,0.25]

    // OutFragColor = vec4(inColor + color_fact, 1.0);
    OutFragColor = vec4(inColor, 1.0);

    // vec3 ambient = sun.ambient * color;
    //
    // vec3 light_dir = normalize(sun.direction);
    // float diffuse_factor = max(dot(normal, light_dir), 0.f);
    // vec3 diffuse = sun.diffuse * diffuse_factor * color;
    //
    // vec3 view_dir = normalize(inViewPos - inFragPos);
    // vec3 specular_dir = reflect(-light_dir, normal);
    // float specular_factor = pow(max(dot(view_dir, specular_dir), 0.f), 32);
    // vec3 specular = sun.specular * specular_factor * color;
    //
    // vec3 result = ambient + diffuse + specular;
}
