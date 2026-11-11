# Testing with the AirFrameXR lab (no headset needed)

AirFrameXR's lab (`harness/` in the [AirFrameXR](https://github.com/jfbobier/AirFrameXR) repo) runs a game against the real streamer host and a mock headset on one PC, and checks that frames come out the other side. These two scenarios run a libovr game in it.

1. **Lab patch:** `lab-custom-app.patch` adds `"exe": "custom"` to the lab's app launcher (any program, `command` run in `cwd`). Apply it to `harness/lib.py` of an AirFrameXR checkout (`git apply`).
2. **Scenarios:** copy `e2e_le2_native.json` and `e2e_le2_launcher.json` to `harness/scenarios/` and edit the paths in `app.command` (the game exe; for the launcher scenario also where `libovr_launch.exe` is, the `dist` folder `build.ps1` made, copied to the lab PC).
   * `e2e_le2_native`: the game started directly, with the bridge DLL put by hand in a folder and picked up through `LIBOVR_DLL_DIR`, and the signature patch from another source.
   * `e2e_le2_launcher`: the game started by `libovr_launch.exe` (what the release does).
3. **Run:** `python3 harness/run.py run e2e_le2_launcher` (the lab stops any running `alvr_host.exe` on the lab PC: close your streamer first).

Pass criteria are only that the host sees frames and the mock client streams (`host_frames_sent`, `client_streaming`); the xr_probe-specific checks (`app_exit_ok`, duplicate timestamps) report FAIL for a game and can be ignored. Read the lab's `ovrshim.log` and decode `client.hevc` to look at the picture.

Last result (2026-11-11, RTX 5090, Lone Echo II build 0x61578052): launcher scenario with the 1.0.0 release build: signature patch and `--texture-budget 2` applied, 3819 frames sent to the mock headset.
