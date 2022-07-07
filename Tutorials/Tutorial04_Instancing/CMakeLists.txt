cmake_minimum_required (VERSION 3.6)

project(Tutorial04_Instancing CXX)

set(SOURCE
    src/Tutorial04_Instancing.cpp
    src/QxInstancing.cpp
    ../Common/src/TexturedCube.cpp
)

set(INCLUDE
    src/Tutorial04_Instancing.hpp
    src/QxInstancing.h
    ../Common/src/TexturedCube.hpp
)

set(SHADERS
    assets/cube_inst.vsh
    assets/cube_inst.psh
    assets/QxCubeInst.vsh
    assets/QxCubeInst.psh
)

set(ASSETS
       assets/DGLogo0.png
       assets/DGLogo1.png
       assets/DGLogo2.png
       assets/DGLogo3.png
)

add_sample_app("Tutorial04_Instancing" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
target_link_libraries(Tutorial04_Instancing PRIVATE Diligent-TextureLoader)