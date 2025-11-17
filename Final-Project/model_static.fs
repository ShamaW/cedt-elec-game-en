#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D texture_diffuse1; // Convention used by the model loader

void main()
{
    // Sample the texture
    FragColor = texture(texture_diffuse1, TexCoords);
}