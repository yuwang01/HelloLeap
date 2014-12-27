#version 400
layout(location = 0)in vec3 fingerPos;
layout(location = 1)in vec3 fingerColor;

uniform mat4 view, proj;

out vec3 Color;

void main() 
{ 
	Color = fingerColor;
	gl_Position = proj * view * vec4(fingerPos.x, fingerPos.y, fingerPos.z, 1.0);
}