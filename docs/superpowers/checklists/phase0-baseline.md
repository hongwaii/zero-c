# Phase 0 — Baseline Smoke Test

Run these after each rebuild of Phase 0. All must pass.

- [x] `./build.bat` builds without error
- [x] `./build.bat test` builds and runs `out/TEST.exe`; prints "test_strbuf: all pass"
- [x] `./build.bat shell` builds
- [x] `./build.bat help` lists `shell` in the command table
- [ ] Launching `out/APP-1.*.exe` opens a 1280×800 window [DEFERRED — requires Windows desktop]
- [ ] Window title is "Modem Agent" [DEFERRED — requires Windows desktop]
- [ ] Window body shows "Modem Agent v1 - <version>" (ASCII hyphen; CJK font in Task 10) [DEFERRED — requires Windows desktop]
- [ ] Closing the window (X) exits with rc=0 [DEFERRED — requires Windows desktop]
- [ ] Launching without env var: no console window appears (GUI subsystem) [DEFERRED — requires Windows desktop]
- [ ] Launching with `AGENT_DEBUG_CONSOLE=1`: console window appears with printf output [DEFERRED — requires Windows desktop]
- [ ] Launching with `--debug-console` flag: same as env var [DEFERRED — requires Windows desktop]
- [x] Re-running `./build.bat clean && ./build.bat` reproduces a working build from scratch

> Note: items 5–11 (windowed behavior) require a Windows desktop session to verify and are marked DEFERRED until the next desktop test. The build success confirms the EXE is valid; full visual verification happens in Phase 1. The debug console toggle is opt-in via the `AGENT_DEBUG_CONSOLE=1` env var or the `--debug-console` CLI flag, implemented in `app/shell/debug_console.{h,cpp}`.
