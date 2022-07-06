cmake_minimum_required (VERSION 3.6)

project(ImguiDemo CXX)

set(SOURCE
    src/ImguiDemo.cpp
    src/QxImguiDemo.cpp
    ${DILIGENT_DEAR_IMGUI_PATH}/imgui_demo.cpp
)

set(INCLUDE
    src/ImguiDemo.hpp
    src/QxImguiDemo.h
)

set(SHADERS
)

set(ASSETS
)

add_sample_app("ImguiDemo" "DiligentSamples/Samples" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
