#version 450

layout(binding = 1) uniform LightInfo {
    vec3 lightPos;
    vec3 lightColor;
    vec3 camPos;
} light;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main() {
    // 材质颜色
    vec3 objectColor = vec3(0.0, 1.0, 0.0);

    // 环境光
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * light.lightColor; 

    // 漫反射
    float diffuseStrength = 0.5;
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(light.lightPos  - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diffuseStrength * diff * light.lightColor; 

    // 镜面反射
    float specularStrength = 1.0;
    vec3 viewDir = normalize(light.camPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    if(diff == 0.0) {
        spec = 0.0;
    }
    vec3 specular = specularStrength * spec * light.lightColor; 

    // 最终颜色
    vec3 result = (ambient + diffuse + specular) * objectColor;
    outColor = vec4(result, 1.0);

}