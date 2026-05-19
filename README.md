# Surge Engine
Welcome to the development repository of SurgeEngine!
Surge is a mobile-first game engine, inspired by SUPERCELL's Titan engine. Also why the name is suffixed with _Expired, because we are looking for a new name!

 ## Platforms:

Currently supported platforms are Windows and Android, the Editor is Windows only as we only support Windows as out only development platform. The runtime ships on android and Windows. We plan to support iOS devices with a Robust Metal Renderer in future.

## Building:

Clone the repository by `git clone https://github.com/ridhwandll/Surge_Expired`. Then follow the platform specific instructions below.

### Windows
Inside the Scripts folder run `WindowsGenProjects.bat`. It will create a Visual Studio solution file in the `build` folder inside the root directory. You need to have a valid installation of VS along with the Clang compiler for VS installed. Build the solution, set `Editor/Player` as the startup project.

### Android
You need Android Studio installed with a valid SDK and NDK. After you have installed Android Studio, open up the `android` folder inside the root directory in Android Studio. Everything should be good to go. (If Android Studio prompts you to install proper version 
of NDK and SDK, you must install it, just accept the prompt)

### iOS
Due to my lack of Apple devices, it is completely unsupported. But I plan to add support for this in future.

## Rendering backend
The RHI currently supports `Vulkan 1.1` on Android and Windows. And We will keep `Vulkan 1.1` for Windows (No `DX12` ever). For Apple we will use `Metal` in future.

## Dependencies:

All dependencies can be found inside the `Engine\Vendor` from project root directory.

- cgltf - Mesh loading
- entt - ECS
- glm - Math Library
- ImGui - Tools UI
- json - Serialization
- Optick - (`Windows` only, profiler)
- shaderc - (`Windows` only, shader compiler to  SPIR-V)
- SPIRV-Cross - Shader Reflection and cross compilation
- stb - Texture file reader
- volk - Meta loader for Vulkan API
- Vulkan-Headers
- VulkanMemoryAllocator

All dependencies are included as a part of project with a custom CMakeLists.txt, no package manager/git submodule used. Required LICENSE can be found inside the respective Vendor folders.

# Features

- Verbose RHI
- Batch Renderer2D (100k quads in 10 draw calls)
- Renderer3D with robust Matarial workflow via shader reflection
- Automatic VertexAttribute, DescriptorSetLayout, PipelineLayout generation via shader reflection
- Mobile optimized Energy consrving Blinn-Phong with PBR parameters
- Currently working on RenderGraphs
- Scene system
- C++ Scripting (deprecated & disabled, needs rewrite)
- Automatic Serializarion via C++ Reflection (deprecated & disabled needs rewrite)

