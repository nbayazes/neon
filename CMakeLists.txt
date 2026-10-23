cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    message(WARNING
        "CMAKE_TOOLCHAIN_FILE is not set. Configure with the vcpkg toolchain, e.g.:\n"
        "  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake\n"
        "or use one of the presets in CMakePresets.json.")
endif()

project(neon VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "neon currently only supports Windows (D3D12/SDL3).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Link-time code generation (/GL + /LTCG) for Release builds
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(ryml CONFIG REQUIRED)
find_package(RmlUi CONFIG REQUIRED)
find_package(EASTL CONFIG REQUIRED)
find_package(lodepng CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(directx-dxc CONFIG REQUIRED)
find_package(directxtk12 CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)
find_package(D3D12MemoryAllocator CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(OpenAL CONFIG REQUIRED)

add_subdirectory(src/imgui)
add_subdirectory(src/neon)
