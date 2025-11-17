#version 330 core
out vec4 FragColor;

// Uniform to set the color from the C++ code
uniform vec3 ourColor;

void main()
{
    FragColor = vec4(ourColor, 1.0);
}