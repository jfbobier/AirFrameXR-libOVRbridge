# AirFrameXR libOVR bridge (experimental)

An **experimental** bridge that lets old **libovr** (Oculus Rift SDK) PC VR games talk to a headset streamed by [AirFrameXR](https://github.com/jfbobier/AirFrameXR), for example an Apple Vision Pro, with no Oculus software, no SteamVR and no OpenXR translation layer in between. The game talks libovr straight to a `LibOVRRT64_1.dll` that hands its frames to the AirFrameXR streamer.

Windows + NVIDIA only (like AirFrameXR). MIT licensed.

**Set your expectations low.** This is a hobby-grade experiment, not a supported feature of AirFrameXR. It has been tried on one game, one PC and one headset, and it breaks easily (see below). If a game does not start, shows no picture or stutters, that is the expected outcome more often than not. There is no support and no promise that a game will ever work.

## Status, honestly

| | |
|---|---|
| Tried | One libovr game (a D3D12 title, final Oculus build) on an Apple Vision Pro, RTX 5090: it rendered, tracked, and controller haptics were felt. That is the whole test history. |
| Not tested | any other game, D3D11 libovr games (that path is OVRNull's, unchanged), other GPUs, Quest headsets, controller input beyond a short play, long sessions. |
| Known limits | The game's own desktop window may flicker while it runs (independent of this project's mirror texture; the headset picture is not affected). D3D12 mirror textures are accepted but not drawn. No boundary / guardian. Haptics are a simple approximation. Each game needs its own entry in the launcher's patch table (below); only one build is in it today. |

### Why it breaks easily

* **It is tied to one AirFrameXR version.** The bridge carries a copy of AirFrameXR's IPC header and the streamer refuses a mismatch (the game's log then says `AirFrameXR host not reachable (not running, or a different version)` and no picture arrives). An AirFrameXR update that changes the IPC needs a matching rebuild of this bridge. It broke exactly like that when AirFrameXR moved to 1.3.10 until it was rebuilt.
* **It patches the game in memory** to get past the libovr loader's signature check, with offsets that are only valid for the exact exe builds in the launcher's table. Any other build is refused or not patched.
* **It relies on the game's libovr behaviour** as it was in the Oculus era; anything a game does that the shim does not implement fails quietly.

## What you need

1. [AirFrameXR](https://github.com/jfbobier/AirFrameXR) at the version this release was built for, set up and connected to your headset: **1.3.10 (IPC version 37)**. Check the version before blaming anything else.
2. A libovr game on this PC that you can run, whose exe build is in the launcher's table.
3. This release: `libovr_launch.exe` and `bin\LibOVRRT64_1.dll`, kept together in one folder anywhere.

## Use

1. Start the AirFrameXR streamer and connect the headset.
2. Start the game through the launcher instead of its own exe:

```
libovr_launch.exe "D:\Games\SomeGame\bin\win10\game.exe"
```

Nothing is installed in the game folder and no system setting changes. Remove the game's folder-side leftovers from other mods (`winmm.dll` from a mod loader, an `openxr` bridge folder) so that only one thing steers the libovr loader. The launcher writes `libovr_launch.log` next to itself and the bridge writes its log to `logs\AirFrameXR.log` next to `bin\`; those are the first places to look when it does not work.

`--texture-budget 2|4` (optional, only for the game build that has such an entry in the table): raises the game's in-memory texture cache budget, which the game otherwise sets from its quality setting without looking at your VRAM. x2 suits GPUs with 12 GB or more, x4 GPUs with 16 GB or more.

## How it works

* The game's libovr loader (statically linked in the exe) looks for `LibOVRRT64_1.dll` in the folder named by `LIBOVR_DLL_DIR` before anywhere else. The launcher sets it to its `bin\` folder.
* That loader refuses a DLL without Oculus's signature. The launcher starts the game suspended and edits the four bytes of that branch in memory before the game runs (offsets and bytes from [le2-mod-engine](https://github.com/kotorvr/le2-mod-engine), MIT). Nothing on disk is modified.
* `LibOVRRT64_1.dll` is the AirFrameXR shim: OVRNull from [VirtualDesktop-OpenXR](https://github.com/mbucchia/VirtualDesktop-OpenXR) (MIT) with the AirFrameXR host driver, which composes the game's layers and hands the picture to the streamer. What this repo adds to it for native games:
  * **D3D12 swapchains**: the shim makes its own D3D11 device on the game's adapter, creates the colour images as NT-handle shared textures that the game opens on its D3D12 device, and a shared fence carries the game queue's progress to the compositor (GPU-side wait). D3D12 depth images live on the game's own device (D3D11 cannot share depth, and the compositor never reads it).
  * **Haptics**: `ovr_SubmitControllerVibration` takes the strongest sample of each submitted buffer as the hand's amplitude; the streamer turns that into headset haptics.
  * D3D12 mirror textures are accepted but not drawn.

## Add another game

The launcher's table (`launcher\libovr_launch.cpp`, `Profiles()`) is keyed on the exe's PE timestamp. For a new game: find the libovr loader in its exe (the string `LIBOVR_DLL_DIR` leads to it), find the `call` to its signature check (`WinVerifyTrust`) and the `test eax,eax` / `jne` after it, and add the offset and the bytes (`85 c0 75 xx` becomes `90 90 eb xx`). Pull requests with entries for games you have verified are welcome.

## Build

```
powershell -ExecutionPolicy Bypass -File build.ps1
```

Needs Visual Studio 2022 (C++ workload) and git; the first run fetches VirtualDesktop-OpenXR at the pinned commit, its LibOVR headers (Oculus SDK license, not part of this repo) and WIL. Output in `dist\`. `-Debug` keeps the per-frame trace.

## Licenses

MIT; see `LICENSE` and `THIRD_PARTY.md`. This project does not touch store ownership or entitlement checks and contains no game files.
