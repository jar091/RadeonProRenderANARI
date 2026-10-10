cmake_minimum_required(VERSION 3.11)

## Language setup

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_INSTALL_RPATH "$ORIGIN")

## Establish project

project(anari_library_rpr LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
include(GNUInstallDirs)
include(FetchContent)

## ANARI ##

# the device is built on top of helium, which is part of the ANARI SDK
find_package(anari 0.17 REQUIRED)

## RPR ##

# RPR_SDK_ROOT points to a checkout of
# https://github.com/GPUOpen-LibrariesAndSDKs/RadeonProRenderSDK (including the
# 'hipbin' submodule with the precompiled Northstar kernels). If it is not set
# the SDK is downloaded into the build directory.
set(RPR_SDK_ROOT "" CACHE PATH "Root directory of the RadeonProRender SDK")
set(RPR_SDK_GIT_TAG "v3.1.7.1" CACHE STRING
    "RadeonProRender SDK version to download when RPR_SDK_ROOT is not set")

if (NOT RPR_SDK_ROOT)
    FetchContent_Declare(rpr_sdk
            GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/RadeonProRenderSDK.git
            GIT_TAG ${RPR_SDK_GIT_TAG}
            GIT_SHALLOW ON
            GIT_SUBMODULES hipbin
            SOURCE_DIR ${CMAKE_BINARY_DIR}/rpr
            )
    FetchContent_GetProperties(rpr_sdk)
    if (NOT rpr_sdk_POPULATED)
        FetchContent_Populate(rpr_sdk)
    endif ()
    set(RPR_SDK_ROOT "${CMAKE_BINARY_DIR}/rpr" CACHE PATH
            "Root directory of the RadeonProRender SDK" FORCE)
endif ()

set(RPR_DIR "${RPR_SDK_ROOT}/RadeonProRender")
set(RPR_KERNELS_DIR "${RPR_SDK_ROOT}/hipbin")

if (NOT EXISTS "${RPR_DIR}/inc/RadeonProRender.h")
    message(FATAL_ERROR "RadeonProRender SDK not found in '${RPR_SDK_ROOT}'")
endif ()

if (WIN32)
    set(RPR_LIB_DIR "${RPR_DIR}/binWin64")
    set(CMAKE_FIND_LIBRARY_SUFFIXES .dll ${CMAKE_FIND_LIBRARY_SUFFIXES})
else ()
    if (APPLE)
        set(RPR_LIB_DIR "${RPR_DIR}/binMacOS")
    else ()
        # there is also a Centos version
        set(RPR_LIB_DIR "${RPR_DIR}/binUbuntu20")
    endif ()
endif ()

# find RPR library and plugin renderer to install

find_library(RPR_LIB
        NAMES RadeonProRender64 libRadeonProRender64
        HINTS "${RPR_LIB_DIR}"
        NO_DEFAULT_PATH)
find_library(RPR_PLUGIN_NORTHSTAR
        NAMES Northstar64 libNorthstar64
        HINTS "${RPR_LIB_DIR}"
        NO_DEFAULT_PATH)

if (NOT RPR_LIB OR NOT RPR_PLUGIN_NORTHSTAR)
    message(FATAL_ERROR "RadeonProRender libraries not found in '${RPR_LIB_DIR}'")
endif ()

# Northstar needs its precompiled kernels at runtime (*.hipbin for AMD GPUs,
# *.cudabin for NVIDIA GPUs). They are a submodule of the SDK repository.
file(GLOB RPR_KERNELS "${RPR_KERNELS_DIR}/*.hipbin" "${RPR_KERNELS_DIR}/*.cudabin"
        "${RPR_KERNELS_DIR}/*.json")
if (NOT RPR_KERNELS)
    message(WARNING "No precompiled Northstar kernels found in '${RPR_KERNELS_DIR}' "
            "(run 'git submodule update --init hipbin' in the RPR SDK). "
            "GPU rendering will not work without them.")
endif ()
option(RPR_ANARI_INSTALL_KERNELS "Install the precompiled Northstar kernels next to the device" ON)

# On NVIDIA GPUs Northstar renders with CUDA. It loads the runtime library of the CUDA version it was
# built with (cudart64_13.dll for RPR SDK 3.1.7) and crashes if the library is missing. The library is
# part of the CUDA toolkit and of the Python wheel 'nvidia-cuda-runtime', it is not part of the RPR SDK.
if (WIN32)
    find_file(RPR_CUDA_RUNTIME_LIBRARY
            NAMES cudart64_13.dll
            HINTS ENV CUDA_PATH
            PATH_SUFFIXES bin bin/x64 bin/x86_64
            DOC "CUDA runtime library needed by Northstar on NVIDIA GPUs")
    if (NOT RPR_CUDA_RUNTIME_LIBRARY)
        message(WARNING "cudart64_13.dll not found, set RPR_CUDA_RUNTIME_LIBRARY to install it next to "
                "the device. Without it the device does not render with CUDA on NVIDIA GPUs.")
    endif ()
endif ()

# The node definitions of the MaterialX surface shaders are compiled into the device: the MaterialX
# loader of RPR needs them for every document and the device must not depend on files at runtime.
set(RPR_MATERIALX_DEFINITIONS open_pbr_surface standard_surface usd_preview_surface)
set(RPR_MATERIALX_ARRAYS "")
set(RPR_MATERIALX_TABLE "")
foreach (RPR_MATERIALX_NAME ${RPR_MATERIALX_DEFINITIONS})
    set(RPR_MATERIALX_FILE "${CMAKE_CURRENT_SOURCE_DIR}/material/materialx/${RPR_MATERIALX_NAME}.mtlx")
    # the source file is generated again when a definition changes
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${RPR_MATERIALX_FILE}")
    file(READ "${RPR_MATERIALX_FILE}" RPR_MATERIALX_HEX HEX)
    # 16 bytes per line
    string(REGEX REPLACE "(................................)" "\\1\n" RPR_MATERIALX_HEX "${RPR_MATERIALX_HEX}")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," RPR_MATERIALX_HEX "${RPR_MATERIALX_HEX}")
    string(APPEND RPR_MATERIALX_ARRAYS
            "const unsigned char ${RPR_MATERIALX_NAME}[] = {\n${RPR_MATERIALX_HEX}\n};\n\n")
    string(APPEND RPR_MATERIALX_TABLE
            "    {\"${RPR_MATERIALX_NAME}\", (const char *)${RPR_MATERIALX_NAME}, sizeof(${RPR_MATERIALX_NAME})},\n")
endforeach ()
configure_file(material/MaterialXDefinitions.cpp.in
        ${CMAKE_CURRENT_BINARY_DIR}/MaterialXDefinitions.cpp @ONLY)

add_library(${PROJECT_NAME} SHARED
        Object.cpp
        RPRDevice.cpp
        RPRGlobalState.cpp
        RPRLibrary.cpp

        camera/Camera.cpp
        camera/Perspective.cpp
        camera/Orthographic.cpp

        frame/Frame.cpp

        material/Material.cpp
        material/MaterialGraph.cpp
        material/MaterialXDocument.cpp
        material/MaterialXMaterial.cpp
        material/MatteMaterial.cpp
        material/PhysicallyBasedMaterial.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/MaterialXDefinitions.cpp

        sampler/Sampler.cpp
        sampler/TransformSampler.cpp
        sampler/util/TransformNode.cpp
        sampler/image/Image.cpp
        sampler/image/Image1D.cpp
        sampler/image/Image2D.cpp
        sampler/image/Image3D.cpp

        renderer/Renderer.cpp
        renderer/PathTracerRenderer.cpp

        scene/Instance.cpp
        scene/Group.cpp
        scene/World.cpp

        scene/geometry/Surface.cpp
        scene/geometry/Geometry.cpp
        scene/geometry/Isosurface.cpp
        scene/geometry/MeshData.cpp
        scene/geometry/attributes/Attribute.cpp
        scene/geometry/mesh/Mesh.cpp
        scene/geometry/primitives/Primitives.cpp
        scene/geometry/primitives/Spheres.cpp
        scene/geometry/primitives/Cylinders.cpp
        scene/geometry/primitives/Cones.cpp
        scene/geometry/primitives/Curves.cpp

        scene/lights/Light.cpp
        scene/lights/analytic/AnalyticLight.cpp
        scene/lights/analytic/DirectionalLight.cpp
        scene/lights/analytic/PointLight.cpp
        scene/lights/analytic/SpotLight.cpp
        scene/lights/analytic/HdriLight.cpp
        scene/lights/analytic/RingLight.cpp
        scene/lights/area/QuadLight.cpp

        scene/volume/SpatialField.cpp
        scene/volume/StucturedRegularSpatialField.cpp
        scene/volume/Volume.cpp
        scene/volume/TransferFunction1DVolume.cpp)

# generates the object/parameter/extension query functions of the device
anari_generate_queries(
        DEVICE_TARGET ${PROJECT_NAME}
        CPP_NAMESPACE anari_rpr
        JSON_DEFINITIONS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/rpr_device.json
)

target_compile_definitions(${PROJECT_NAME} PRIVATE -Drpr_device_EXPORTS)
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${RPR_DIR}/inc)
if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _USE_MATH_DEFINES NOMINMAX _CRT_SECURE_NO_WARNINGS)
    if (MSVC)
        # The functions of the RPR API can throw C++ exceptions (for example if the compilation of the
        # kernels fails). With /EHsc the compiler assumes that functions with C linkage never throw and
        # does not catch them reliably.
        target_compile_options(${PROJECT_NAME} PRIVATE /EHs /EHc-)
    endif ()
    target_link_directories(${PROJECT_NAME} PRIVATE "${RPR_DIR}/libWin64")
    target_link_libraries(${PROJECT_NAME} PRIVATE anari::helium RadeonProRender64 delayimp)
    # RadeonProRender64.dll is loaded on first use from the directory of the device library
    # (see RPRGlobalState.cpp), not from the search path of the host application
    target_link_options(${PROJECT_NAME} PRIVATE "/DELAYLOAD:RadeonProRender64.dll")
else ()
    target_link_libraries(${PROJECT_NAME} PRIVATE anari::helium ${RPR_LIB})
endif ()

# Installation

if (WIN32)
    set(RPR_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
else ()
    set(RPR_RUNTIME_DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif ()

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )

# RPR runtime: core library, Northstar plugin and its precompiled kernels
install(FILES ${RPR_LIB} ${RPR_PLUGIN_NORTHSTAR}
        DESTINATION ${RPR_RUNTIME_DESTINATION})
if (RPR_ANARI_INSTALL_KERNELS AND RPR_KERNELS)
    install(FILES ${RPR_KERNELS}
            DESTINATION ${RPR_RUNTIME_DESTINATION}/hipbin)
endif ()
if (WIN32 AND RPR_CUDA_RUNTIME_LIBRARY)
    install(FILES ${RPR_CUDA_RUNTIME_LIBRARY}
            DESTINATION ${RPR_RUNTIME_DESTINATION})
endif ()

# Copy RPR runtime to the build directory to be able to run the device from there
if (WIN32)
    add_custom_command(TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RPR_LIB}" "${RPR_PLUGIN_NORTHSTAR}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMENT "Copy RPR SDK libraries")
else ()
    file(COPY ${RPR_LIB} ${RPR_PLUGIN_NORTHSTAR}
            DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif ()
