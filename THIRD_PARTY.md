# Third-party code and what this repo does not contain

| What | Where from | License | How it is used |
|---|---|---|---|
| OVRNull (the null libovr runtime) | [VirtualDesktop-OpenXR](https://github.com/mbucchia/VirtualDesktop-OpenXR) at `1a83fec8b5c565b14b06ffa8e1eb7e4768057573`, by Matthieu Bucchianeri | MIT | `shim\` is OVRNull's driver with the AirFrameXR host driver (IPC to the streamer, frame composition) and the changes listed in the README. `build.ps1` fetches the original at the pinned commit and overlays `shim\` on it. |
| Patch offsets and bytes for Lone Echo II build `0x61578052` | [le2-mod-engine](https://github.com/kotorvr/le2-mod-engine), kotorvr | MIT | `launcher\libovr_launch.cpp` carries the table. |
| LibOVR headers (`OVR_CAPI*.h`) | the `external/LibOVR` submodule of VirtualDesktop-OpenXR | Oculus SDK license | **Not in this repo.** `build.ps1` fetches them to compile; the built DLL contains no SDK source. |
| WIL | NuGet `Microsoft.Windows.ImplementationLibrary` | MIT | build-time only. |

This repo contains no game files and no code from any game. It does not touch store ownership or entitlement checks.
