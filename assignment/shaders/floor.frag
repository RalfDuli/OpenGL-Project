#version 330 core
in vec2 UV;

uniform sampler2D textureSampler;

out vec3 finalColor;

void main() {
    finalColor = texture(textureSampler, UV).rgb;
}
