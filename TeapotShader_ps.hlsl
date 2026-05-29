/*TEAPOT PIXEL SHADER*/

#include "Common.hlsli"

Texture2D DiffuseMap : register(t0); //Teapot texture, passed from C++.
                                        
SamplerState TexSampler : register(s0); // Anisotripic filtering, high quality texture filtering. (Defined in C++)


float4 main(SimplePixelShaderInput input) : SV_Target
{
    // Sample diffuse material colour for this pixel from a texture using a given sampler.
    float3 diffuseMapColour = DiffuseMap.Sample(TexSampler, input.uv).rgb;
    
    // Equation for smooth changing of colours, gTime passed via constant buffer from C++.
    float red = (sin(gTime) + 1.0f) / 2.0f; 
    float green = (sin(gTime * 0.7f) + 1.0f) / 2.0f;
    float blue = (sin(gTime * 1.3f) + 1.0f) / 2.0f;
    
    // Combine each individual colour channel into one final colour.
    float3 colours = float3(red, green, blue);
    
    // modulates texture colour with colour outputted from smooth changing formula.
    float3 finalColour = diffuseMapColour * colours;

    return float4(finalColour, 1.0f);
}