# DLSS build with PRs #69, #81 and #71

The current source build is based on Windows v1.9.1 and integrates, in order:

- #69: paced DLSS/FSR frame generation, Reflex, reconstruction and custom-output fixes.
- #81: optional neural rendering with shared settings and up to four model passes.
- #71: Windows HDR-display black-level correction and a preferred 10-bit swapchain.

The complete renderer and launcher must be rebuilt together. The local DLSS
bridge uses ABI **3**, distinguishing PR #69's table from the incompatible ABI 2
in the stock v1.9.1 release. Do not copy this bridge into an official package.
The separate neural-rendering adapter uses its own ABI 1.

The new source package is `dist/bbport-windows.zip`, with its playable folder at
`dist/bbport-windows/`. The older `Bloodborne-Windows-DLSS-4.5.zip` and
`Bloodborne-Windows-1.9.1-DLSS-4.5.zip` remain earlier builds without this PR stack.

Start `Bloodborne.exe` and choose the game folder. Select DLSS under Graphics.
Native AA, Quality, Balanced and Performance request DLSS 4.5 preset M; Ultra
Performance requests L. NVIDIA driver or App overrides may affect model selection.
In Advanced, Extra variables accepts `BB_DLSS_PRESET=default`, `K`, `L`, or `M`.
The default option delegates selection to NVIDIA; K can reduce model cost on
older RTX GPUs. These SR presets are separate from the NR model presets.

Frame generation and Reflex controls now follow PR #69: backend, factor and
Reflex changes take effect after restart. Frame generation and NR default to
Off. See [FG_REFLEX.md](FG_REFLEX.md) for settings and diagnostics.

Neural rendering requires a separately supplied compatible `nvngx_dlssnr.dll`.
It is **not included** in the package. Put it beside `bin/bb-probe.exe`, or set
`BB_NR_RUNTIME` to its directory. Missing components leave NR unavailable and
preserve the original scene. See [NEURAL_RENDERING.md](NEURAL_RENDERING.md).
This is the community feature-18 integration from PR #81, not an official public
DLSS 5 SDK integration.

PR #71 follows the window's Windows HDR-display state. `BB_HDR_GAMMA=0` disables
its gamma correction and preferred 10-bit SDR swapchain.

## Rebuild

Use MSYS2 CLANG64 and the dependencies in `packaging/windows/README.md`:

```bash
git submodule update --init --recursive
bash build.sh
```

`MSYS2_ROOT` can point to a relocated toolchain. This workspace's toolchain is in
`out/msys2/clang64`; use `MSYS2_ROOT="$PWD/out/msys2"` in an MSYS2 shell.
The FSR vendor patches stay as working-tree changes in the submodule and are
reapplied by `build.sh` when needed.

Build the DLSS adapter with Visual Studio C++ and a current NVIDIA DLSS SDK:

```powershell
./packaging/windows/build_dlss.ps1 -SdkRoot C:/path/to/DLSS-310.9.16 -SmokeTest
```

The NR adapter builds independently with CMake from `gpu/nr_bridge`; set
`DLSS_SDK_ROOT` and `VULKAN_INCLUDE`, build the Release configuration, and copy
`bbport_nr_nvngx.dll` into `out/`. `BBPORT_NR_BUILD_SMOKE=ON` builds the optional
standalone model test, which needs the separately supplied NR runtime.

Finally package the rebuilt renderer and launcher:

```powershell
./packaging/windows/package_dlss.ps1 -WindowsPython C:/path/to/python.exe
```

The former `-BaseArchive` overlay mode is rejected because it would pair an
incompatible renderer and adapter. No game data is included. Local integration
and validation details are in [PR_STACK_INTEGRATION.md](PR_STACK_INTEGRATION.md).
