//--------------------------------------------------------------------------------------
// Per-Pixel Lighting Pixel Shader
//--------------------------------------------------------------------------------------
// Pixel shader receives position and normal from the vertex shader and uses them to calculate
// lighting per pixel. Also samples a samples a diffuse + specular texture map and combines with light colour.

#include "Common.hlsli" // Shaders can also use include files - note the extension


//--------------------------------------------------------------------------------------
// Textures (texture maps)
//--------------------------------------------------------------------------------------

// Here we allow the shader access to a texture that has been loaded from the C++ side and stored in GPU memory.
// Note that textures are often called maps (because texture mapping describes wrapping a texture round a mesh).
// Get used to people using the word "texture" and "map" interchangably.
Texture2D DiffuseSpecularMap : register(t0); // Textures here can contain a diffuse map (main colour) in their rgb channels and a specular map (shininess) in the a channel
TextureCube Skybox           : register(t1);
SamplerState TexSampler      : register(s0); // A sampler is a filter for a texture like bilinear, trilinear or anisotropic - this is the sampler used for the texture above

Texture2D ShadowMapLight1 : register(t2); // Texture holding the view of the scene from a light
SamplerState PointClamp : register(s1);

//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Pixel shader entry point - each shader has a "main" function
// This shader just samples a diffuse texture map
float4 main(LightingPixelShaderInput input) : SV_Target
{
    // Normal might have been scaled by model scaling or interpolation so renormalise
    input.worldNormal = normalize(input.worldNormal); 

	///////////////////////
	// Calculate lighting
    
    // Direction from pixel to camera
    float3 cameraDirection = normalize(gCameraPosition - input.worldPosition);

	//// Light 1 ////

	// Direction and distance from pixel to light
	float3 light1Direction = normalize(gLight1Position - input.worldPosition);
    float3 light1Dist = length(gLight1Position - input.worldPosition);
    
    // Equations from lighting lecture
    float3 diffuseLight1 = gLight1Colour * max(dot(input.worldNormal, light1Direction), 0) / light1Dist; 
    float3 halfway = normalize(light1Direction + cameraDirection);
    float3 specularLight1 =  diffuseLight1 * pow(max(dot(input.worldNormal, halfway), 0), gSpecularPower); // Multiplying by diffuseLight instead of light colour - my own personal preference


	//// Light 2 ////

	float3 light2Direction = normalize(gLight2Position - input.worldPosition);
    float3 light2Dist = length(gLight2Position - input.worldPosition);
    float3 diffuseLight2 = gLight2Colour * max(dot(input.worldNormal, light2Direction), 0) / light2Dist; 
    halfway = normalize(light2Direction + cameraDirection);
    float3 specularLight2 =  diffuseLight2 * pow(max(dot(input.worldNormal, halfway), 0), gSpecularPower);


	// Sum the effect of the lights - add the ambient at this stage rather than for each light (or we will get too much ambient)

    //// Light 3 ////
    const float depthAdjust = 0.0005;
    
    float3 diffuseLight3 = 0;
    float3 specularLight3 = 0;
    
    float3 light3Direction = normalize(gLight3Position - input.worldPosition);
    
    if (dot(gLight3Facing, -light3Direction) > gLight3CosHalfAngle)
    {
        float4 light3ViewPosition = mul(gLight3ViewMatrix, float4(input.worldPosition, 1.0f));
        float4 light3Projection = mul(gLight3ProjectionMatrix, light3ViewPosition);
        
        float2 shadowMapUV = 0.5f * light3Projection.xy / light3Projection.w + float2(0.5f, 0.5f);
        shadowMapUV.y = 1.0f - shadowMapUV.y;
        
        float depthFromLight = light3Projection.z / light3Projection.w - depthAdjust;
        
        if (depthFromLight < ShadowMapLight1.Sample(PointClamp, shadowMapUV).r)
        {
            float3 light3Dist = length(gLight3Position - input.worldPosition);
            diffuseLight3 = gLight3Colour * max(dot(input.worldNormal, light3Direction), 0) / light3Dist; // Equations from lighting lecture
            float3 halfway = normalize(light3Direction + cameraDirection);
            specularLight3 = diffuseLight3 * pow(max(dot(input.worldNormal, halfway), 0), gSpecularPower);
        }
    }
    
    float3 diffuseLight = gAmbientColour + diffuseLight1 + diffuseLight2 + diffuseLight3;
    float3 specularLight = specularLight1 + specularLight2 + specularLight3;
    

	////////////////////
	// Combine lighting and textures

    // Sample diffuse material and specular material colour for this pixel from a texture using a given sampler that you set up in the C++ code
    float4 textureColour = DiffuseSpecularMap.Sample(TexSampler, input.uv);
    float3 diffuseMaterialColour = textureColour.rgb; // Diffuse material colour in texture RGB (base colour of model)
    float specularMaterialColour = textureColour.a;   // Specular material colour in texture A (shininess of the surface)

    //Reflection Calculation
    float3 viewDirection = normalize(input.worldPosition - gCameraPosition);
    
    float3 reflectionDirection = reflect(viewDirection, input.worldNormal);
    
    float3 reflectionColour = Skybox.Sample(TexSampler, reflectionDirection).xyz;
    
    // Combine lighting with texture colours
    float3 lightingColour = diffuseLight * diffuseMaterialColour + specularLight * specularMaterialColour;

    
    float3 finalColour = lerp(lightingColour, reflectionColour, gReflectivity);
    
    return float4(finalColour, 1.0f); // Always use 1.0f for output alpha - no alpha blending in this lab
}