#version 330 core

in vec3 color;
in vec2 uv;

uniform sampler2D textureSampler;
out vec3 finalColor;

void main()
{
    finalColor = vec3(255, 0, 0);
    finalColor = texture(textureSampler, uv).rgb;
}