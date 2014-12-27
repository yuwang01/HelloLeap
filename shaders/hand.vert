#version 400
in vec3 vp;
in vec3 vColor;

out vec3 Color;

void main() 
{ 
	Color = vColor;
	gl_Position = vec4(vp.x, vp.y, vp.z, 1.0);
}