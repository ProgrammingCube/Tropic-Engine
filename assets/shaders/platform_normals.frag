#version 330 core
in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 objectColor;
uniform vec3 ambientColor;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);

    vec3 ambient = ambientColor * objectColor;
    vec3 diffuse = diff * objectColor;

    FragColor = vec4(ambient + diffuse, 1.0);
}
