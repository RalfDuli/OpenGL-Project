#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 ids;
layout(location = 3) in vec4 weights;

//uniform mat4 projectionView;
//uniform mat4 model;
uniform mat4 MVP;

out vec2 UV;
out vec3 worldPosition;
out vec3 worldNormal;

void main()
{
    gl_Position = MVP * vec4(position, 1.0);

    worldPosition = position;
    worldNormal = normal;

    //gl_Position = projectionView * model * vec4(position, 1.0);
}