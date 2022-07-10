cmake_minimum_required (VERSION 3.6)

project(Tutorial07_GeometryShader CXX)

set(SOURCE
    src/Tutorial07_GeometryShader.cpp
    src/QxGeometryShader.cpp
    ../Common/src/TexturedCube.cpp
)

set(INCLUDE
    src/Tutorial07_GeometryShader.hpp
    src/QxGeometryShader.h
    ../Common/src/TexturedCube.hpp
)

set(SHADERS
    assets/cube.vsh
    assets/cube.psh
    assets/cube.gsh
    assets/structures.fxh
    assets/QxStructures.fxh
    assets/QxCube.gsh
    assets/QxCube.psh
    assets/QxCube.vsh
)

set(ASSETS
    assets/DGLogo.png
)

add_sample_app("Tutorial07_GeometryShader" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
target_link_libraries(Tutorial07_GeometryShader PRIVATE Diligent-TextureLoader)
