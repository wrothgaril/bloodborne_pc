# SPDX-FileCopyrightText: Copyright 2026 IFreemz (shadps4_dlss bridge), bbport contributors
# SPDX-License-Identifier: MIT
#
# bbport_dlss.dll links the NVIDIA DLSS SDK (MSVC only) and is built separately from the port:
#   cmake -S gpu/dlss_bridge -B out/dlss-bridge -G "Visual Studio 17 2022" -A x64 \
#         -DDLSS_SDK_ROOT=<github.com/NVIDIA/DLSS checkout> -DVULKAN_INCLUDE=<Vulkan-Headers include>
#   cmake --build out/dlss-bridge --config Release

cmake_minimum_required(VERSION 3.20)
project(bbport_dlss LANGUAGES CXX)

set(DLSS_SDK_ROOT "" CACHE PATH "NVIDIA DLSS SDK root (contains include/ and lib/)")
set(VULKAN_INCLUDE "" CACHE PATH "Vulkan-Headers include directory")

if (NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The DLSS bridge is Windows x64 only")
endif()
if (NOT EXISTS "${DLSS_SDK_ROOT}/include/nvsdk_ngx_vk.h")
    message(FATAL_ERROR "Set DLSS_SDK_ROOT to the NVIDIA DLSS SDK (github.com/NVIDIA/DLSS)")
endif()
if (NOT MSVC)
    message(FATAL_ERROR "The NVIDIA NGX library requires an MSVC-compatible compiler")
endif()
file(READ "${DLSS_SDK_ROOT}/include/nvsdk_ngx_defs.h" NGX_DEFINITIONS)
if (NOT NGX_DEFINITIONS MATCHES "NVSDK_NGX_DLSS_Hint_Render_Preset_M")
    message(FATAL_ERROR "DLSS 4.5 requires a current NVIDIA DLSS SDK with L/M render presets")
endif()
if (NOT EXISTS "${VULKAN_INCLUDE}/vulkan/vulkan.h")
    message(FATAL_ERROR "Set VULKAN_INCLUDE to the Vulkan SDK Include directory")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# nvsdk_ngx_s.lib is the static-CRT (/MT) variant: no Visual C++ runtime install needed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

add_library(bbport_dlss SHARED bbport_dlss_bridge.cpp bbport_dlss_bridge.h)
target_include_directories(bbport_dlss PRIVATE "${DLSS_SDK_ROOT}/include" "${VULKAN_INCLUDE}")
target_link_libraries(bbport_dlss PRIVATE "${DLSS_SDK_ROOT}/lib/Windows_x86_64/x64/nvsdk_ngx_s.lib")
target_compile_definitions(bbport_dlss PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(bbport_dlss PRIVATE /EHsc)

option(BB_DLSS_SMOKE_TEST "Build a Vulkan/NGX feature creation smoke test (needs an RTX GPU)" OFF)
if (BB_DLSS_SMOKE_TEST)
    find_package(Vulkan REQUIRED)
    add_executable(bbport_dlss_smoke tests/dlss_smoke.cpp)
    target_link_libraries(bbport_dlss_smoke PRIVATE Vulkan::Vulkan)
    target_compile_definitions(bbport_dlss_smoke PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(bbport_dlss_smoke PRIVATE /EHsc)
endif()
