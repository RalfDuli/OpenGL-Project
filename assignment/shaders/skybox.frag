#version 330 core
in vec3 TexCoords;
out vec3 FragColor;

uniform samplerCube skybox;

void main() {
    FragColor = texture(skybox, TexCoords).rgb;
}
