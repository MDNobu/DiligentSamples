cmake_minimum_required (VERSION 3.6)

project(Tutorial02_Cube CXX)

set(SOURCE
    src/Tutorial02_Cube.cpp
    src/QxCube.cpp
)

set(INCLUDE
    src/Tutorial02_Cube.hpp
    src/QxCube.h
)

set(SHADERS
    assets/cube.vsh
    assets/cube.psh
    assets/QxCube.vsh
    assets/QxCube.psh
)

set(ASSETS)

add_sample_app("Tutorial02_Cube" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
