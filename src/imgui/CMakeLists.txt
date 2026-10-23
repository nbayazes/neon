add_library(imgui STATIC
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_impl_sdl3.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
)

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(imgui PUBLIC
    SDL3::SDL3
)

target_compile_definitions(imgui PRIVATE
    _LIB
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
    $<$<NOT:$<CONFIG:Debug>>:IMGUI_DISABLE_DEMO_WINDOWS>
)

if(MSVC)
    target_compile_options(imgui PRIVATE
        /W3
        /permissive-
        /utf-8
        /MP
    )
endif()
