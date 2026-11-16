cmake_minimum_required(VERSION 3.25)
project(title_fit LANGUAGES CXX)
# Scratch design harness for docs/design/windows-title-fit-v1. Not product code.
# It compiles the lane's own title-row sources so the strip, its style, its
# glyphs, the modality control and the Windows caption buttons are the
# application's, and adds only the option variants under study.
set(BPR_ROOT "" CACHE PATH "Exported BetterPullReview source tree")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
set(qt "${BPR_ROOT}/src/adapters/qt")
add_executable(title_fit
    title_fit.cpp
    stubs.cpp
    "${qt}/src/workbench_strip.cpp"
    "${qt}/src/painted_glyph_icon.cpp"
    "${qt}/src/keyboard_shortcuts.cpp"
    "${qt}/src/keyboard_focus_ring.cpp"
    "${BPR_ROOT}/src/core/src/appearance_contrast.cpp"
    "${BPR_ROOT}/src/core/src/appearance_model.cpp"
)
if(WIN32)
    target_sources(title_fit PRIVATE "${qt}/src/windows_caption_buttons.cpp")
    target_compile_options(title_fit PRIVATE /utf-8 /permissive- /EHsc)
endif()
target_include_directories(title_fit PRIVATE
    "${qt}/include" "${qt}/src"
    "${BPR_ROOT}/src/core/include"
    "${BPR_ROOT}/src/application/include")
target_link_libraries(title_fit PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
