# Collect all sources and headers recursively. CONFIGURE_DEPENDS makes the build
# re-check the globs so added/removed files are picked up without a manual reconfigure.
file(GLOB_RECURSE NEON_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_executable(neon ${NEON_SOURCES})
add_executable(neon::neon ALIAS neon)

# Mirror the directory layout in the IDE solution explorer
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${NEON_SOURCES})

target_include_directories(neon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src/imgui
)

target_precompile_headers(neon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/pch.h
)

target_compile_definitions(neon PRIVATE
    _CONSOLE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

target_link_libraries(neon PRIVATE
    imgui
    fmt::fmt
    spdlog::spdlog
    ryml::ryml
    RmlUi::RmlUi
    EASTL
    lodepng
    magic_enum::magic_enum
    Microsoft::DirectXShaderCompiler
    Microsoft::DirectXTK12
    Microsoft::DirectXTex
    GPUOpen::D3D12MemoryAllocator
    SDL3::SDL3
    OpenAL::OpenAL

    d3d12
    dxgi
    dxguid
    uuid
    kernel32
    user32
    comdlg32
    advapi32
    shell32
    ole32
    oleaut32
    runtimeobject
    winmm
    dwmapi
)

if(MSVC)
    target_compile_options(neon PRIVATE
        /W4
        /WX
        /sdl
        /permissive-
        /MP
        /fp:fast
        /utf-8
        /Zc:__cplusplus
        /we4715
        /w34800
        /wd4324
        /external:anglebrackets
        /external:W1
        /external:templates-
    )
endif()

# Note: generator expressions are not expanded here by VS "Open Folder" CMake mode
# (the CMake file API reports them verbatim), so use a plain path. The source dir
# contains the shaders/ and ui/ assets loaded relative to the working directory.
set_target_properties(neon PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_custom_command(TARGET neon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:neon>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/ui"
        "$<TARGET_FILE_DIR:neon>/ui"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:neon>
        "$<TARGET_FILE_DIR:neon>"
    COMMAND_EXPAND_LISTS
    VERBATIM
)
