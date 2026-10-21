# Crash, depth-of-field, audio and patch compatibility fixes

Integrated on top of the Windows 1.9.1 DLSS/NR and shader-cache build, in this
order, on 18 October 2026:

| PR | Pinned head | Result |
| --- | --- | --- |
| [#87](https://github.com/Supermedo/bloodborne_pc/pull/87) | `eeaa7e9c9ab27302b51208149eb2c3d03ba0afe4` | Clamp texture view ranges, guard hashes of unmapped memory, enlarge frame heaps and report GPU device loss |
| [#75](https://github.com/Supermedo/bloodborne_pc/pull/75) | `ad9423a27c679212c9ec0385a61be5f769adc996` | Auto live resolution uses startup patches above 1080p with depth of field enabled |
| [#80](https://github.com/Supermedo/bloodborne_pc/pull/80) | `0455f6980b6569837cd53faa6b08123eee4f90be` | Maintain a bounded audio cushion, drop overfull packets without clearing queued audio, raise Windows mixer priority |
| [#90](https://github.com/Supermedo/bloodborne_pc/pull/90) | `1a2aa5cc53cf14653c0b402cfbcb21462c894668` | Fingerprint FPS, resolution and effect patch groups independently; build wide HUD layers only when the resolution group is compatible |

The integration preserves the existing centered HUD/output-space viewports,
DLSS bridge ABI 3, NR bridge ABI 1, FG/Reflex submission tracking, HDR correction,
cache controls and GPU optimizations. The unmapped-memory guard also covers
#78's parallel/reused mip hashes. Device-fault feature negotiation retains the
existing dynamic-rendering and Reflex feature chains; fault report iteration is
bounded by allocated storage. Empty view ranges are clamped to a valid extent.

The Larger Frame Heaps fix checks both original MOV EDX instructions separately
before applying. It can therefore apply to an executable with modified FPS
code when those heap instructions remain compatible, and recognizes an already
enlarged heap. Unrecognized instructions skip the fix. External/extra patches
still require every fingerprinted group to match, unless explicitly forced.

## Behavior

- A GPU device loss now produces a fault report when the driver supports it and
  exits with code 23 rather than retrying a failed wait indefinitely. This
  diagnoses device loss; it does not prevent a driver reset.
- `live_resolution=auto` above 1080p with depth of field enabled uses startup
  resolution patches to avoid the known blur. Explicit `live_resolution=1`
  retains the live path. Changing these options requires a restart.
- The current #80 revision targets four device periods of audio, at least 80 ms
  and capped at 200 ms. This trades additional audio buffering for resilience to
  late delivery. It differs from the older 40 ms PR summary.
- Modified executables retain only patch groups whose original code matches.
  The launcher explains which options were disabled. A missing resolution
  group prevents wide HUD layers from being installed over a 16:9 game.

## Validation

Full CLANG64 renderer, capability tool and play helper compilation succeeds.
The native regressions cover production audio queue handling, cache IO, compile
job scheduling, DLSS ABI rejection, FG/NR settings, motion history, UI, Reflex
and aspect handling. Synthetic Python checks exercise isolated patch groups,
guarded frame heaps, partial-FPS patch generation, centered UI viewports and
the depth-of-field decision table without needing game data.

The audio test uses an SDL dummy device and the production mixer: it verifies
prefill limits, an overfull queue retaining its existing audio, bounded refill
after an underrun, and unchanged stereo PCM volume. It does not establish that
crackle is eliminated on a physical sound device.

Seven CTest cases and ten additional native suites pass. The focused Windows
Python suite runs 102 tests with six skipped because a prepared retail image is
unavailable. With only Windows system directories on PATH, the packaged renderer
passes Vulkan submission and 4096-byte GPU readback on the RTX 5090. The frozen
launcher creates its FG/NR controls with both features defaulting to Off.
BUILD-MANIFEST.json inside the package records its source revision and hashes.
Logs are under `out/pr-stability`. Retail game fingerprint
and route-dependent tests require a prepared game image and are skipped when
one is unavailable. The contributor's gameplay results are not local validation
of this combined build. Gameplay, long-session stability, actual device-loss
reporting, physical audio, HDR output and NR image output remain unverified.
