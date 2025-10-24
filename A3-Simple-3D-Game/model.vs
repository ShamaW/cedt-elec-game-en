#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
// NEW: Bone attributes
layout (location = 3) in ivec4 aBoneIDs;
layout (location = 4) in vec4 aWeights;

out vec2 TexCoords;
out vec3 Normal;
out vec3 FragPos;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

// NEW: Bone matrices
const int MAX_BONES = 100;
uniform mat4 finalBonesMatrices[MAX_BONES];

void main()
{
    // NEW: Calculate animation transform
    mat4 boneTransform = mat4(0.0);
    for(int i = 0 ; i < 4 ; i++)
    {
        if(aBoneIDs[i] == -1)
            continue;
        if(aBoneIDs[i] >= MAX_BONES)
        {
            boneTransform = mat4(1.0);
            break;
        }
        boneTransform += finalBonesMatrices[aBoneIDs[i]] * aWeights[i];
    }
    boneTransform = boneTransform == mat4(0.0) ? mat4(1.0) : boneTransform;


    // Apply animation transform first
    vec4 animPos = boneTransform * vec4(aPos, 1.0);

    // Then apply world transforms
    FragPos = vec3(model * animPos);
    
    // Calculate normals (a bit more complex with animation)
    mat3 normalMatrix = mat3(transpose(inverse(model * boneTransform)));
    Normal = normalMatrix * aNormal;
    
    TexCoords = aTexCoords;
    
    gl_Position = projection * view * model * animPos;
}