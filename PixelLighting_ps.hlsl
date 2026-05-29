/*PER-PIXEL LIGHTING PIXEL SHADER*/

#include "Common.hlsli"


Texture2D DiffuseSpecularMap : register(t0); // Texture for given model passed from C++
TextureCube Skybox           : register(t1);
SamplerState TexSampler      : register(s0);

Texture2D ShadowMapLight1 : register(t2); // Texture holding the view of the scene from a light
SamplerState PointClamp : register(s1);


float4 main(LightingPixelShaderInput input) : SV_Target
{
    // Normal might have been scaled by model scaling or interpolation so renormalise
    input.worldNormal = normalize(input.worldNormal); 
   
    // Direction from pixel to camera
    float3 cameraDirection = normalize(gCameraPosition - input.worldPosition);

	//// Light 1 ////

	// Direction and distance from pixel to light
	float3 light1Direction = normalize(gLight1Position - input.worldPosition); // Normalised direction of the surface to the position of the light.
    float3 light1Dist = length(gLight1Position - input.worldPosition); // Distance between the light and surface (used for attenuation).
    
    
    float3 diffuseLight1 = gLight1Colour * max(dot(input.worldNormal, light1Direction), 0) / light1Dist; // Lambertian diffuse calculation.
    float3 halfway = normalize(light1Direction + cameraDirection); // used for blinn-phong specular model, calculates the halfway normal between the camera and light.
    float3 specularLight1 =  diffuseLight1 * pow(max(dot(input.worldNormal, halfway), 0), gSpecularPower);  // Blinn-Phong calculation.


	//// Light 2 ////

	float3 light2Direction = normalize(gLight2Position - input.worldPosition); // Same calculations as light 1, see above.
    float3 light2Dist = length(gLight2Position - input.worldPosition);
    
    float3 diffuseLight2 = gLight2Colour * max(dot(input.worldNormal, light2Direction), 0) / light2Dist; 
    halfway = normalize(light2Direction + cameraDirection);
    float3 specularLight2 =  diffuseLight2 * pow(max(dot(input.worldNormal, halfway), 0), gSpecularPower);


    //// Light 3 ////
    const float depthAdjust = 0.0005; // Prevents shadow acne, self shadowing caused by floating point error.
    
    float3 diffuseLight3 = 0;
    float3 specularLight3 = 0;
    
    float3 light3Direction = normalize(gLight3Position - input.worldPosition);
    
    if (dot(gLight3Facing, -light3Direction) > gLight3CosHalfAngle) // Spotlight cone check, compares cosine values in dot product, checks if a pixel is within the view cone.
    {
        float4 light3ViewPosition = mul(gLight3ViewMatrix, float4(input.worldPosition, 1.0f)); // Transforms the world position into light space.
        float4 light3Projection = mul(gLight3ProjectionMatrix, light3ViewPosition); // Transforms the light space into clip space.
        
        float2 shadowMapUV = 0.5f * light3Projection.xy / light3Projection.w + float2(0.5f, 0.5f); //Remaps clip space coordinates to uv coordinates. standard NDC TO UV.
        shadowMapUV.y = 1.0f - shadowMapUV.y; // DirectX Y clip space points up, UV points down flips values so it is correctly mirrored.
        
        float depthFromLight = light3Projection.z / light3Projection.w - depthAdjust; // Converts projected depth value from clip space into normalised device coordinates, gives a comparable depth value.
        
        if (depthFromLight < ShadowMapLight1.Sample(PointClamp, shadowMapUV).r) // Shadow depth test, if the pixels depth from the light is less than depth in shadow map, pixel should be lit.
        {
            // same calculations as lights 1 and 2, see above.
            float3 light3Dist = length(gLight3Position - input.worldPosition);
            diffuseLight3 = gLight3Colour * max(dot(input.worldNormal, light3Direction), 0) / light3Dist;
            float3 halfway = normalize(light3Direction + cameraDirection);
            specularLight3 = diffuseLight3 * pow(max(dot(input.worldNormal, halfway), 0), gSpecularPower);
        }
    }
    
    // Sum the effect of the lights, adding ambient lighting.
    float3 diffuseLight = gAmbientColour + diffuseLight1 + diffuseLight2 + diffuseLight3;
    float3 specularLight = specularLight1 + specularLight2 + specularLight3;
    

    // Sample diffuse material and specular material colour for this pixel from a texture using a given sampler.
    float4 textureColour = DiffuseSpecularMap.Sample(TexSampler, input.uv);
    float3 diffuseMaterialColour = textureColour.rgb; // Diffuse material colour in texture RGB (base colour of model)
    float specularMaterialColour = textureColour.a;   // Specular material colour in texture A (shininess of the surface)

    //Reflection Calculations
    float3 viewDirection = normalize(input.worldPosition - gCameraPosition); // Direction of camera to the surface.
    
    float3 reflectionDirection = reflect(viewDirection, input.worldNormal); // Calculates view direction that bounces off the surface, given the surface normal and view angle.
    
    float3 reflectionColour = Skybox.Sample(TexSampler, reflectionDirection).rgb; // Samples texel from skybox texture, sampling in the given direction gives what the surface is looking at.
    
    // Combine lighting with texture colours
    float3 lightingColour = diffuseLight * diffuseMaterialColour + specularLight * specularMaterialColour;

    float3 finalColour = lerp(lightingColour, reflectionColour, gReflectivity); // reflectivity of 0 = full lighting, reflectivity of 1 = no lighting, values inbetween blend proportionally.
    
    return float4(finalColour, 1.0f);
}