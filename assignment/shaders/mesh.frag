#version 330 core
out vec4 FragColor;

in vec3 worldPosition;
in vec3 worldNormal;

//uniform sampler2D textureSampler;
uniform vec3 material_diffuse;

void main()
{
    //FragColor = texture(texture_normal1, UV);
    FragColor = vec4(material_diffuse, 1);
}