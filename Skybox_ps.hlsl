/*SKYBOX PIXEL SHADER*/

#include "Common.hlsli" 

TextureCube DiffuseMap : register(t0); // Texture Cube for the skybox, passed from C++
                                        
SamplerState TexSampler : register(s0); // Skybox Sampler, passed from C++


float4 main(SimplePixelShaderInput input) : SV_Target
{
    // Sample diffuse material colour for this pixel from a texture cube using a sampler setup in C++.
    // Sampled using a 3D direction vector, rather than UV coordinates. 
    // WorldPosition is used as skybox is centred around the camera.
    float3 finalColour = DiffuseMap.Sample(TexSampler, input.worldPosition); 
     
    return float4(finalColour, 1.0f);
}