#version 330 core

in vec2 UV;
in vec3 worldPosition;
in vec3 worldNormal;

uniform sampler2D textureSampler;
uniform vec3 lightPosition;
uniform vec3 lightIntensity;

out vec3 finalColor;

void main() {
    vec3 ambient = vec3(0.2);

    vec3 lightDir = normalize(lightPosition - worldPosition);
    float diff = max(dot(normalize(worldNormal), lightDir), 0.0);
    vec3 diffuse = diff * lightIntensity;

    vec3 lighting = ambient + diffuse;

    // Tone mapping
    lighting = lighting / (1 + lighting);

    // Gamma correction
    lighting = pow(lighting, vec3(2.2));

    finalColor = texture(textureSampler, UV).rgb;
    finalColor = finalColor * lighting;
}