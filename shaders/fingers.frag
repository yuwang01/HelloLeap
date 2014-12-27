#version 400
in vec3 Color;
out vec4 frag_color;

void main() 
{ 
	//frag_color = vec4(Color, 1.0);
	frag_color = vec4(0.0, 1.0, 0.0, 1.0);
	// frag_color = vec4(0.0, 1.0, 0.0, 1.0);
	//frag_color = vec4(.0, 1.0, .0, 1.0);
}