
/*SCENE RENDERING AND UPDATES*/

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
#include "MathHelpers.h"     // Helper functions for maths (provided for coursework)
#include "GraphicsHelpers.h" // Helper functions to unclutter the code here (provided for coursework)

#include "ColourRGBA.h" 

#include <sstream>
#include <memory>


/*SCENE DATA*/

// Constants controlling speed of movement/rotation
const float ROTATION_SPEED = 2.0f;  // 2 radians per second for rotation
const float MOVEMENT_SPEED = 50.0f; // 50 units per second for movement


// Meshes, models and cameras. Meshes prepared in InitGeometry function, Models & camera in InitScene

// Meshes
Mesh* gAnimatedMesh;
Mesh* gTeapotMesh;
Mesh* gGroundMesh;
Mesh* gLightMesh;
Mesh* gSkyboxMesh;
Mesh* gCrateMesh;


// Models
Model* gAnimated;
Model* gTeapot;
Model* gGround;
Model* gSkybox;

Camera* gCamera;

// Constant value for lights, stored in an array of lights
const int NUM_LIGHTS = 3;
Light gLights[NUM_LIGHTS];

// Constant value for number of crates, stored in an array of models
const int NUM_CRATES = 4;
Model* gCrates[NUM_CRATES];

// Additional light information
CVector3 gAmbientColour = { 0.2f, 0.2f, 0.3f }; // Background level of light
float    gSpecularPower = 256; // Specular power controls shininess of models, same for all models


ColourRGBA gBackgroundColor = { 0.2f, 0.2f, 0.3f, 1.0f }; // Background colour of the screen

// Variables controlling light1's orbiting of the robot
const float gLightOrbit = 20.0f;
const float gLightOrbitSpeed = 0.7f;
float gSpotlightConeAngle = 90.0f; // Spot light cone angle in degrees, essentially the spotlights FOV

// Lock FPS to monitor refresh rate, which will typically set it to 60fps. Press 'p' to toggle to full fps
bool lockFPS = true;

int gShadowMapSize = 16384; // Size of shadow map, higher number = higher resolution shadows


// Texture, depth stencil and shader resource view for shadow map
ID3D11Texture2D* gShadowMap1Texture = nullptr;
ID3D11DepthStencilView* gShadowMap1DepthStencil = nullptr; 
ID3D11ShaderResourceView* gShadowMap1SRV = nullptr; 


/*CONSTANT BUFFERS*/

// Variables that are sent over to the GPU each frame, structure is located in common.h and matches the structure of common.hlsli
// Anything used by the shaders needs to be sent over via a constant buffer


PerFrameConstants gPerFrameConstants;      // The constants that need to be sent to the GPU each frame
ID3D11Buffer*     gPerFrameConstantBuffer; // The GPU buffer that will recieve the constants above

PerModelConstants gPerModelConstants;      // As above, but constant that change per-model (e.g. world matrix)
ID3D11Buffer*     gPerModelConstantBuffer; 


/*TEXTURES*/

// DirectX objects controlling textures

// Robot Texture
ID3D11Resource*           gCharacterDiffuseSpecularMap    = nullptr; // Represents the memory used on the texture in the GPU
ID3D11ShaderResourceView* gCharacterDiffuseSpecularMapSRV = nullptr; // Gives the shader access to the texture used above

// Teapot Texture
ID3D11Resource*           gTeapotDiffuseSpecularMap    = nullptr;
ID3D11ShaderResourceView* gTeapotDiffuseSpecularMapSRV = nullptr;

// Ground Texture
ID3D11Resource*           gGroundDiffuseSpecularMap    = nullptr;
ID3D11ShaderResourceView* gGroundDiffuseSpecularMapSRV = nullptr;

// Light Texture (model only)
ID3D11Resource*           gLightDiffuseMap    = nullptr;
ID3D11ShaderResourceView* gLightDiffuseMapSRV = nullptr;

// Skybox Texture
ID3D11Resource*           gSkyboxDiffuseMap    = nullptr;
ID3D11ShaderResourceView* gSkyboxDiffuseMapSRV = nullptr;

// Container Texture
ID3D11Resource*           gContainerDiffuseMap    = nullptr;
ID3D11ShaderResourceView* gContainerDiffuseMapSRV = nullptr;


/*LIGHT HELPER FUNCTIONS*/

// Constructs the view matrix for a spotlight by inverting the world matrix. 
// The result transforms world-space positions into the lights local space, treating the light as a camera - used for shadow maps.
CMatrix4x4 CalculateLightViewMatrix(int lightIndex)
{
    return InverseAffine(gLights[lightIndex].GetModel()->WorldMatrix());
}

// Constructs the projection matrix for a spotlight's shadow pass.
// FOV is set to the spotlight cone so the frustum matches the light's cone.
CMatrix4x4 CalculateLightProjectionMatrix(int lightIndex)
{
    return MakeProjectionMatrix(1.0f, ToRadians(gSpotlightConeAngle)); // Helper function in Utility\GraphicsHelpers.cpp (provided for coursework)
}



/*INITIALISE SCENE GEOMETRY, CONSTANT BUFFERS AND STATES*/

// Prepare the geometry required for the scene
// Returns true on success
bool InitGeometry()
{
    // Load mesh geometry data, doesnt create anything in the scene.
    try 
    {
        gAnimatedMesh = new Mesh("robot.x");
        gTeapotMesh    = new Mesh("teapot.x");
        gGroundMesh   = new Mesh("Hills.x");
        gLightMesh    = new Mesh("Light.x");
        gSkyboxMesh = new Mesh("cube.x");
        gCrateMesh = new Mesh("CargoContainer.x");
    }
    catch (std::runtime_error e)
    {
        gLastError = e.what();
        return false;
    }


    // Load the shaders required for the geometry we will use
    if (!LoadShaders())
    {
        gLastError = "Error loading shaders";
        return false;
    }


    // Create GPU-side constant buffers to receive the gPerFrameConstants and gPerModelConstants structures above
    // These allow us to pass data from CPU to shaders
    gPerFrameConstantBuffer = CreateConstantBuffer(sizeof(gPerFrameConstants));
    gPerModelConstantBuffer = CreateConstantBuffer(sizeof(gPerModelConstants));

    if (gPerFrameConstantBuffer == nullptr || gPerModelConstantBuffer == nullptr) // Only continues if the constant buffers are succesfully created.
    {
        gLastError = "Error creating constant buffers";
        return false;
    }


    // Load and Prepare textures on the GPU

    // Load textures and create DirectX objects for them
    // Load texture resources to manage GPU memory for the texture and also shader resource viewers to allow textures to be used in shaders.
    //Function handles filling in these pointers with usable data.     
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

    /*CREATE SHADOW MAP TEXTURES*/

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = gShadowMapSize; // Size of the shadow map determines quality / resolution of shadows
    textureDesc.Height = gShadowMapSize; // Width == Height, aspect ratio of 1.0f as specified in CalulateLightProjectionMatrix().
    textureDesc.MipLevels = 1; // 1 level, means just the main texture, no additional mip-maps. not needed as sampled at full resolution.
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R32_TYPELESS; // The shadow map contains a single 32-bit value. typeless used as same texture needs to be interpreted in two different ways
    textureDesc.SampleDesc.Count = 1; // Disable MSAA, doesnt apply to depth only pass.
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D10_BIND_DEPTH_STENCIL | D3D10_BIND_SHADER_RESOURCE; // Indicate we will use texture as a depth buffer and also pass it to shaders
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;

    if (FAILED(gD3DDevice->CreateTexture2D(&textureDesc, NULL, &gShadowMap1Texture))) // Initialises texture as empty (NULL)
    {
        gLastError = "Error creating shadow map texture";
        return false;
    }

    //Create the depth stencil view, the texture just created is to be used as a depth buffer
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; //D32, one format of the TYPELESS definition above.
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0; //Mip Level 0, not needed as no additional textures.
    dsvDesc.Flags = 0;

    if (FAILED(gD3DDevice->CreateDepthStencilView(gShadowMap1Texture, &dsvDesc, &gShadowMap1DepthStencil)))
    {
        gLastError = "Error creating shadow map depth stencil view";
        return false;
    }

    //Shader resource view creation, to allow texture to be sent over to the shaders.
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT; // R32, The shaders see textures as colours, so shadow map pixels are not seen as depths but rather as "red" floats (one float taken from RGB). Although the shader code will use the value as a depth
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    if (FAILED(gD3DDevice->CreateShaderResourceView(gShadowMap1Texture, &srvDesc, &gShadowMap1SRV)))
    {
        gLastError = "Error creating shadow map shader resource view";
        return false;
    }


  	// Create all filtering modes, blending modes etc.
	if (!CreateStates())
	{
		gLastError = "Error creating states";
		return false;
	}

	return true;
}

/*SCENE PREPARATION (TRUE ON SUCCESS)*/

bool InitScene()
{
    // Scene Setup

    gAnimated = new Model(gAnimatedMesh);
    gGround = new Model(gGroundMesh);
    gTeapot = new Model(gTeapotMesh);
    gSkybox = new Model(gSkyboxMesh);


    for (int i = 0; i < NUM_CRATES; i++)
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

    gAnimated->SetPosition({ 20, 1.0f, 0 });
    gAnimated->SetScale(5);
    gAnimated->SetRotation({ 0, ToRadians(135.0f), 0 });

    gTeapot->SetPosition({ 50, 0, 70 });
    gTeapot->SetScale(2);
    gTeapot->SetRotation({ 0, ToRadians(180.0f), 0 });

    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        gLights[i].SetModel(new Model(gLightMesh));
    }

    gLights[0].SetColour({ 0.8f, 0.8f, 1.0f });
    gLights[0].SetStrength(10);
    gLights[0].GetModel()->SetPosition({ 30, 20, 0 });
    gLights[0].GetModel()->SetScale(pow(gLights[0].GetStrength(), 0.7f)); // ad-hoc equation to convert light strength to nice value.

    gLights[1].SetColour({ 1.0f, 0.8f, 0.2f });
    gLights[1].SetStrength(40);
    gLights[1].GetModel()->SetPosition({ -20, 50, 20 });
    gLights[1].GetModel()->SetScale(pow(gLights[1].GetStrength(), 0.7f));

    gLights[2].SetColour({ 0.8f, 0.8f, 1.0f });
    gLights[2].SetStrength(60);
    gLights[2].GetModel()->SetPosition({10.0f, 40, -70.0f});
    gLights[2].GetModel()->SetScale(pow(gLights[2].GetStrength(), 0.7f));

    // Camera Setup

    gCamera = new Camera();
    gCamera->SetPosition({ 15, 30,-70 });
    gCamera->SetRotation({ ToRadians(13), 0, 0 });

    return true;
}

/*RELEASE GEMOETRY AND SCENE RESOURCES FROM MEMORY*/

void ReleaseResources()
{
    ReleaseStates();

    if (gLightDiffuseMap)                  gLightDiffuseMapSRV->Release();
    if (gLightDiffuseMap)                  gLightDiffuseMap->Release();

    if (gContainerDiffuseMap)              gContainerDiffuseMap->Release();
    if (gContainerDiffuseMapSRV)           gContainerDiffuseMapSRV->Release();

    if (gGroundDiffuseSpecularMapSRV)      gGroundDiffuseSpecularMapSRV->Release();
    if (gGroundDiffuseSpecularMap)         gGroundDiffuseSpecularMap->Release();

    if (gTeapotDiffuseSpecularMapSRV)      gTeapotDiffuseSpecularMapSRV->Release();
    if (gTeapotDiffuseSpecularMap)         gTeapotDiffuseSpecularMap->Release();

    if (gCharacterDiffuseSpecularMapSRV)   gCharacterDiffuseSpecularMapSRV->Release();
    if (gCharacterDiffuseSpecularMap)      gCharacterDiffuseSpecularMap->Release();

    if (gShadowMap1SRV)                    gShadowMap1SRV->Release();
    if (gShadowMap1Texture)                gShadowMap1Texture->Release();
    if (gShadowMap1DepthStencil)           gShadowMap1DepthStencil->Release();

    if (gSkyboxDiffuseMap)                 gSkyboxDiffuseMap->Release();
    if (gSkyboxDiffuseMapSRV)              gSkyboxDiffuseMapSRV->Release();


    if (gPerModelConstantBuffer)           gPerModelConstantBuffer->Release();
    if (gPerFrameConstantBuffer)           gPerFrameConstantBuffer->Release();

    ReleaseShaders();

    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        delete gLights[i].GetModel();  gLights[i].SetModel(nullptr);
    }

    for (int i = 0; i < NUM_CRATES; i++)
    {
        delete gCrates[i]; gCrates[i] = nullptr;
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



/*SCENE RENDERING*/

void RenderDepthBufferFromLight(int lightIndex)
{
    // Pass results of view and projection matrix of the light over to the GPU via constant buffer.
    gPerFrameConstants.viewMatrix = CalculateLightViewMatrix(lightIndex);
    gPerFrameConstants.projectionMatrix = CalculateLightProjectionMatrix(lightIndex);
    gPerFrameConstants.viewProjectionMatrix = gPerFrameConstants.viewMatrix * gPerFrameConstants.projectionMatrix;
    UpdateConstantBuffer(gPerFrameConstantBuffer, gPerFrameConstants);

    // Indicate that the constant buffer we just updated is for use in the vertex and pixel shader,
    // first parameter must match constant buffer in shader.
    gD3DContext->VSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);
    gD3DContext->PSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);


    /*MODELS RENDERED THAT CAST SHADOWS*/

    // Use depth only rendering shaders.
    gD3DContext->VSSetShader(gBasicTransformVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gDepthOnlyPixelShader, nullptr, 0);

    //No blending, normal depth-buffer, Cull front.
    gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
    gD3DContext->RSSetState(gCullFrontState); // Reduces shadow acne (self shadowing).

    // Render models.
    gGround->Render();
    gAnimated->Render();
    for (int i = 0; i < NUM_CRATES; i++)
    {
        gCrates[i]->Render();
    }
}

// Render whole scene from the camera.
void RenderSceneFromCamera(Camera* camera)
{
    // Pass camera matrices over to GPU via constant buffer.
    gPerFrameConstants.viewMatrix           = camera->ViewMatrix();
    gPerFrameConstants.projectionMatrix     = camera->ProjectionMatrix();
    gPerFrameConstants.viewProjectionMatrix = camera->ViewProjectionMatrix();
    UpdateConstantBuffer(gPerFrameConstantBuffer, gPerFrameConstants);

    // Same as RenderDepthBufferFromLight().
    gD3DContext->VSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);
    gD3DContext->PSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);


    /*RENDER LIT MODELS*/

    // Select shaders for lighting.
    gD3DContext->VSSetShader(gPixelLightingVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gPixelLightingPixelShader,  nullptr, 0);
    
    // no blending, normal depth-buffer, back culling.
    gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
    gD3DContext->RSSetState(gCullBackState);

    
    
    gD3DContext->PSSetShaderResources(1, 1, &gSkyboxDiffuseMapSRV); // Sets texture slot 1 for all models to skybox, for reflections.

    // Select the approriate textures and sampler to use in the pixel shader
    gD3DContext->PSSetShaderResources(0, 1, &gGroundDiffuseSpecularMapSRV); // Texture slot 0, ground texture.
    gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler); //Sampler slot 0, anisotropic.

    gPerModelConstants.gReflectivity = 0.0f;
    gGround->Render();

    // Render other lit models, only change textures for each one
    gD3DContext->PSSetShaderResources(0, 1, &gCharacterDiffuseSpecularMapSRV); 
    gPerModelConstants.gReflectivity = 0.12f;
    gAnimated->Render();

    gD3DContext->PSSetShaderResources(0, 1, &gContainerDiffuseMapSRV);
    for (int i = 0; i < NUM_CRATES; i++)
    {
        gPerModelConstants.gReflectivity = 0.12f;
        gCrates[i]->Render();
    }


    /*RENDER LIGHTS*/

    // Select which shaders to use for light models
    gD3DContext->VSSetShader(gBasicTransformVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gLightModelPixelShader,      nullptr, 0);

    // Select the texture and sampler to use in the pixel shader
    gD3DContext->PSSetShaderResources(0, 1, &gLightDiffuseMapSRV);
    gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);

    // additive blending, read-only depth buffer, no culling. (additive provides transparency)
    gD3DContext->OMSetBlendState(gAdditiveBlendingState, nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
    gD3DContext->RSSetState(gCullNoneState);

    // Render all the lights in the array
    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        gPerModelConstants.objectColour = gLights[i].GetColour();
        gLights[i].GetModel()->Render();
    }
}




/*RENDER SCENE*/

void RenderScene()
{   
    // Set up the light information in the constant buffer
    // Don't send to the GPU yet, RenderSceneFromCamera does this.
    gPerFrameConstants.light1Colour   = gLights[0].GetColour() * gLights[0].GetStrength();
    gPerFrameConstants.light1Position = gLights[0].GetModel()->Position();

    gPerFrameConstants.light2Colour   = gLights[1].GetColour() * gLights[1].GetStrength();
    gPerFrameConstants.light2Position = gLights[1].GetModel()->Position();

    gPerFrameConstants.light3Colour = gLights[2].GetColour() * gLights[2].GetStrength();
    gPerFrameConstants.light3Position = gLights[2].GetModel()->Position();
    gPerFrameConstants.light3Facing = Normalise(gLights[2].GetModel()->WorldMatrix().GetZAxis()); // Forward vector of the lights transform.
    gPerFrameConstants.light3CosHalfAngle = cos(ToRadians(gSpotlightConeAngle / 2)); // used for single dot product comparison, compares cosines.
    gPerFrameConstants.light3ViewMatrix = CalculateLightViewMatrix(2);        
    gPerFrameConstants.light3ProjectionMatrix = CalculateLightProjectionMatrix(2);   


    gPerFrameConstants.ambientColour  = gAmbientColour;
    gPerFrameConstants.specularPower  = gSpecularPower;
    gPerFrameConstants.cameraPosition = gCamera->Position();


 
    /*RENDER FROM LIGHTS POINT OF VIEW*/

    // Setup the viewport to the size of the shadow map texture
    D3D11_VIEWPORT vp;
    vp.Width = static_cast<FLOAT>(gShadowMapSize);
    vp.Height = static_cast<FLOAT>(gShadowMapSize);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    gD3DContext->RSSetViewports(1, &vp);

    // Select the shadow map texture as the current depth buffer. clear shadow map depth buffer to far plane.
    gD3DContext->OMSetRenderTargets(0, nullptr, gShadowMap1DepthStencil);
    gD3DContext->ClearDepthStencilView(gShadowMap1DepthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Render the scene from the point of view of light (only depth values written)
    RenderDepthBufferFromLight(2); // Renders depth pass for spotlight, using light index 2.

    /*MAIN SCENE RENDERING*/

    // Set the back buffer as the target for rendering and select the main depth buffer.
    // When finished the back buffer is sent to the front buffer.
    gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);

    // Clear the back buffer to a fixed colour and the depth buffer to the far plane.
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

    //Send appropriate shadow map texture and sampler to the pixel shader.
    gD3DContext->PSSetShaderResources(2, 1, &gShadowMap1SRV);
    gD3DContext->PSSetSamplers(1, 1, &gTrilinearSampler);


    RenderSceneFromCamera(gCamera);

    //Render Teapot.
    gD3DContext->VSSetShader(gBasicTransformVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gTeapotPixelShader, nullptr, 0);

    gD3DContext->PSSetShaderResources(0, 1, &gTeapotDiffuseSpecularMapSRV);
    gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);

    gD3DContext->OMSetBlendState(gAdditiveBlendingState, nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
    gD3DContext->RSSetState(gCullNoneState);

    gTeapot->Render();

    //Render Skybox.
    gD3DContext->VSSetShader(gSkyboxVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gSkyboxPixelShader, nullptr, 0);

    gD3DContext->PSSetShaderResources(0, 1, &gSkyboxDiffuseMapSRV);
    gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);

    gD3DContext->OMSetDepthStencilState(gLessEqualDepthState, 0);
    gD3DContext->RSSetState(gCullNoneState);

    gSkybox->Render();
  
    // Present image to the front buffer when rendering complete.
    // Set first parameter to 1 to lock to vsync.
    gSwapChain->Present(lockFPS ? 1 : 0, 0);
}


/*SCENE UPDATES*/

void UpdateScene(float frameTime)
{
    gPerFrameConstants.time += frameTime;

	// Control Robot (will update its world matrix)
    gAnimated->Control(0, frameTime, Key_I, Key_K, Key_J, Key_L, Key_U, Key_O, Key_Period, Key_Comma); 	

    // Orbit the light.
	static float rotate = 0.0f;
    static bool go = true;
	gLights[0].GetModel()->SetPosition(gAnimated->Position() + CVector3{cos(rotate) * gLightOrbit, 10, sin(rotate) * gLightOrbit});   

    if (go)  rotate -= gLightOrbitSpeed * frameTime;
    if (KeyHit(Key_1))  go = !go;

	// Control camera (will update its view matrix)
	gCamera->Control(frameTime, Key_Up, Key_Down, Key_Left, Key_Right, Key_W, Key_S, Key_A, Key_D );

   
    // Switches colours, turns light on and off.
    gLights[0].UpdateColour(frameTime);
    gLights[1].UpdateToggle(frameTime);


    // Changes spotlight colours.
    static bool go2 = false;
    if (go2) { gLights[2].UpdateColour(frameTime); }
    else { gLights[2].SetColour({ 0.8f, 0.8f, 1.0f }); }
    
    if (KeyHit(Key_2))  go2 = !go2;

    // Toggle FPS limiting
    if (KeyHit(Key_P))  lockFPS = !lockFPS;


    // Show frame time / FPS in the window title
    const float fpsUpdateTime = 0.5f; // How long between updates (in seconds)
    static float totalFrameTime = 0;
    static int frameCount = 0;
    totalFrameTime += frameTime;
    ++frameCount;
    if (totalFrameTime > fpsUpdateTime)
    {     
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
