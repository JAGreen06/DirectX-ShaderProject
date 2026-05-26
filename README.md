# DirectX 11 Graphics Demo
This project was built as part of my second year university coursework for CO2409. It extends a base program provided by my tutor which included Direct3D device and swap chain setup, basic rendering pipeline, model loading from file and display. On top of this, I implemented shadow mapping (rendering a depth map from a light's perspective and sampling it in the pixel shader), a static cube map environment (pushing the skybox to the far plane in a vertex shader, wrapping the texture around a cube model), cube map reflections per model (sampling the skybox texture in a pixel shader, outputting a final reflected colour based on a model's reflection strength) and custom HLSL shaders which calculated world positions of the skybox and lights in the vertex shader and their final rendered colour in the pixel shader.

# Features
## Shadow Mapping
Shadow mapping involved creating a new texture to store the shadow map. Each pixel in this texture would be a single floating point value, instead of four individual R, G, B and A values. The scene is then rendered from the light's perspective and stored in the shadow map. Whether a pixel is in shadow is determined in three steps. First the location of the pixel projected onto the shadow map is determined. Then we compare the stored depth value with the actual distance between the light and the pixel. If the value in the map is less than the actual distance the pixel is shadowed. This determines if the pixel's final colour is affected by the light source.

## Static Cube Map Environment
The process of cube mapping involves the wrapping of a texture around an environment, giving an illusion that this skybox is infinitely far away. This was achieved through a custom skybox shader, which forced the texture to the far plane by setting the projected values of z and w to each other. To allow this to work, a new depth stencil was used (LESS_EQUAL), ensuring that the normalised depth values at z = 1 would not fail if the depth buffer was cleared to 1. The pixel shader used the world positions of each fragment to sample the correct face of the TextureCube. 

## Cube Map Reflections
Cube map reflections were handled in a single pixel shader alongside all the lighting calculations. This was achieved by calculating the direction of the camera to the pixel, then using the built in reflect function to find the direction of the reflection. The skybox texture was then sampled using the reflection direction to determine the reflected colour. A per-model constant reflectivity value was also implemented to allow for control over the reflectivity of each model individually. 

## Custom HLSL Shaders
Per-pixel lighting was used in the program for its more realistic lighting compared to per-vertex lighting. The pixel shader calculated the contribution of each light source per pixel, producing smoother and more accurate lighting. This is in contrast to per-vertex lighting which determines colour based on vertex positions, interpolated between them. A depth-only vertex shader was also used to transform vertices into the light's perspective for shadow mapping, outputting depth values rather than colour. Additionally, a basic transform vertex shader handled transforming models from model space to projection space.

# Built Using
- DirectX 11 - Primary graphics API used for rendering.
- DirectXTK - Microsoft's DirectX tool kit, used for texture loading.
- Assimp - Open source asset importing library, used for loading 3D models from file.

# Building the Project
- Clone the repo.
- Open the solution in VS 2022 (untested in other versions).
- Build and run - NuGet restores automatically.

# Future Work
- Code Reformat - global variables used extensively in the program, reformat to make a more object-orientated solution for better maintainability.
- Percentage Closer Filtering (PCF) - Softening the edges of shadows giving more realistic shadows. 
- Dynamic Cube Environment - Captures the surrounding environment from the exact position of a reflective object, more realistic than static cube maps which is captured once at a fixed position. 

# Known Issues
Two shader warnings appear on build (X3578, X3206) but don't affect functionality.

# References
- *Luna's Introduction to 3D Game Programming with DirectX 11*
