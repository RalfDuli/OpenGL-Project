#version 330 core

in vec3 color;
in vec3 worldPosition;
in vec3 worldNormal; 

out vec3 finalColor;

uniform vec3 lightPosition;
uniform vec3 lightIntensity;

void main()
{
    // Ambient
    float ambient = 0.2;

    // Diffuse
	vec3 lightDir = normalize(lightPosition - worldPosition);
	float diff = max(dot(normalize(worldNormal), lightDir), 0.0);
	vec3 diffuse = diff * lightIntensity;

	vec3 lighting = ambient + diffuse;

	// Tone mapping
	lighting = lighting / (1 + lighting);

	// Gamma correction
	lighting = pow(lighting, vec3(2.2));

	finalColor = color * lighting;
}
