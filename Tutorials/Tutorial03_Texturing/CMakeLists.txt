cmake_minimum_required (VERSION 3.6)

project(Tutorial03_Texturing CXX)

set(SOURCE
    src/Tutorial03_Texturing.cpp
    src/QxTexturing.cpp
)

set(INCLUDE
    src/Tutorial03_Texturing.hpp
    src/QxTexturing.h
)

set(SHADERS
    assets/cube.vsh
    assets/cube.psh
    assets/QxCube.psh
    assets/QxCube.vsh
)

set(ASSETS
    assets/DGLogo.png
)

add_sample_app("Tutorial03_Texturing" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
target_link_libraries(Tutorial03_Texturing PRIVATE Diligent-TextureLoader)