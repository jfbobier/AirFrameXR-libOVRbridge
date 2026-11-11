# AirFrameXR libOVR bridge

Run old **libovr** (Oculus Rift SDK) PC VR games on a headset streamed by [AirFrameXR](https://github.com/jfbobier/AirFrameXR), for example an Apple Vision Pro, with no Oculus software, no SteamVR and no OpenXR translation layer in between. The game talks libovr straight to a `LibOVRRT64_1.dll` that hands its frames to the AirFrameXR streamer.

Windows + NVIDIA only (like AirFrameXR). MIT licensed.

## Status, honestly

| | |
|---|---|
| Played and checked | **Lone Echo II** (final Oculus build 1.1.561021.2, a D3D12 game) on an Apple Vision Pro, RTX 5090, AirFrameXR 1.3.9: it renders, tracks, and the owner felt the controller haptics. Steady at 100 fps once the link was fixed (the wireless link, not this project, was what stuttered; see AirFrameXR's docs). |
| Not tested | other libovr games, D3D11 libovr games (the D3D11 path is OVRNull's, unchanged), other GPUs, Quest headsets, controller input beyond a short play, the texture-budget patch in a long session. |
| Known limits | **Lone Echo II's desktop window flickers** (flat blue/black colours) while it runs. It does so with the shim's mirror texture drawn, kept black, and refused altogether, so it is the game's own window and not this project's output; the headset picture is not affected. D3D12 mirror textures are accepted but not drawn. No boundary / guardian. Only Lone Echo II build `0x61578052` is in the launcher's patch table; other games need their own entry (below). |

## What you need

1. [AirFrameXR](https://github.com/jfbobier/AirFrameXR) **1.3.9 or newer**, set up and connected to your headset (this project speaks its IPC version 36).
2. A libovr game on this PC that you can run.
3. This release: `libovr_launch.exe` and `bin\LibOVRRT64_1.dll`, kept together in one folder anywhere.

## Use

1. Start the AirFrameXR streamer and connect the headset.
2. Start the game through the launcher instead of its own exe:

```
libovr_launch.exe "D:\Games\Lone Echo 2\bin\win10\loneecho2.exe"
libovr_launch.exe --texture-budget 2 "D:\Games\Lone Echo 2\bin\win10\loneecho2.exe"
```

Nothing is installed in the game folder and no system setting changes. Remove the game's folder-side leftovers from other mods (`winmm.dll` from a mod loader, an `openxr` bridge folder) so that only one thing steers the libovr loader. The launcher writes `libovr_launch.log` next to itself and the bridge writes its log to `logs\AirFrameXR.log` next to `bin\`.

`--texture-budget 2|4` (Lone Echo II only, optional): the game sets its texture cache budget from the Texture quality setting and never looks at your VRAM (768 MiB, 1.5 GiB, 3 GiB), which makes textures drop to low detail and stay there. x2 suits GPUs with 12 GB or more, x4 GPUs with 16 GB or more; x4 also re-evaluates every object every frame and resumes mip upgrades after 1 s instead of 5 s. It replaces the discontinued community "VEGA patch" in effect, not in method: VEGA swapped NVIDIA driver profiles, this edits the game's own budget in memory.

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
