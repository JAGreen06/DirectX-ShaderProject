//--------------------------------------------------------------------------------------
// Scene geometry and layout preparation
// Scene rendering & update
//--------------------------------------------------------------------------------------

#include "Scene.h"
#include "Mesh.h"
#include "Model.h"
#include "Camera.h"
#include "Light.h"
#include "State.h"
#include "Shader.h"
#include "Input.h"
#include "Common.h"

#include "CVector2.h" 
#include "CVector3.h" 
#include "CMatrix4x4.h"
#include "MathHelpers.h"     // Helper functions for maths
#include "GraphicsHelpers.h" // Helper functions to unclutter the code here

#include "ColourRGBA.h" 

#include <sstream>
#include <memory>


//--------------------------------------------------------------------------------------
// Scene Data
//--------------------------------------------------------------------------------------
// Addition of Mesh, Model and Camera classes have greatly simplified this section
// Geometry data has gone to Mesh class. Positions, rotations, matrices have gone to Model and Camera classes

// Constants controlling speed of movement/rotation (measured in units per second because we're using frame time)
const float ROTATION_SPEED = 2.0f;  // 2 radians per second for rotation
const float MOVEMENT_SPEED = 50.0f; // 50 units per second for movement (what a unit of length is depends on 3D model - i.e. an artist decision usually)


// Meshes, models and cameras, same meaning as TL-Engine. Meshes prepared in InitGeometry function, Models & camera in InitScene
Mesh* gAnimatedMesh;
Mesh* gTeapotMesh;
Mesh* gGroundMesh;
Mesh* gLightMesh;
Mesh* gSkyboxMesh;
Mesh* gCrateMesh;

Model* gAnimated;
Model* gTeapot;
Model* gGround;
Model* gSkybox;

Camera* gCamera;

// Store lights in an array.
const int NUM_LIGHTS = 3;
Light gLights[NUM_LIGHTS];

const int NUM_TROLLS = 4;
Model* gCrates[NUM_TROLLS];
float trollX = 0.0f;

// Additional light information
CVector3 gAmbientColour = { 0.2f, 0.2f, 0.3f }; // Background level of light (slightly bluish to match the far background, which is dark blue)
float    gSpecularPower = 256; // Specular power controls shininess - same for all models in this app

ColourRGBA gBackgroundColor = { 0.2f, 0.2f, 0.3f, 1.0f };

// Variables controlling light1's orbiting of the cube
const float gLightOrbit = 20.0f;
const float gLightOrbitSpeed = 0.7f;
float gSpotlightConeAngle = 90.0f; // Spot light cone angle (degrees), like the FOV (field-of-view) of the spot light

// Lock FPS to monitor refresh rate, which will typically set it to 60fps. Press 'p' to toggle to full fps
bool lockFPS = true;

int gShadowMapSize = 16384;


ID3D11Texture2D* gShadowMap1Texture = nullptr;
ID3D11DepthStencilView* gShadowMap1DepthStencil = nullptr; 
ID3D11ShaderResourceView* gShadowMap1SRV = nullptr; 

//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------
// Variables sent over to the GPU each frame
// The structures are now in Common.h
// IMPORTANT: Any new data you add in C++ code (CPU-side) is not automatically available to the GPU
//            Anything the shaders need (per-frame or per-model) needs to be sent via a constant buffer

PerFrameConstants gPerFrameConstants;      // The constants that need to be sent to the GPU each frame (see common.h for structure)
ID3D11Buffer*     gPerFrameConstantBuffer; // The GPU buffer that will recieve the constants above

PerModelConstants gPerModelConstants;      // As above, but constant that change per-model (e.g. world matrix)
ID3D11Buffer*     gPerModelConstantBuffer; // --"--



//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------

// DirectX objects controlling textures used in this lab
ID3D11Resource*           gCharacterDiffuseSpecularMap    = nullptr; // This object represents the memory used by the texture on the GPU
ID3D11ShaderResourceView* gCharacterDiffuseSpecularMapSRV = nullptr; // This object is used to give shaders access to the texture above (SRV = shader resource view)

ID3D11Resource*           gTeapotDiffuseSpecularMap    = nullptr;
ID3D11ShaderResourceView* gTeapotDiffuseSpecularMapSRV = nullptr;

ID3D11Resource*           gGroundDiffuseSpecularMap    = nullptr;
ID3D11ShaderResourceView* gGroundDiffuseSpecularMapSRV = nullptr;

ID3D11Resource*           gLightDiffuseMap    = nullptr;
ID3D11ShaderResourceView* gLightDiffuseMapSRV = nullptr;

ID3D11Resource*           gSkyboxDiffuseMap    = nullptr;
ID3D11ShaderResourceView* gSkyboxDiffuseMapSRV = nullptr;

ID3D11Resource*           gContainerDiffuseMap    = nullptr;
ID3D11ShaderResourceView* gContainerDiffuseMapSRV = nullptr;

//--------------------------------------------------------------------------------------
// Light Helper Functions
//--------------------------------------------------------------------------------------

// Get "camera-like" view matrix for a spotlight
CMatrix4x4 CalculateLightViewMatrix(int lightIndex)
{
    return InverseAffine(gLights[lightIndex].GetModel()->WorldMatrix());
}

// Get "camera-like" projection matrix for a spotlight
CMatrix4x4 CalculateLightProjectionMatrix(int lightIndex)
{
    return MakeProjectionMatrix(1.0f, ToRadians(gSpotlightConeAngle)); // Helper function in Utility\GraphicsHelpers.cpp
}



//--------------------------------------------------------------------------------------
// Initialise scene geometry, constant buffers and states
//--------------------------------------------------------------------------------------

// Prepare the geometry required for the scene
// Returns true on success
bool InitGeometry()
{
    // Load mesh geometry data, just like TL-Engine this doesn't create anything in the scene. Create a Model for that.
    // IMPORTANT NOTE: Will only keep the first object from the mesh - multipart objects will have parts missing - see later lab for more robust loader
    try 
    {
        gAnimatedMesh = new Mesh("robot.x");
        gTeapotMesh    = new Mesh("teapot.x");
        gGroundMesh   = new Mesh("Hills.x");
        gLightMesh    = new Mesh("Light.x");
        gSkyboxMesh = new Mesh("cube.x");
        gCrateMesh = new Mesh("CargoContainer.x");
    }
    catch (std::runtime_error e)  // Constructors cannot return error messages so use exceptions to catch mesh errors (fairly standard approach this)
    {
        gLastError = e.what(); // This picks up the error message put in the exception (see Mesh.cpp)
        return false;
    }


    // Load the shaders required for the geometry we will use (see Shader.cpp / .h)
    if (!LoadShaders())
    {
        gLastError = "Error loading shaders";
        return false;
    }


    // Create GPU-side constant buffers to receive the gPerFrameConstants and gPerModelConstants structures above
    // These allow us to pass data from CPU to shaders such as lighting information or matrices
    // See the comments above where these variable are declared and also the UpdateScene function
    gPerFrameConstantBuffer = CreateConstantBuffer(sizeof(gPerFrameConstants));
    gPerModelConstantBuffer = CreateConstantBuffer(sizeof(gPerModelConstants));
    if (gPerFrameConstantBuffer == nullptr || gPerModelConstantBuffer == nullptr)
    {
        gLastError = "Error creating constant buffers";
        return false;
    }


    //// Load / prepare textures on the GPU ////

    // Load textures and create DirectX objects for them
    // The LoadTexture function requires you to pass a ID3D11Resource* (e.g. &gCubeDiffuseMap), which manages the GPU memory for the
    // texture and also a ID3D11ShaderResourceView* (e.g. &gCubeDiffuseMapSRV), which allows us to use the texture in shaders
    // The function will fill in these pointers with usable data. The variables used here are globals found near the top of the file.
    if (!LoadTexture("MetalDiffuseSpecular.dds", &gCharacterDiffuseSpecularMap, &gCharacterDiffuseSpecularMapSRV) ||
        !LoadTexture("WoodDiffuseSpecular.dds", &gTeapotDiffuseSpecularMap,    &gTeapotDiffuseSpecularMapSRV) ||
        !LoadTexture("GrassDiffuseSpecular.dds", &gGroundDiffuseSpecularMap,    &gGroundDiffuseSpecularMapSRV   ) ||
        !LoadTexture("CargoA.dds", &gContainerDiffuseMap, &gContainerDiffuseMapSRV) ||
        !LoadTexture("Flare.jpg",                &gLightDiffuseMap,             &gLightDiffuseMapSRV) ||
        !LoadTexture("skybox.dds",              &gSkyboxDiffuseMap,             &gSkyboxDiffuseMapSRV))
    {
        gLastError = "Error loading textures";
        return false;
    }

    //**** Create Shadow Map texture ****//

    // We also need a depth buffer to go with our portal
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = gShadowMapSize; // Size of the shadow map determines quality / resolution of shadows
    textureDesc.Height = gShadowMapSize;
    textureDesc.MipLevels = 1; // 1 level, means just the main texture, no additional mip-maps. Usually don't use mip-maps when rendering to textures (or we would have to render every level)
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R32_TYPELESS; // The shadow map contains a single 32-bit value [tech gotcha: have to say typeless because depth buffer and shaders see things slightly differently]
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D10_BIND_DEPTH_STENCIL | D3D10_BIND_SHADER_RESOURCE; // Indicate we will use texture as a depth buffer and also pass it to shaders
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;
    if (FAILED(gD3DDevice->CreateTexture2D(&textureDesc, NULL, &gShadowMap1Texture)))
    {
        gLastError = "Error creating shadow map texture";
        return false;
    }

    //Create the depth stencil view, i.e.indicate that the texture just created is to be used as a depth buffer
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; // See "tech gotcha" above. The depth buffer sees each pixel as a "depth" float
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    dsvDesc.Flags = 0;
    if (FAILED(gD3DDevice->CreateDepthStencilView(gShadowMap1Texture, &dsvDesc, &gShadowMap1DepthStencil)))
    {
        gLastError = "Error creating shadow map depth stencil view";
        return false;
    }

    // We also need to send this texture (resource) to the shaders. To do that we must create a shader-resource "view"
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT; // See "tech gotcha" above. The shaders see textures as colours, so shadow map pixels are not seen as depths
    // but rather as "red" floats (one float taken from RGB). Although the shader code will use the value as a depth
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    if (FAILED(gD3DDevice->CreateShaderResourceView(gShadowMap1Texture, &srvDesc, &gShadowMap1SRV)))
    {
        gLastError = "Error creating shadow map shader resource view";
        return false;
    }


  	// Create all filtering modes, blending modes etc. used by the app (see State.cpp/.h)
	if (!CreateStates())
	{
		gLastError = "Error creating states";
		return false;
	}

	return true;
}


// Prepare the scene
// Returns true on success
bool InitScene()
{
    //// Set up scene ////

    gAnimated = new Model(gAnimatedMesh);
    gGround = new Model(gGroundMesh);
    gTeapot = new Model(gTeapotMesh);
    gSkybox = new Model(gSkyboxMesh);


    for (int i = 0; i < NUM_TROLLS; i++)
    {
        gCrates[i] = new Model(gCrateMesh);
        gCrates[i]->SetScale(5);
    }

    gCrates[0]->SetPosition({ 50.0f, 1.0f, -100.0f });
    gCrates[0]->SetRotation({ 0, ToRadians(135.0f), 0 });
    gCrates[1]->SetPosition({ -20.0f, 10.0f, 10.0f });
    gCrates[2]->SetPosition({ -75.f, 0.0f, 150.0f });
    gCrates[3]->SetPosition({ 100.0f, 15.0f, -10.0f });

    gSkybox->SetPosition({ 0, 0, 0 });
    gSkybox->SetScale(1000);


    // Initial positions
    gAnimated->SetPosition({ 20, 1.0f, 0 });
    gAnimated->SetScale(5); // Increase model scale to 5.0 for the robot, which has been drawn at a different scale
    gAnimated->SetRotation({ 0, ToRadians(135.0f), 0 });

    gTeapot->SetPosition({ 50, 0, 70 });
    gTeapot->SetScale(2);
    gTeapot->SetRotation({ 0, ToRadians(180.0f), 0 });

    // Light set-up - using an array this time
    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        gLights[i].SetModel(new Model(gLightMesh));
    }

    gLights[0].SetColour({ 0.8f, 0.8f, 1.0f });
    gLights[0].SetStrength(10);
    gLights[0].GetModel()->SetPosition({ 30, 20, 0 });
    gLights[0].GetModel()->SetScale(pow(gLights[0].GetStrength(), 0.7f)); // Convert light strength into a nice value for the scale of the light - equation is ad-hoc.

    gLights[1].SetColour({ 1.0f, 0.8f, 0.2f });
    gLights[1].SetStrength(40);
    gLights[1].GetModel()->SetPosition({ -20, 50, 20 });
    gLights[1].GetModel()->SetScale(pow(gLights[1].GetStrength(), 0.7f));

    gLights[2].SetColour({ 0.8f, 0.8f, 1.0f });
    gLights[2].SetStrength(60);
    gLights[2].GetModel()->SetPosition({10.0f, 40, -70.0f});
    gLights[2].GetModel()->SetScale(pow(gLights[2].GetStrength(), 0.7f)); // Convert light strength into a nice value for the scale of the light - equation is ad-hoc.  

    //// Set up camera ////

    gCamera = new Camera();
    gCamera->SetPosition({ 15, 30,-70 });
    gCamera->SetRotation({ ToRadians(13), 0, 0 });

    return true;
}


// Release the geometry and scene resources created above
void ReleaseResources()
{
    ReleaseStates();

    if (gLightDiffuseMap)             gLightDiffuseMapSRV->Release();
    if (gLightDiffuseMap)                gLightDiffuseMap->Release();
    if (gContainerDiffuseMap)             gContainerDiffuseMap->Release();
    if (gContainerDiffuseMapSRV)             gContainerDiffuseMapSRV->Release();
    if (gGroundDiffuseSpecularMapSRV)    gGroundDiffuseSpecularMapSRV->Release();
    if (gGroundDiffuseSpecularMap)       gGroundDiffuseSpecularMap->Release();
    if (gTeapotDiffuseSpecularMapSRV)     gTeapotDiffuseSpecularMapSRV->Release();
    if (gTeapotDiffuseSpecularMap)        gTeapotDiffuseSpecularMap->Release();
    if (gCharacterDiffuseSpecularMapSRV) gCharacterDiffuseSpecularMapSRV->Release();
    if (gCharacterDiffuseSpecularMap)    gCharacterDiffuseSpecularMap->Release();

    if (gSkyboxDiffuseMap)           gSkyboxDiffuseMap->Release();
    if (gSkyboxDiffuseMapSRV)         gSkyboxDiffuseMapSRV->Release();


    if (gPerModelConstantBuffer)  gPerModelConstantBuffer->Release();
    if (gPerFrameConstantBuffer)  gPerFrameConstantBuffer->Release();

    ReleaseShaders();

    // See note in InitGeometry about why we're not using unique_ptr and having to manually delete
    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        delete gLights[i].GetModel();  gLights[i].SetModel(nullptr);
    }

    for (int i = 0; i < NUM_TROLLS; i++)
    {
        delete gCrates[i]; gCrates[i] == nullptr;
    }

    delete gCamera;    gCamera    = nullptr;
    delete gGround;    gGround    = nullptr;
    delete gTeapot;    gTeapot    = nullptr;
    delete gAnimated;  gAnimated  = nullptr;
    delete gSkybox;    gSkybox    = nullptr;

    delete gLightMesh;     gLightMesh     = nullptr;
    delete gGroundMesh;    gGroundMesh    = nullptr;
    delete gTeapotMesh;    gTeapotMesh    = nullptr;
    delete gAnimatedMesh;  gAnimatedMesh  = nullptr;
    delete gSkyboxMesh;    gSkyboxMesh    = nullptr;
}



//--------------------------------------------------------------------------------------
// Scene Rendering
//--------------------------------------------------------------------------------------

void RenderDepthBufferFromLight(int lightIndex)
{
    gPerFrameConstants.viewMatrix = CalculateLightViewMatrix(lightIndex);
    gPerFrameConstants.projectionMatrix = CalculateLightProjectionMatrix(lightIndex);
    gPerFrameConstants.viewProjectionMatrix = gPerFrameConstants.viewMatrix * gPerFrameConstants.projectionMatrix;
    UpdateConstantBuffer(gPerFrameConstantBuffer, gPerFrameConstants);

    // Indicate that the constant buffer we just updated is for use in the vertex shader (VS) and pixel shader (PS)
    gD3DContext->VSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer); // First parameter must match constant buffer number in the shader 
    gD3DContext->PSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);


    //// Only render models that cast shadows ////

    // Use special depth-only rendering shaders
    gD3DContext->VSSetShader(gBasicTransformVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gDepthOnlyPixelShader, nullptr, 0);

    // States - no blending, normal depth buffer and culling
    gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
    gD3DContext->RSSetState(gCullFrontState);

    // Render models - no state changes required between each object in this situation (no textures used in this step)
    gGround->Render();
    gAnimated->Render();
    for (int i = 0; i < NUM_TROLLS; i++)
    {
        gCrates[i]->Render();
    }
}

// Render everything in the scene from the given camera
void RenderSceneFromCamera(Camera* camera)
{
    // Set camera matrices in the constant buffer and send over to GPU
    gPerFrameConstants.viewMatrix           = camera->ViewMatrix();
    gPerFrameConstants.projectionMatrix     = camera->ProjectionMatrix();
    gPerFrameConstants.viewProjectionMatrix = camera->ViewProjectionMatrix();
    UpdateConstantBuffer(gPerFrameConstantBuffer, gPerFrameConstants);

    // Indicate that the constant buffer we just updated is for use in the vertex shader (VS) and pixel shader (PS)
    gD3DContext->VSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer); // First parameter must match constant buffer number in the shader 
    gD3DContext->PSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);


    //// Render lit models ////

    // Select which shaders to use next
    gD3DContext->VSSetShader(gPixelLightingVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gPixelLightingPixelShader,  nullptr, 0);
    
    // States - no blending, normal depth buffer and culling
    gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
    gD3DContext->RSSetState(gCullBackState);

    gD3DContext->PSSetShaderResources(1, 1, &gSkyboxDiffuseMapSRV); // First parameter must match texture slot number in the shader

    // Select the approriate textures and sampler to use in the pixel shader
    gD3DContext->PSSetShaderResources(0, 1, &gGroundDiffuseSpecularMapSRV); // First parameter must match texture slot number in the shader
    gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);

    // Render model - it will update the model's world matrix and send it to the GPU in a constant buffer, then it will call
    // the Mesh render function, which will set up vertex & index buffer before finally calling Draw on the GPU
    gPerModelConstants.gReflectivity = 0.0f;
    gGround->Render();

    // Render other lit models, only change textures for each onee
    gD3DContext->PSSetShaderResources(0, 1, &gCharacterDiffuseSpecularMapSRV); 
    gPerModelConstants.gReflectivity = 0.12f;
    gAnimated->Render();



    // Render other lit models, only change textures for each onee
    gD3DContext->PSSetShaderResources(0, 1, &gContainerDiffuseMapSRV);
    for (int i = 0; i < NUM_TROLLS; i++)
    {
        gPerModelConstants.gReflectivity = 0.12f;
        gCrates[i]->Render();
    }


    //// Render lights ////

    // Select which shaders to use next
    gD3DContext->VSSetShader(gBasicTransformVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gLightModelPixelShader,      nullptr, 0);

    // Select the texture and sampler to use in the pixel shader
    gD3DContext->PSSetShaderResources(0, 1, &gLightDiffuseMapSRV); // First parameter must match texture slot number in the shaer
    gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);

    // States - additive blending, read-only depth buffer and no culling (standard set-up for blending
    gD3DContext->OMSetBlendState(gAdditiveBlendingState, nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
    gD3DContext->RSSetState(gCullNoneState);

    // Render all the lights in the array
    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        gPerModelConstants.objectColour = gLights[i].GetColour(); // Set any per-model constants apart from the world matrix just before calling render (light colour here)
        gLights[i].GetModel()->Render();
    }
}




// Rendering the scene
void RenderScene()
{
    //// Common settings ////

    // Set up the light information in the constant buffer
    // Don't send to the GPU yet, the function RenderSceneFromCamera will do that
    gPerFrameConstants.light1Colour   = gLights[0].GetColour() * gLights[0].GetStrength();
    gPerFrameConstants.light1Position = gLights[0].GetModel()->Position();

    gPerFrameConstants.light2Colour   = gLights[1].GetColour() * gLights[1].GetStrength();
    gPerFrameConstants.light2Position = gLights[1].GetModel()->Position();

    gPerFrameConstants.light3Colour = gLights[2].GetColour() * gLights[2].GetStrength();
    gPerFrameConstants.light3Position = gLights[2].GetModel()->Position();
    gPerFrameConstants.light3Facing = Normalise(gLights[2].GetModel()->WorldMatrix().GetZAxis());    // Additional lighting information for spotlights
    gPerFrameConstants.light3CosHalfAngle = cos(ToRadians(gSpotlightConeAngle / 2)); // --"--
    gPerFrameConstants.light3ViewMatrix = CalculateLightViewMatrix(2);         // Calculate camera-like matrices for...
    gPerFrameConstants.light3ProjectionMatrix = CalculateLightProjectionMatrix(2);   //...lights to support shadow mapping


    gPerFrameConstants.ambientColour  = gAmbientColour;
    gPerFrameConstants.specularPower  = gSpecularPower;
    gPerFrameConstants.cameraPosition = gCamera->Position();


    //***************************************//
    //// Render from light's point of view ////

    // Only rendering from light 1 to begin with

    // Setup the viewport to the size of the shadow map texture
    D3D11_VIEWPORT vp;
    vp.Width = static_cast<FLOAT>(gShadowMapSize);
    vp.Height = static_cast<FLOAT>(gShadowMapSize);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    gD3DContext->RSSetViewports(1, &vp);

    // Select the shadow map texture as the current depth buffer. We will not be rendering any pixel colours
    // Also clear the the shadow map depth buffer to the far distance
   gD3DContext->OMSetRenderTargets(0, nullptr, gShadowMap1DepthStencil);
    gD3DContext->ClearDepthStencilView(gShadowMap1DepthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Render the scene from the point of view of light 1 (only depth values written)
    RenderDepthBufferFromLight(2); 

    //// Main scene rendering ////

    // Set the back buffer as the target for rendering and select the main depth buffer.
    // When finished the back buffer is sent to the "front buffer" - which is the monitor.
    gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);

    // Clear the back buffer to a fixed colour and the depth buffer to the far distance
    gD3DContext->ClearRenderTargetView(gBackBufferRenderTarget, &gBackgroundColor.r);
    gD3DContext->ClearDepthStencilView(gDepthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Setup the viewport to the size of the main window
    vp.Width  = static_cast<FLOAT>(gViewportWidth);
    vp.Height = static_cast<FLOAT>(gViewportHeight);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    gD3DContext->RSSetViewports(1, &vp);

    gD3DContext->PSSetShaderResources(2, 1, &gShadowMap1SRV);
    gD3DContext->PSSetSamplers(1, 1, &gTrilinearSampler);


    // Render the scene from the main camera
    RenderSceneFromCamera(gCamera);

    //Render Teapot.
    gD3DContext->VSSetShader(gBasicTransformVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gTeapotPixelShader, nullptr, 0);

    gD3DContext->PSSetShaderResources(0, 1, &gTeapotDiffuseSpecularMapSRV); // First parameter must match texture slot number in the shaer
    gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);

    gD3DContext->OMSetBlendState(gAdditiveBlendingState, nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
    gD3DContext->RSSetState(gCullNoneState);

    gTeapot->Render();

    //Render Skybox
    gD3DContext->VSSetShader(gSkyboxVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gSkyboxPixelShader, nullptr, 0);

    gD3DContext->PSSetShaderResources(0, 1, &gSkyboxDiffuseMapSRV);
    gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);

    gD3DContext->OMSetDepthStencilState(gLessEqualDepthState, 0);
    gD3DContext->RSSetState(gCullNoneState);

    gSkybox->Render();

    //// Scene completion ////

    // When drawing to the off-screen back buffer is complete, we "present" the image to the front buffer (the screen)
    // Set first parameter to 1 to lock to vsync (typically 60fps)
    gSwapChain->Present(lockFPS ? 1 : 0, 0);
}


//--------------------------------------------------------------------------------------
// Scene Update
//--------------------------------------------------------------------------------------

// Update models and camera. frameTime is the time passed since the last frame
void UpdateScene(float frameTime)
{
    gPerFrameConstants.time += frameTime;
	// Control sphere (will update its world matrix)
    gAnimated->Control(0, frameTime, Key_I, Key_K, Key_J, Key_L, Key_U, Key_O, Key_Period, Key_Comma); 	

    // Orbit the light - a bit of a cheat with the static variable [ask the tutor if you want to know what this is]
	static float rotate = 0.0f;
    static bool go = true;
	gLights[0].GetModel()->SetPosition(gAnimated->Position() + CVector3{cos(rotate) * gLightOrbit, 10, sin(rotate) * gLightOrbit});   

    if (go)  rotate -= gLightOrbitSpeed * frameTime;
    if (KeyHit(Key_1))  go = !go;

	// Control camera (will update its view matrix)
	gCamera->Control(frameTime, Key_Up, Key_Down, Key_Left, Key_Right, Key_W, Key_S, Key_A, Key_D );

   
    //Switches colours, and on off.
    gLights[0].UpdateColour(frameTime);
    gLights[1].UpdateToggle(frameTime);


    //allows for changing of colours on the spotlight
    static bool go2 = false;
    if (go2) { gLights[2].UpdateColour(frameTime); }
    else { gLights[2].SetColour({ 0.8f, 0.8f, 1.0f }); }
    
    if (KeyHit(Key_2))  go2 = !go2;

    // Toggle FPS limiting
    if (KeyHit(Key_P))  lockFPS = !lockFPS;

    // Show frame time / FPS in the window title //
    const float fpsUpdateTime = 0.5f; // How long between updates (in seconds)
    static float totalFrameTime = 0;
    static int frameCount = 0;
    totalFrameTime += frameTime;
    ++frameCount;
    if (totalFrameTime > fpsUpdateTime)
    {
        // Displays FPS rounded to nearest int, and frame time (more useful for developers) in milliseconds to 2 decimal places
        float avgFrameTime = totalFrameTime / frameCount;
        std::ostringstream frameTimeMs;
        frameTimeMs.precision(2);
        frameTimeMs << std::fixed << avgFrameTime * 1000;
        std::string windowTitle = "CO2409 Week 21: Matrix Hierarchies / Animation - Frame Time: " + frameTimeMs.str() +
                                  "ms, FPS: " + std::to_string(static_cast<int>(1 / avgFrameTime + 0.5f));
        SetWindowTextA(gHWnd, windowTitle.c_str());
        totalFrameTime = 0;
        frameCount = 0;
    }
}
