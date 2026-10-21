# Bloodborne for Windows
<a href="https://trendshift.io/repositories/286547?utm_source=trendshift-badge&amp;utm_medium=badge&amp;utm_campaign=badge-trendshift-286547" target="_blank" rel="noopener noreferrer"><img src="https://trendshift.io/api/badge/trendshift/repositories/286547/daily?language=C%2B%2B" alt="Supermedo%2Fbloodborne_pc | Trendshift" width="250" height="55"/></a>

**Bloodborne running natively on Windows 10 and 11, by [Supermedo](https://github.com/Supermedo) Mohammed Albarghouthi.**

The original PlayStation 4 game runs directly on your PC: its own x86-64 code runs natively,
and the graphics are translated to Vulkan. No emulator window, no setup scripts: unpack the zip,
start `Bloodborne.exe`, pick your game folder and press **PLAY**.

**[Download the latest version](https://github.com/Supermedo/bloodborne_pc/releases/latest)** · **[Join the Discord](https://discord.gg/yTMG8c4Bqm)**  **[Join X,com](https://x.com/SuperMedoPorts)**
Check my other ports 
https://nativeretro.com/

> **No game files are included.** You need your own decrypted dump of Bloodborne
> (CUSA03173). Version 1.09 is needed for the community patches (60/90/unlocked FPS,
> resolution, effects); other versions run at 30 FPS.
> This project is not affiliated with Sony Interactive Entertainment or FromSoftware.

## How this build differs from upstream

This branch is upstream's Windows release **1.9.4**
([Supermedo/bloodborne_pc](https://github.com/Supermedo/bloodborne_pc), `windows-v1.9.4`) with
the pull requests below merged on top, each from its author's latest push, plus this fork's own
DLSS and integration commits. Apart from this section and the build notes, this README is
upstream 1.9.4's.

### Added

| PR | Author | What it adds |
|---|---|---|
| [#69](https://github.com/Supermedo/bloodborne_pc/pull/69) | FIocker | Paced DLSS (2x–4x) and FSR 3.1 (2x) frame generation with NVIDIA Reflex. It **replaces** upstream's frame generation; see below. |
| [#81](https://github.com/Supermedo/bloodborne_pc/pull/81) | FIocker | Optional DLSS neural rendering: presets, pass chain, toggle hotkey (`nr_*` settings, launcher and in-game menu). |
| [#71](https://github.com/Supermedo/bloodborne_pc/pull/71) | fliperama86 | Gamma 2.2 for the SDR picture when Windows runs the display in HDR (`BB_HDR_GAMMA=0` turns it off). |
| [#78](https://github.com/Supermedo/bloodborne_pc/pull/78) | andrecabralgit | Awaken's GPU optimizations: occlusion queries, async pipeline compiles, sampler and texture-set memos, mip-hash reuse, faster shader preload, death-loading delay fix. |
| [#92](https://github.com/Supermedo/bloodborne_pc/pull/92) | bbport | One shader cache root for every install (`BB_CACHE_DIR`), a size cap (`BB_CACHE_MAX_MB`, default 2048) and statistics (`BB_CACHE_STATS=1`). |
| [#87](https://github.com/Supermedo/bloodborne_pc/pull/87) | DarkSpartan | Crash fixes: texture views clamped to their image, larger per-frame render heaps (eboot patch), GPU waits that stop on device loss, hashing only mapped guest memory. |
| [#80](https://github.com/Supermedo/bloodborne_pc/pull/80) | andrecabralgit | A deeper Windows audio queue (about four device periods) against crackling. |
| this fork | | DLSS 4.5 presets (L/M, `BB_DLSS_PRESET`) and a native DLSS bridge build; shader-cache IO hardening; a guard for the frame heap patch; tests and packaging. See [docs/DLSS_BUILD.md](docs/DLSS_BUILD.md). |

### Behaviour that differs from 1.9.4

- **Frame generation settings are new.** `frame_generation` (`off`/`2x`/`3x`/`4x`), `fg_backend`
  (`dlss`/`fsr`/`fsr401`) and `reflex` (`off`/`on`/`boost`) replace upstream's `frame_gen`,
  `frame_gen_multiplier` and `frame_gen_fit_display`, which are no longer read: coming from
  upstream, pick frame generation again. There is no "fit to display refresh"; #69's presenter
  paces the generated frames instead. A changed choice applies on the next start.
- **AMD ML-FG 4.0.1 is kept** as the third backend (`fsr401`: Radeon RX 9000, Windows 11, 2x,
  SDR). It runs serially on the render queue under #69's pacing, Reflex and HUD pass instead of
  upstream's detached completion queue (upstream measured the detached form about 6% faster on
  an RX 9070). A saved ML-FG choice the GPU cannot run uses FSR 3.1 for that session.
- **The DLSS bridge is ABI 3.** `bbport_dlss.dll` from upstream releases has a different frame
  generation table and is rejected; use the bridge this build packages.
- **#78's optimizations are on by default** and were measured only on the shadPS4 0.20.1 GPU
  layer, which upstream 1.9.3 replaced with 1.8.2's. If something looks wrong, set the switch
  to 0: `BB_OCCLUSION`, `BB_ASYNC_PIPELINES`, `BB_SAMPLER_MEMO`, `BB_KEEP_MIP_HASHES`,
  `BB_PARALLEL_IMAGE_HASH`, `BB_LOCAL_TEXTURE_SETS`, `BB_MEMO_REVALIDATE`, `BB_SKIP_UNCOMPILED`,
  `BB_LATE_PIPELINE_BIND`, `BB_FENCE_ORDER`, `BB_SUBMIT_END_TASKS`, `BB_GPU_THREAD_PRIORITY`,
  `BB_FAST_PRELOAD` (`BB_DMA_WAITS` stays opt-in).
- **GPU readbacks have no Off** (it broke the hunter's face); Off is treated as Relaxed.
- **Wider outputs keep a centred HUD** by default (`hud_position=0`) with #69's centred UI
  stage; 1280×800 and 1080×1920 are added to the output sizes.
- **The shader cache is pruned** to 2048 MiB by default (`BB_CACHE_MAX_MB=0`: no cap).
- **Larger Frame Heaps** is written whenever the eboot's two heap instructions are original,
  independent of the frame rate, resolution and effect groups.
- **FSR 3.1 frame generation** uses #69's `fsr-vulkan` patches 0003–0005; 0005 replaces 1.9.2's
  narrower allocation-retirement patch.
- The swapchain has one more image while frame generation is on, and an in-game restart re-reads
  the frame generation and output settings instead of keeping the launcher's start values.

### Not applied, because 1.9.4 already covers it

- **#75** (depth of field with live resolution) and **#90** (patch groups for modified eboots)
  are part of 1.9.4.
- **#84** (driver memory budget on discrete GPUs) and **#63** (pressure-aware eviction) are
  recorded without code changes: 1.9.4's texture cache keeps the unused buffer reserve out of
  the usage and a 60% pressure mark below 8 GB (`BB_GC_PRESSURE_PERCENT`); `BB_GC_BUDGET_MB`
  still forces a budget.
- **#78's per-size object-motion images**: 1.9.4's opt-in target cache
  (`BB_OBJECT_MOTION_CACHE`, the launcher's motion-vector cache) solves the same problem.
- **#80's audio-thread priority**: 1.9.4 already raises the FMOD threads (`BB_THREAD_PRIORITY=0`
  turns it off).

### Testing status

Built with MSYS2 CLANG64. The native tests pass on an RTX 5090, except the ML-FG stress test,
which needs an RX 9000 and fails the same way on upstream; `camera-motion-test`,
`scene-resolution-test` and `taa-shader-test` do not link on Windows, as in upstream 1.9.4. The
Python suite has the same environment-dependent failures as upstream 1.9.4 on the same machine.
Not yet tested in the game: frame generation and Reflex on 1.9.4, ML-FG under #69's pacing,
and #78's defaults on the 1.8.2 GPU layer.

## Features

- **Online play (beta):** messages, bloodstains and wandering ghosts from other hunters through
  [The Hunter's Dream](https://thehuntersdream.com), and bells and summons through a shadNet
  server (shadPS4's public one by default).
- **Launcher with every setting in one window**, in 13 languages: English, Arabic, Russian,
  Spanish, Portuguese, French, German, Italian, Polish, Turkish, Chinese, Japanese, Korean.
- **Guided first start:** five steps check your game files (a clean 1.09 `eboot.bin`, a complete
  folder) and your PC before the first start, set the picture for your hardware and your goal,
  set up keyboard or controller, bring your character along and sign you in for online play.
- **Simple or expert view:** the simple view shows the pages a player needs and sets everything
  technical by itself; the expert view adds every option. A switch at the bottom of the side bar.
- **Your goal instead of graphics jargon:** Smooth (up to 120 FPS), Balanced (60 FPS, the best
  picture) or Quiet (60 FPS, less work for the card); the settings for it come from your
  hardware, rated like the hardware check on [bbport.de](https://bbport.de/install/), each with
  its reason, and "Measure on this PC" checks them in the game.
- **Search** (Ctrl+F): settings and help by your own words ("stutters", "black screen",
  "controller").
- **Saves from other versions:** characters from shadPS4, Awaken's port, bbhost or a PS4 export
  are found and imported (your own saves are backed up first).
- **Help page:** checks your installation (a clean 1.09 `eboot.bin`, a complete game folder, the
  page file, the port's location) and shows the fix for each problem step by step; after a
  crash the launcher names the cause from the log. "Save a report" makes a zip for the Discord.
- **Game tests:** the launcher plays a fixed scene by itself and measures it, to find the best
  settings for 60, 90 or 120 FPS, to compare one setting with screenshots side by side, or to
  test a beta for crashes over hours; the report has every chart and the logs.
- **Updates from the launcher:** when a new version is out it tells you, and **Update**
  installs it. Your saves, settings and mods are kept.
- **NVIDIA DLSS** on GeForce RTX cards (RTX 20 series and newer).
- **AMD FSR 3.1 and FSR 4** upscaling, plus native-resolution TAA.
- **Windows AMD ML-FG 4.0.1** on supported RX 9000 GPUs, alongside FSR 3.1 frame
  generation. [Requirements, implementation and measurements](docs/ML_FRAME_GENERATION_WINDOWS.md).
- **Unlocked frame rate** with a frame cap (up to 120 by default), or 30/60/90 FPS.
- **Output resolutions** from 720p to 4K, presets from Native AA to Ultra Performance.
- **Cheats page:** never die, stealth, silent footsteps, Rally that never fades, enemy control,
  and gameplay tweaks (camera distance, no camera auto-rotation, easier running, ragdoll
  physics, the Japanese/Asian button layout with Circle to confirm and Cross to dodge).
- **Game effects** on and off: chromatic aberration, depth of field, motion blur, SSAO, the
  game's own AA, dynamic light shadows, screen-space reflections, model detail.
- **Field of view** from 30 to 90 degrees (the game has 43), for every camera of the game.
- **Mute in the background:** optionally silent while the game window is not in focus; the
  mouse is released then too.
- **Mods and third-party patches**, loaded without changing your game files.
- **DLC** from your own dump (The Old Hunters on base-game dumps), with a DLC folder setting.
- **Add to Steam** puts the game in your Steam library in one click.
- **`Play Bloodborne.exe`** starts the game straight away with your saved settings (good for a
  desktop shortcut).
- **Controller and keyboard**, an in-game settings menu (Insert or L3+R3), name entry on
  screen, a desktop shortcut, and a button to clear the shader cache.
- **Keyboard and mouse:** remap keys and buttons on the Controls page (up to three per action), key combinations such as
  Shift + left click, a Dark Souls III layout, mouse look, and a direct 1:1 mouse camera
  ([docs/INPUT.md](docs/INPUT.md)).
- **Text and voice language** chosen separately, Brazilian Portuguese text included.
- **Achievements page** in the launcher with the trophies you have earned, updated live.

## Requirements

- Windows 10 (1903 or later) or Windows 11, 64-bit
- An Intel Core 4th generation (Haswell, 2013) or newer, or an AMD Ryzen: the game uses
  instructions of the PS4's processor (AVX, BMI1, MOVBE, LZCNT). Intel 2nd and 3rd generation
  (Sandy and Ivy Bridge) run with translated instructions, best effort; processors without AVX
  (many Pentium and Celeron models) cannot run it
- A graphics card with Vulkan 1.3 and a current driver
- About 6 GB of free memory (RAM + page file), 10 GB for 1440p or 4K output
- DLSS: NVIDIA GeForce RTX 20 series or newer

Nothing else to install: everything the game needs is in the zip.

## How to play

1. Download the zip from [Releases](https://github.com/Supermedo/bloodborne_pc/releases/latest)
   and unpack it anywhere.
2. Start `Bloodborne.exe`. The first time, a short guide checks your game folder (the one
   with `eboot.bin`) and your PC, and sets the picture for your hardware.
3. Press **PLAY**.

**DLC.** The Game of the Year edition (CUSA03173) already has The Old Hunters. With a base-game
dump, also dump your add-ons: each is a folder named like `SPEXPANSIONDLC03` with only
`sce_sys` inside, since their data ships with the 1.09 update. Put them in one folder and choose
it as **DLC folder** on **Game & effects**, or copy them to `user\addcont\<title id>\` (the
layout shadPS4 and bbport use).

In the game, **Insert** (or **L3+R3** on a controller) opens the settings menu.
Keyboard: WASD move, arrows camera, Space Cross, Left Shift Circle, E Square, V Triangle,
1/3 L1/R1, R/F L2/R2, Z L3, Q/C R3 (lock-on), I/K/J/L d-pad, Enter Options, Tab / Backspace
left / right touchpad half. Xbox controllers: Back + d-pad left / right clicks the left / right
touchpad half (gestures / key items), Back alone the left half.
The keyboard now works together with a connected controller instead of only without one.
All of this is configurable in `input.ini` (shadPS4's input config syntax: a Bloodborne file
made for shadPS4 can be copied over it), including mouse look: see [docs/INPUT.md](docs/INPUT.md).

## Playing online (beta)

1. Register a shadNet account at [shadnet.shadps4.net](https://shadnet.shadps4.net).
2. In the launcher, open **Online co-op**, tick **Play online** and enter your account name
   (not your email) and password.
3. Press **Test connection**: all three lines should say "reachable". Then press **PLAY** and
   choose online at the title screen. The Hunter's Dream welcome message means you are online.

Everyone you want to play with needs game version 1.09 and the same co-op server. A private
shadNet server (from the [shadp2p](https://github.com/Wozzardman/shadp2p) project) works too:
put its address in **Server address**.

Playing with bbhost players on The Hunter's Dream's co-op pool is off in this port until it
works properly with their server. Please don't ask The Hunter's Dream for help with bbport: use
our [Discord server](https://discord.gg/yTMG8c4Bqm) instead.

## Known issues

- Some AMD graphics cards may still crash when the game world loads; please send
  `user\last_run.log` if yours does.
- Online play is new. Messages, bloodstains, ghosts and summons with the bells work on the
  default server; please report how it goes on Discord.
- Keep the frame cap at 120 or lower. The sprint that stayed at half speed above 90 FPS is
  fixed, but the game's own timing above 120 FPS has not been checked everywhere.
- Intel 2nd and 3rd generation processors run with translated instructions (best effort):
  a few rarely used ones are not translated yet.

## Problems and feedback

Ask on the [Discord server](https://discord.gg/yTMG8c4Bqm), or open an [issue](https://github.com/Supermedo/bloodborne_pc/issues) and attach
`user\last_run.log` from the game folder, with your graphics card and what happened.
If the game shows only a black screen, try **Advanced → Clear shader cache** first.
Crashes on AMD RDNA2 cards, slow FSR 4 and online errors are collected in
[docs/WINDOWS_TROUBLESHOOTING.md](docs/WINDOWS_TROUBLESHOOTING.md).

## Building from source

This build's pull requests on top of Windows 1.9.4 are listed in
[How this build differs from upstream](#how-this-build-differs-from-upstream). Their earlier
integration on 1.9.1 and its measurements are recorded in [integration notes](docs/PR_STACK_INTEGRATION.md),
[cache controls and validation](docs/CACHE_OPTIMIZATION.md) and
[stability integration notes](docs/STABILITY_PRS.md).

For the custom **DLSS 4.5** package, native PowerShell bridge build, and the current
DLSS 5 SDK limitation, see [docs/DLSS_BUILD.md](docs/DLSS_BUILD.md).
The verified SDK search and remaining DLSS 5 dependency are recorded in
[docs/DLSS5_SDK_STATUS.md](docs/DLSS5_SDK_STATUS.md).

See [packaging/windows/README.md](packaging/windows/README.md): MSYS2 CLANG64, `bash build.sh`,
then `bash packaging/windows/package.sh`. DLSS is built separately with
`packaging/windows/build_dlss.sh` (needs Visual Studio 2022).

## Credits

Built on [bbport](https://github.com/deadinside28/bloodborne_pc), the native Linux port of
Bloodborne by deadinside28, and on the [shadPS4](https://github.com/shadps4-emu/shadPS4)
renderer. The original README is in [docs/original-readme](docs/original-readme/README.md).

Online play uses the network, NP and shadNet code of [shadPS4](https://github.com/shadps4-emu/shadPS4)
0.20 and the shadNet server of the [shadPS4](https://shadps4.net) team, put together for this port
by **Awaken** ([BloodBorneButPc](https://github.com/Awakenchan/BloodBorneButPc)), whose online code
bbport now uses, including the backend for [The Hunter's Dream](https://thehuntersdream.com)
adapted from [bbhost](https://github.com/droogie/bbhost) by droogie (GPL-3.0-or-later; see
[gpu/bbnet/SHADNET-PORT.md](gpu/bbnet/SHADNET-PORT.md)). Thanks to Metr1k and The Hunter's Dream
team for their guidance, and to Wozzardman's [shadp2p](https://github.com/Wozzardman/shadp2p),
which bbport's first online version was built on.
Awaken's engine work (occlusion queries, mip-hash reuse, async pipeline compiles, draw-thread
waits) is ported behind `BB_*` switches; see [docs/parallel_gpu.md](docs/parallel_gpu.md).

The low-VRAM profile for 4 GB graphics cards comes from the 4 GB mod by **fixdag** and
**bashiku**.

Also used: [FSR-Vulkan](https://github.com/FireBurn/FSR-Vulkan) and the AMD FidelityFX SDK
(FSR), FSR 4 assets from [FireBurn/Q2RTX](https://github.com/FireBurn/Q2RTX), the DLSS bridge
adapted from [IFreemz/shadPS4-Bloodborne-DLSS-FSR](https://github.com/IFreemz/shadPS4-Bloodborne-DLSS-FSR),
the [NVIDIA DLSS SDK](https://github.com/NVIDIA/DLSS) (`nvngx_dlss.dll` under NVIDIA's license),
[LibAtrac9](https://github.com/Thealexbarney/LibAtrac9), [SDL3](https://github.com/libsdl-org/SDL),
[FFmpeg](https://ffmpeg.org), [Dear ImGui](https://github.com/ocornut/imgui),
[sirit](https://github.com/shadps4-emu/sirit), [magic_enum](https://github.com/Neargye/magic_enum),
[miniz](https://github.com/richgel999/miniz), [xbyak](https://github.com/herumi/xbyak),
[wepoll](https://github.com/piscisaureus/wepoll), [cpp-httplib](https://github.com/yhirose/cpp-httplib),
[Protocol Buffers](https://protobuf.dev), [OpenSSL](https://www.openssl.org),
[miniupnpc](https://miniupnp.tuxfamily.org), [JSON for Modern C++](https://github.com/nlohmann/json),
[Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator),
[MSYS2](https://www.msys2.org) / [LLVM](https://llvm.org), [PyInstaller](https://pyinstaller.org)
and [Pillow](https://python-pillow.org). Game patches by Kyo, Lance McDonald, auser1337,
illusion, emoose and the Bloodborne community.

NVIDIA, GeForce RTX and DLSS are trademarks of NVIDIA Corporation. The icon is original
artwork.

## License

GNU GPL v2 or later ([LICENSE](LICENSE)). Third-party components keep their own licenses
(see `licenses\` in the download).
