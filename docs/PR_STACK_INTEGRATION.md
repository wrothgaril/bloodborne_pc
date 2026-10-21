# PR stack integration on Windows v1.9.1

Integrated in dependency order on top of `windows-v1.9.1` (`b4dcc823`):

| PR | Pinned head | Changes |
| --- | --- | --- |
| [#69](https://github.com/Supermedo/bloodborne_pc/pull/69) | `948a87037604db96b6da9311b65d68a6171b5a56` | Pacing, Reflex, frame generation, reconstruction and custom outputs |
| [#81](https://github.com/Supermedo/bloodborne_pc/pull/81) | `735f9665311666d1ab207d46a139fcbefc0ebb0c` | Optional neural rendering and model/pass controls |
| [#71](https://github.com/Supermedo/bloodborne_pc/pull/71) | `8a1268034acc862aec696324cebf29f2485ceba4` | Windows HDR-display black levels and 10-bit swapchain preference |

The dependency's FG implementation replaces the competing upstream 1.9 FG
paths. Upstream's newer GPU memory/cache/index-buffer fixes remain in place.
Adaptations retain the newer component mapping and scheduler wait-array API,
use the PR settings schema in the swapchain, and carry Reflex identity through
both upstream flip-submission entry points. Local SR preset selection remains.

The SR/FG bridge ABI is locally bumped to **3**. Both upstream and PR #69 called
their different tables ABI 2; accepting the stock table would let the renderer
call the wrong function pointer. The separate NR bridge stays ABI 1. The full
renderer, adapters and launcher are rebuilt as a set rather than overlaid onto
an official binary release. The workspace-local compiler/libraries are under
`out/msys2`; the pinned dependencies are initialized as Git submodules.

Validation on this machine:

- Full CLANG64 renderer, capability tool and native play helper compile.
- Both MSVC adapters and the standalone NR smoke program compile.
- Eleven native CPU suites pass: command buffers, motion history, UI composition,
  upscaler support, output settings, FG timing/settings, Reflex, aspect thunk,
  neural settings and guarded rejection of incompatible DLSS tables.
- Focused Python suite: 92 tests, one skipped because `out/eboot.elf` requires a
  game preparation run. The Windows launcher widget/keyboard checks run with SDL.
- Both guarded DLSS-table CTest cases pass, including rejection of stock ABI 2.
- Six changed compute/fragment shaders compile and pass SPIR-V validation.
- The DLSS SR hardware smoke test passes all five quality modes and
  reinitialization on the RTX 5090; the adapter reports FG available.
- The frozen launcher reports version 1.9.1, defaults NR and FG to Off, and
  creates the neural-rendering controls and an additional model pass.
- The packaged renderer passes Vulkan command submission and a 4096-byte GPU
  readback on the RTX 5090. The packaged capability tool also starts successfully.
- The NR smoke program loads its ABI, queries the RTX 5090 and creates its Vulkan
  device. Without a model DLL it logs the missing-runtime reason and exits 3;
  this does not validate neural image output.

Build/test logs are under `out/pr-integration`. The contributor's original
measurements in `GPU_VALIDATION.md` and `NEURAL_RENDERING_VALIDATION.md` describe
other executables; they are not measurements of this integration.

No gameplay, physical HDR-display output, NR model output, or end-to-end frame
pacing/latency has been verified locally. NR is disabled by default and its
proprietary model is not shipped. The existing earlier ZIPs remain separate.
