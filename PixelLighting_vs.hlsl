/*PER-PIXEL LIGHTING VERTEX SHADER*/

#include "Common.hlsli"


LightingPixelShaderInput main(BasicVertex modelVertex)
{
    LightingPixelShaderInput output; // Data sent over to the pixel shader.

    // Input position is x,y,z only - need a 4th element to multiply by a 4x4 matrix. Use 1 for a vertex, 0 for a vector.
    float4 modelPosition = float4(modelVertex.position, 1); 

    float4 worldPosition     = mul(gWorldMatrix,      modelPosition); // Transform model vertex position to world space.
    float4 viewPosition      = mul(gViewMatrix,       worldPosition); // Transform vertex from world space into view space.
    output.projectedPosition = mul(gProjectionMatrix, viewPosition); // Transform vertex to clip space.

    // Also transform model normals into world space using world matrix, as lighting is calculated in world space.
    // Pass this normal to the pixel shader as it is needed to calculate per-pixel lighting
    float4 modelNormal = float4(modelVertex.normal, 0); // Add a 0 in the 4th element for normals, needs to be in 4x4 matrix format for multiplication.
    output.worldNormal = mul(gWorldMatrix, modelNormal).xyz; // Multiplies by world matrix, then disregards the 0.
                                                             
    output.worldPosition = worldPosition.xyz; // Also pass world position to pixel shader for lighting

    // Pass texture coordinates (UVs) on to the pixel shader, the vertex shader doesn't need them
    output.uv = modelVertex.uv;

    return output; // Output data sent down the pipeline, to the pixel shader.
}
