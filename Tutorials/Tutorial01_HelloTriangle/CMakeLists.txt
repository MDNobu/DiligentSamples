cmake_minimum_required (VERSION 3.6)

project(Tutorial01_HelloTriangle CXX)

set(SOURCE
    src/Tutorial01_HelloTriangle.cpp
    src/QxHelloTriangle.cpp
)

set(INCLUDE
    src/Tutorial01_HelloTriangle.hpp
    src/QxHelloTriangle.h
)

set(SHADERS)
set(ASSETS)

add_sample_app("Tutorial01_HelloTriangle" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
