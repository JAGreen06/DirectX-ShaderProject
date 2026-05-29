/*SKYBOX VERTEX SHADER*/

#include "Common.hlsli" 

SimplePixelShaderInput main(BasicVertex modelVertex)
{
    SimplePixelShaderInput output; // Data sent over to the pixel shader.

    // Input position is x,y,z only - need a 4th element to multiply by a 4x4 matrix. 1 for a vertex, 0 for a vector.
    float4 modelPosition = float4(modelVertex.position, 1);
    
    float4 worldPosition = mul(gWorldMatrix, modelPosition); //Conversion of a vertex from model space to world space.    
    float4 viewPosition = mul(gViewMatrix, worldPosition); // Converts vertex from world space to view space.
    output.projectedPosition = mul(gProjectionMatrix, viewPosition); // Converts from view space to projection space, stored for use in the pixel shader.
   
    // Renders the skybox behind everything else in the scene.
    output.projectedPosition.z = output.projectedPosition.w - 0.001f; //Pushes skybox to the far plane, accounting for floating point error.   
    
    output.worldPosition = worldPosition.xyz; // World position drops w component so the pixel shader can use it as a direction. used for sampling texture cube in pixel shader.

    // UV is unused when sampling the skybox.
    
    return output; // Output data sent down the pipeline, to the pixel shader.
}
