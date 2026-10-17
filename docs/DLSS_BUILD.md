# DLSS 4.5 Windows build

This custom package uses the Windows v1.8.2 release's launcher and game renderer,
a rebuilt ABI 1 Vulkan DLSS bridge, and NVIDIA's signed release Super Resolution
runtime 310.9.16.0. The complete renderer was not rebuilt: the downloaded source
archive omits its dependency submodules and the CLANG64 dependencies are absent.
No game data is included.

Start `Bloodborne.exe`, select your own game folder, and press PLAY. The supplied
`bbport.ini` selects DLSS initially; the launcher still calls this option
**DLSS (NVIDIA GeForce RTX)**. Native AA, Quality, Balanced and Performance request
the DLSS 4.5 second-generation transformer preset M; Ultra Performance requests L.
NVIDIA's driver or NVIDIA App overrides may affect the model actually selected.

Preset M can cost more GPU time, especially on older RTX cards. For the runtime's
recommended presets, add `BB_DLSS_PRESET=default` to the launcher's **Advanced →
Extra variables**. `BB_DLSS_PRESET=K`, `L`, or `M` explicitly requests
that model instead. With default selection NVIDIA currently recommends K for
DLAA/Quality/Balanced, M for Performance, and L for Ultra Performance. Check
`user/last_run.log` for the requested numeric preset: K=11, L=12, M=13, default=0.

DLSS Frame Generation, Multi Frame Generation, Ray Reconstruction, and DLSS 5
Neural Rendering are not implemented in this bridge. The inspected Streamline
2.14.16 SDK defines `kFeatureDLSS_NR`, but provides no `sl_dlss_nr.h`,
`sl.dlss_nr.dll`, or `nvngx_dlssnr.dll`. A feature ID alone cannot implement DLSS 5;
its matching SDK, runtime and integration documentation are still needed.

## Rebuilding the bridge

Install Visual Studio C++ x64 build tools (2022 or 2026) and the Vulkan SDK.
Obtain the NVIDIA DLSS SDK from <https://github.com/NVIDIA/DLSS>, including its
`include`, `lib/Windows_x86_64/x64`, and `lib/Windows_x86_64/rel` folders and license.
This build used commit `9981c6350dc58c0ce21f024fd65e31e851c31689`.

From the repository root in PowerShell:

```powershell
./packaging/windows/build_dlss.ps1 -SdkRoot ./out/deps/DLSS -SmokeTest
./packaging/windows/package_dlss.ps1 -BaseArchive C:/path/to/Bloodborne-Windows.zip
```

The first command writes `out/bbport_dlss.dll` and `out/nvngx_dlss.dll`.
`-SmokeTest` requires a supported RTX GPU and NVIDIA driver. It verifies the ABI,
Vulkan extension discovery, NGX initialization, actual GPU feature creation in
all five quality modes, release, shutdown and reinitialization. It does not
evaluate image reconstruction or verify in-game visual quality or performance.
Omit that switch for compilation on a machine without an RTX GPU.

The second command creates a separate playable folder and ZIP in `dist`, with a
runtime version and SHA-256 manifest. It leaves the base ZIP and existing saves
alone. The base must be the official Windows v1.8.2 binary release archive.

