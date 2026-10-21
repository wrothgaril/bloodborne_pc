# DLSS 5 SDK investigation

Historical search of NVIDIA's official SDK channels. The current tree also
integrates PR #81's community neural-rendering adapter, after PR #69, as described
in [PR_STACK_INTEGRATION.md](PR_STACK_INTEGRATION.md). It still requires a separate
model DLL; this does not change the official SDK availability findings below.

Checked 17 October 2026. No complete, publicly downloadable NVIDIA DLSS 5 Neural
Rendering SDK was located in the channels below. At the time of this search no
DLSS 5 implementation was added. The earlier DLSS 4.5 package remains in `dist`.

## SDK identified in NVIDIA's sample

NVIDIA's Vulkan sample has an implementation of Neural Rendering, but explicitly
requires a beta NGX SDK with `include/nvsdk_ngx_helpers_dlssnr_vk.h`. Its CMake
configuration gives this example of a private SDK override:

`NVIDIA-NGX-DLSS-SDK-INTERNAL-v310.9.0-beta3`

This is a referenced package name, not a public download link or proof that it
is the only compatible version. Without the beta header, the sample disables
`USE_DLSSNR`.

Sources, pinned to the inspected sample revision:

- [SDK override and availability checks](https://github.com/nvpro-samples/vk_gltf_renderer/blob/4e02cfe5a759adb5d274e1ed88486aab5829e8df/CMakeLists.txt)
- [Neural Rendering input and evaluation documentation](https://github.com/nvpro-samples/vk_gltf_renderer/blob/4e02cfe5a759adb5d274e1ed88486aab5829e8df/docs/denoising.md)
- [Sample's NGX calls](https://github.com/nvpro-samples/vk_gltf_renderer/blob/4e02cfe5a759adb5d274e1ed88486aab5829e8df/src/dlss.cpp)

## Download channels checked

| Channel | Result |
| --- | --- |
| [NVIDIA DLSS v310.9.16](https://github.com/NVIDIA/DLSS/releases/tag/v310.9.16) | Latest release and repository tree inspected; no DLSS-NR header/runtime. The supplied local SDK lacks them too. |
| [NVIDIA Streamline v2.14.16](https://github.com/NVIDIA-RTX/Streamline/releases/tag/v2.14.16) | Latest release and repository tree inspected; the supplied x64 SDK has an NR feature ID, but no NR integration header, plugin, or runtime. |
| [NVIDIA Unreal plugin download page](https://developer.nvidia.com/rtx/dlss) | Inspected all 1,104 central-directory entries in the linked UE 5.8 DLSS 4.5 plugin v8.9.0, dated 16 October 2026. No DLSS-NR component was present. Only a 2 MiB ZIP range was downloaded. |
| [Vulkan sample releases](https://github.com/nvpro-samples/vk_gltf_renderer/releases) | Latest release v2026.10.1 has application binaries, not the beta NGX SDK. Source requires a separately supplied beta SDK. |
| [Streamline getting started](https://developer.nvidia.com/rtx/streamline/get-started) and [DLSS 5 research](https://research.nvidia.com/labs/adlr/DLSS5/) | No matching beta SDK download was located. |

The Unreal archive inspected was
`2026.10.16_UE5.8_DLSS4.5Plugin_v8.9.0.zip` (1,166,608,575 bytes).
The public Streamline changelog mentioning `sl.dlss_nr` does not supply the missing
plugin. The existence of a changelog entry or feature ID does not establish that
the public SDK includes an implementation.

Local research evidence is under `out/dlss5-research/`: the sample file tree,
downloaded CMake/source/documentation files, local SDK checks, and Unreal archive
file list. Those generated files are ignored by Git.

## What is needed to implement and validate it

NVIDIA needs to provide the compatible Vulkan DLSS-NR development headers,
matching NGX libraries, `nvngx_dlssnr.dll`, and the integration documentation.
The public sample documents a post-tonemap LDR neural stage with depth/motion
guides and a floating-point output image. This is separate from the existing
Super Resolution evaluation, so replacing `nvngx_dlss.dll` cannot enable it.

The official [NGX header](https://github.com/NVIDIA/DLSS/blob/9981c6350dc58c0ce21f024fd65e31e851c31689/include/nvsdk_ngx.h)
lists `ngxsupport@nvidia.com` as its contact. No request has been sent.
Suggested request text:

> Please provide the SDK access process for DLSS 5 Neural Rendering in a custom
> Windows x64 Vulkan renderer, using an RTX 5090. The public Vulkan sample requires
> nvsdk_ngx_helpers_dlssnr_vk.h and references an internal 310.9.0-beta3 package.
> We need the matching Vulkan headers, NGX library, neural rendering runtime and
> integration guide. The public DLSS 310.9.16 and Streamline 2.14.16 packages do
> not contain these components. Which package should we obtain?

