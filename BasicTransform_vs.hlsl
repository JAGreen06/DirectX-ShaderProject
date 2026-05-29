/*BASIC TRANSFORM VERTEX SHADER*/

// Used for light model rendering and depth only shadow pass.

#include "Common.hlsli"

SimplePixelShaderInput main(BasicVertex modelVertex)
{
    SimplePixelShaderInput output; // Data passed to pixel shader

    // Input position is x,y,z only - need a 4th element to multiply by a 4x4 matrix. Use 1 for a vertex, 0 for a vector.
    float4 modelPosition = float4(modelVertex.position, 1); 

    float4 worldPosition     = mul(gWorldMatrix,      modelPosition); // Transform model vertex to world space.
    float4 viewPosition      = mul(gViewMatrix,       worldPosition); // Transform world vertex to view space.
    output.projectedPosition = mul(gProjectionMatrix, viewPosition); // Transform view vertex to clip space.

    // Pass texture coordinates (UVs) on to the pixel shader
    output.uv = modelVertex.uv;

    return output; // Output data sent down the pipeline, to the pixel shader.
}
