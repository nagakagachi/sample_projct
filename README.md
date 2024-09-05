# sample_projct

DirectX12 Toy Renderer (WIP).

![d3d12_sample_img00](https://github.com/nagakagachi/sample_projct/assets/25050933/a756e23e-f47d-4291-ab49-fed4edb95f81)

This repository is currently implementing the RenderGraph mechanism ( Render Task Graph, rtg ). </br>
The following reference materials are available on RenderGraph. </br>
https://nagakagachi.notion.site/RenderGraph-54f0cf4284c7466697b99cc0df81be80


</br>

- Third Party
  - Assimp
    - https://github.com/assimp/assimp 
  - DirectXTex
    - https://github.com/microsoft/DirectXTex
  - tinyxml2
    - https://github.com/leethomason/tinyxml2
  - Dear ImGui
    - https://github.com/ocornut/imgui
 
- Build
  - clone
  - build third_party/assimp (cmake)
    - cd assimp
    - cmake CMakeLists.txt 
    - cmake --build . --config Release
  - build third_party/DirectXTex (cmake)
    - cd DirectXTex
    - cmake CMakeLists.txt 
    - cmake --build . --config Release
  - build graphics_test.sln (Visual Studio 2022)
    
- Runtime
  - camera operation
    - Fly-through camera operation with right mouse button down and WASD (UE5-like)
