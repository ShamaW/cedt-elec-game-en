#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;

// The LearnOpenGL Model class automatically names diffuse textures 'texture_diffuse1'
uniform sampler2D texture_diffuse1;

void main()
{
    // For now, just output the texture.
    // We'll add lighting later.
    FragColor = texture(texture_diffuse1, TexCoords);
}