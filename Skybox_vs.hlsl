//--------------------------------------------------------------------------------------
// Light Model Vertex Shader
//--------------------------------------------------------------------------------------
// Basic matrix transformations only

#include "Common.hlsli" // Shaders can also use include files - note the extension


//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Vertex shader gets vertices from the mesh one at a time. It transforms their positions
// from 3D into 2D (see lectures) and passes that position down the pipeline so pixels can be rendered. 
SimplePixelShaderInput main(BasicVertex modelVertex)
{
    SimplePixelShaderInput output; // This is the data the pixel shader requires from this vertex shader

    // Input position is x,y,z only - need a 4th element to multiply by a 4x4 matrix. Use 1 for a point (0 for a vector) - recall lectures
    float4 modelPosition = float4(modelVertex.position, 1);
    
    float4 worldPosition = mul(gWorldMatrix, modelPosition);   
    
    float4 viewPosition = mul(gViewMatrix, worldPosition);
    output.projectedPosition = mul(gProjectionMatrix, viewPosition);
   
    output.projectedPosition.z = output.projectedPosition.w - 0.001f; //Pushes skybox to the far plane, account for floating point error.   
    
    output.worldPosition = worldPosition.xyz;

    return output; // Ouput data sent down the pipeline (to the pixel shader)
}
