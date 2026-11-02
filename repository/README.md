# repository

Code and tools for the PC port of "Love Live! School Idol Festival ~after school ACTIVITY~ Wai-Wai!Home Meeting!!" (PS4, CUSA24620). Plan: `../plan/202610311831-sif-port-roadmap/202610311831-plan.md`.

No game assets, `eboot.bin`, extracted files or keys are stored here. The port reads the game from the user's own dump at run time. Extracted or derived data goes to `../work/` (git-ignored).

## Layout

| Folder | Content |
|---|---|
| `src/engine` | Engine code: `core` (log, crash reports, thread pool), `platform` (OS layer, settings), `fs` (ARC/cmp `.pac` reader, Vfs over the dump), `formats` (`btx` textures), `render` (SDL3 GPU renderer), `audio` (CRI `@UTF`/ACB/AWB, HCA/ADX decoding, SDL3 output) |
| `src/tools` | Native tools (`sif_hca2wav`) |
| `shaders` | GLSL, compiled to SPIR-V by `glslc` at build time and embedded |
| `third_party` | Vendored sources: vgmstream clHCA (ISC), LZMA SDK decoder (public domain); see `THIRD_PARTY.md` |
| `tests/unit` | One executable per test, `Check()` style; `sif_vfs_test` / `sif_audio_test` take the dump folder as an optional argument |
| `src/game` | SIF game rules: `live` (chart, judgement, song loading) |
| `src/app` | Entry point and screens (`live_screen`: the M4 song slice and calibration) |
| `docs/formats` | Format specs: `arc`, `blbn`, `btx`, `bmar`, `scd`, `cri`, `shaders` |
| `tools` | Python RE and data tools (`py -3.14 -I tools/<name>.py ...`); `tools/data` holds small generated metadata |

## Build (Windows)

Needs Visual Studio 2026 with clang-cl, CMake ≥ 3.28 and Ninja. Run from a "x64 Native Tools" developer prompt so `clang-cl` is on PATH:

```
cmake --preset debug
cmake --build ../work/build/debug
```

The build folder is `../work/build/<preset>/`. SDL3 and zlib are fetched at configure time. The Vulkan SDK's `glslc` compiles the shaders (found through `VULKAN_SDK`).

## Run

```
sif.exe --game <your CUSA24620 dump folder> [--title|--menu|--select [--lang en|cn|tw|jp]] [--ui SCREEN:LAYOUT[:CLIP]] [--result-demo] [--live ID:VARIANT [--autoplay] [--stage stNNN] [--member N] | --calibrate | --show VFS_PATH --play SHEET:CUE|none] [--frames N] [--crash] [--data DIR]
```

- `--live 10:4` plays song 0010 on chart variant 04 (`ac_challenge`; 00-04 arcade, 10-13 home difficulties). Keys for the 9 lanes, left to right: `A S D F Space J K L ;`. Esc quits. The title bar shows combo, judgement counts and the score; `sif.log` gets the result and the mean press offset. `--autoplay` presses every note on time. The live screen draws the stage (`--stage`, default `st017`) and the members dancing the song (all who have a dance for it; `--member N` shows only member N, 1-9) behind the lanes, filmed by the song's own camera animation.
- Controllers (any SDL3 gamepad, e.g. a DualShock 4): in menus the D-pad moves, Cross confirms, Circle goes back, Start confirms. In a song the 9 lanes, left to right, are D-pad Left, Up, Right, Down, L1 or R1, Square, Triangle, Circle, Cross; Start quits. This lane layout is a placeholder, not the game's own.
- `--title` runs the boot flow from the game's own UI data (`ui2/01_title`): CRI and Square Enix logos, the notice, then the title screen. Any key skips a logo; any key on the title opens the top menu (`ui2/10_top_menu`; `--menu` starts there). Left/Right turn the wheel, Up/Down pick an entry, Enter opens the song select (`ui2/20_setup`; `--select` starts there): Left/Right pick a song, Enter, then Left/Right pick an arcade difficulty, Enter, then Left/Right pick a stage, Enter starts it; Esc goes back one step. `--lang` picks the UI language (default `en`).
- After a song played to its end from the boot flow, the result screen (`ui2/95_result`) shows the score, max combo and judgement counts; any key returns to the song select (Esc during a song returns at once; Esc in the song select goes back to the top menu). `--result-demo` (developer) shows it with fixed values.
- `--ui SCREEN:LAYOUT[:CLIP]` (developer) shows any ui2 layout, e.g. `--ui 10_top_menu:uil0001_TopMenu00`; `--ui-hide`/`--ui-hide-path` hide nodes or placed instances.
- `--calibrate` plays 24 clicks; press Space on each. The median offset is stored as `timing.audio_offset_ms` and applied to judgement.

- The game folder must be the user's own dump (checked: `sce_sys/param.sfo` names CUSA24620; `common/common.pac`, `sound/sound.pac`, `live/` exist). It is saved to `settings.ini`, so later runs need no `--game`. Song add-ons are read from the dump's `sound/dlc/`.
- Writes `sif.log` and `settings.ini` in `<exe>/data/` (or `--data DIR`).
- `--show` draws one texture, e.g. `chr/c11/c11.pac/model.pac/texture.texs/chika_body_01` (default: a member card picture).
- `--play` plays one cue, e.g. `sys:11_gatya` (default: song 0010 from `music`).
- `--crash` forces an access violation and writes `sif-crash-*.dmp` to the data folder (crash-reporting test).
- `--frames N` closes after N frames.

`settings.ini` keys: `[display] width, height, fullscreen, vsync`, `[audio] volume, output_device (not used yet), buffer_frames`, `[game] folder`, `[timing] audio_offset_ms, input_offset_ms` (for M4).

## License

MIT for this project's code (`LICENSE`); third-party notices in `THIRD_PARTY.md`.
