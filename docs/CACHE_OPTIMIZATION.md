# Shader cache and GPU optimization integration

Reviewed upstream PRs on 18 October 2026 against the local Windows 1.9.1 build
with PRs #69, #81 and #71.

| PR | Decision | Pinned revision |
| --- | --- | --- |
| [#78](https://github.com/Supermedo/bloodborne_pc/pull/78) | Integrate GPU optimizations and cache exit flushing | `2d9eb402fa58a3609367356a132c0836422f5d5e` |
| [#92](https://github.com/Supermedo/bloodborne_pc/pull/92) | Integrate shared cache root, pruning and statistics | `08a10c3f3a216cdde782cfb7355365e6d9f716fa` |
| [#84](https://github.com/Supermedo/bloodborne_pc/pull/84) | Apply driver budgets to discrete GPU texture collection | `1d335a89f94c62e6d8419ab32ac052b3b04e597d` |
| [#63](https://github.com/Supermedo/bloodborne_pc/pull/63) | Already equivalent in Windows 1.9.1; record ancestry | `3624432afaa2a35f672b9e3fb95929f784e8a1d9` |
| [#50](https://github.com/Supermedo/bloodborne_pc/pull/50) | Defer: broad alternative renderer/input/Remix stack overlaps this build | `a5e722b98270cf9d4ee7b24987d92fa2d11dae0c` |
| [#76](https://github.com/Supermedo/bloodborne_pc/pull/76) | Defer: separate FSR 4.1.1 provider integration and experimental motion cache | `aa144db2611dca69537fd63f311fe4427aa04b7b` |
| [#89](https://github.com/Supermedo/bloodborne_pc/pull/89) | Defer: separate small-heap APU allocation policy | `c6fa31854ae53220e0606586b825d49e8c3d18b7` |
| [#42](https://github.com/Supermedo/bloodborne_pc/pull/42) | Already represented by newer renderer micro-tile handling; no older-source merge | `1fcadc0a20676cf941bcada318a4201ace1c9edf` |

Implementation plan:

1. Preserve the validated build on a backup branch and integrate on
   `feature/cache-optimization`.
2. Retain Windows 1.9.1 allocator, shader cache format versions and sampler
   validation, plus the DLSS/NR/Reflex/HDR changes, while adapting #78 and #84.
3. Combine #92 with #78's synchronized IO queue. Test concurrent saves, drain on
   close, explicit flush, reopen/readback, pruning, hit/miss counters and malformed
   word blobs. Exercise pipeline job ownership and occlusion fallback arithmetic.
4. Rebuild, run focused regressions and packaged Vulkan startup checks, then
   preserve the preceding ZIP and produce a complete replacement package.

## Behavior and controls

The renderer already had a persistent shader cache. These changes improve write
completion at exit, startup preload and first-use graphics pipeline scheduling;
they do not eliminate every first-visit compilation stall. Motion vertex shaders
containing session-local addresses still have to be recompiled next launch.

The default cache stays at `user/cache/<serial>`. Set `BB_CACHE_DIR` to an absolute
root to reuse a cache between installations; each game retains its serial folder.
Use compatible builds and avoid running two instances against the same cache.
Changing GPU/profile or cache format can still rebuild incompatible entries.

`BB_CACHE_MAX_MB` defaults to 2048 MiB per serial directory. At startup, shader
blobs are removed oldest first if over that threshold, down to 90% of the limit.
It is a startup pruning threshold, not a live hard disk quota. `0` disables
pruning. The profile and unrelated files are retained; archived caches are not
pruned. Invalid limits use the default.

`BB_CACHE_STATS=1` prints blob read hits/misses and bytes read/written at cache
close or normal process exit. This measures cache IO, not avoided driver
compilation or FPS. `BB_CACHE_FLUSH_ON_EXIT=0` disables the normal exit flush;
otherwise it waits up to two seconds for queued writes. Crashes and forced
termination can still lose queued work.

#78 enables background graphics pipeline compilation, late binding, conservative
occlusion queries, texture/sampler memoization, image hash reuse and bounded
motion-target reuse. Its behavior can be compared using these restart-time
controls in the launcher's Advanced / Extra variables:

| Control | Effect |
| --- | --- |
| `BB_ASYNC_PIPELINES=0` | Compile pipelines synchronously |
| `BB_SKIP_UNCOMPILED=0` | Wait for scene geometry pipelines instead of briefly omitting draws |
| `BB_SKIP_UNCOMPILED_MS=1000` | Maximum omission time per compiling scene pipeline |
| `BB_LATE_PIPELINE_BIND=0` | Wait on the selection thread instead of recording thread |
| `BB_PIPELINE_THREADS=2` | Override compile workers (1–16; default one third of CPU threads, clamped 1–4) |
| `BB_FAST_PRELOAD=0` | Disable module reuse and session pipeline-cache preload optimization |
| `BB_OCCLUSION=0` | Restore always-visible query fallback if geometry disappears |
| `BB_LOCAL_TEXTURE_SETS=0 BB_SAMPLER_MEMO=0 BB_MEMO_REVALIDATE=0` | Disable individual binding memo changes |
| `BB_KEEP_MIP_HASHES=0 BB_PARALLEL_IMAGE_HASH=0` | Disable image hash changes |
| `BB_SUBMIT_END_TASKS=0 BB_FENCE_ORDER=0 BB_DRAWREC_LOOKAHEAD=0` | Disable individual submission overlap changes |
| `BB_GPU_THREAD_PRIORITY=0` | Restore normal render thread priority |
| `BB_RESPAWN_DELAY_FIX=0` | Retain the game's death-triggered 12-second minimum load delay |
| `BB_FRAME_STATS=1` | Print pipeline, frame and wait-site diagnostics |

The respawn patch only applies to a unique matching code sequence and retains
readiness checks and the multiplayer timeout. `BB_DMA_WAITS` remains opt-in.
The older "off by default" import notes in `parallel_gpu.md` are historical;
the current integrated #78 defaults are described here.

## Validation

The full CLANG64 renderer, capability tool and play helper build successfully.
Six CTest cases pass: compile-job/occlusion/respawn guards, production cache IO
with pruning enabled, unlimited and invalid-limit cases, plus both DLSS ABI
rejection cases. Ten additional native suites pass, covering motion history,
command buffers, UI composition, upscaler/output/FG/NR settings, FG timing,
Reflex and aspect handling. The focused Windows Python suite passes 92 tests
with one skipped because a prepared `out/eboot.elf` is not available.

Cache tests submit 400 writes from four concurrent producers, verify exact
readback after reopening, drain 100 additional writes on close, and verify
oldest-first pruning, retained unrelated files, invalidation, missing/truncated
word blobs and per-session statistics. Logs are in `out/pr-optimization`.

The complete package passes Vulkan command submission and 4096-byte GPU
readback on the RTX 5090 with a Windows-system-only PATH. The frozen launcher
reports version 1.9.1 and creates the neural-rendering pass controls, with FG
and NR still defaulting to Off. The preceding package is preserved as
`dist/Bloodborne-Windows-1.9.1-DLSS-PR69-81-71.zip`; the updated package is
`dist/bbport-windows.zip`.

Contributor FPS results are hardware-specific and are not measurements of this
integration. Gameplay, physical HDR output, NR model output and cold/warm cache
FPS comparisons still need a prepared game and a controlled route.
