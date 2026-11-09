# repository

Code and tools for the PC port of "Love Live! School Idol Festival ~after school ACTIVITY~ Wai-Wai!Home Meeting!!" (PS4, CUSA24620). Plan: `../plan/202610311831-sif-port-roadmap/202610311831-plan.md`.

No game assets, `eboot.bin`, extracted files or keys are stored here. The port reads the game from the user's own dump at run time. Extracted or derived data goes to `../work/` (git-ignored).

## Layout

| Folder | Content |
|---|---|
| `src/engine` | Engine code: `core` (log, crash reports, thread pool), `platform` (OS layer, settings), `fs` (ARC/cmp `.pac` reader, Vfs over the dump), `formats` (`btx` textures), `render` (SDL3 GPU renderer), `audio` (CRI `@UTF`/ACB/AWB, HCA/ADX decoding, SDL3 output), `video` (USM movies, MPEG-1 via a patched pl_mpeg) |
| `src/tools` | Native tools (`sif_hca2wav`) |
| `shaders` | GLSL, compiled to SPIR-V by `glslc` at build time and embedded |
| `third_party` | Vendored sources: vgmstream clHCA (ISC), LZMA SDK decoder (public domain), stb_truetype, pl_mpeg (MIT, patched); see `THIRD_PARTY.md` |
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
sif.exe --game <your CUSA24620 dump folder> [--title|--menu|--select [--lang en|cn|tw|jp]] [--ui SCREEN:LAYOUT[:CLIP]] [--result-demo] [--live ID:VARIANT [--autoplay] [--stage stNNN] [--costume dNNN] [--member N] | --calibrate | --show VFS_PATH --play SHEET:CUE|none] [--frames N] [--crash] [--data DIR]
```

- `--live 10:4` plays song 0010 on chart variant 04 (`ac_challenge`; 00-04 arcade, 10-13 home difficulties). Keys for the 9 lanes, left to right: `A S D F Space J K L ;`. Esc quits. The title bar shows combo, judgement counts and the score; `sif.log` gets the result and the mean press offset. `--autoplay` presses every note on time. The live screen draws the stage (`--stage`, default `st017`) and the members dancing the song (all who have a dance for it; `--member N` shows only member N, 1-9) behind the lanes, filmed by the song's own camera animation.
- Controllers (any SDL3 gamepad, e.g. a DualShock 4): in menus the D-pad moves, Cross confirms, Circle goes back, Start confirms. In a song the 9 lanes, left to right, are by default the game's own: L2, L1, D-pad Left, Down, Right or Square (centre), Cross, Circle, R1, R2; Start quits. Options → Control Settings changes them (below).
- `--title` runs the boot flow from the game's own UI data (`ui2/01_title`): CRI and Square Enix logos, the notice, then the title screen. Any key skips a logo; any key on the title opens the top menu (`ui2/10_top_menu`; `--menu` starts there). Left/Right turn the wheel, Up/Down pick an entry, Enter opens the song select (`ui2/20_setup`; `--select` starts there): Left/Right pick a song, Enter, then Left/Right pick an arcade difficulty, Enter, then Left/Right pick a stage, Enter, then Left/Right pick a costume for everyone (it starts on the song's own, else the school uniform), Enter starts it; Esc goes back one step. `--lang` picks the UI language (default `en`).
- After a song played to its end from the boot flow, the result screen (`ui2/95_result`) shows the score, max combo and judgement counts; any key returns to the song select (Esc during a song returns at once; Esc in the song select goes back to the top menu). `--result-demo` (developer) shows it with fixed values.
- Results are kept in `save.txt` in the data folder (the port's own plain-text save: best score, combo, rank and clear rate per chart, play counts, PERFECT and GREAT totals, lives per costume). The song select shows each chart's best rank; the result screen marks new bests with NEW. A save the port cannot read is left untouched and the game plays without saving.
- Add-ons: the song select lists the base game's songs and costumes plus those of the add-ons you have installed (`[game] dlc_folder`); `sif.log` says how many it found.
- Missions: after each finished live, `sif.log` lists the missions it completed (plays, FULL COMBO, rank, score, PERFECT and GREAT totals; general, per song and per costume worn) and the accolade each gives. Top menu → Event → Task List shows them with progress: Left/Right switch the Member, Group, Songs and Costumes tabs, Up/Down move, PageUp/PageDown (pad L1/R1) a page, Enter (pad Cross) claims a completed mission's reward (GET! → COMPLETED; the tabs count the rewards waiting), Esc back.
- Nameplate: the top-menu header shows the player's name plate, accolade and badge. Top menu → Collection → Profile Card opens the game's Change Accolade or Name Plate screen: PageUp/PageDown (pad L1/R1) switch the Accolade, Name Plate and Badge tabs (the claimed rewards plus the starters), arrows move, Enter chooses (the preview and star follow), Esc goes back; the choice is kept in `save.txt`.
- Costumes: each finished live gives some of the song's materials (logged in `sif.log`). Top menu → Costumes → Costume Materials lists them (Left/Right: µ's / Aqours). Create/Change Costumes shows every costume with its state (Obtained, Ready to Create, Insufficient Materials, download only): arrows move, Enter shows the materials needed and owned, and Enter again makes the costume. PageUp/PageDown (pad L1/R1) switch the member being dressed (µ's 1-9, Aqours 11-19, Saint Snow 21-22; the grid shows that member's group); Enter on an owned costume puts it on that member, who then wears it in every live (over the song select's choice) when they have a model for it. The song select offers only the costumes you own (from the start, rewarded, made, or from an installed add-on).
- Talk: on the top menu the navigator (Honoka for now) greets by the time of day; T (pad Triangle) plays another of her lines.
- Options: on the top menu, turn the wheel to Options and pick Sound (volume) or Game Settings (note speed, timing offset). Up/Down pick a row, Left/Right change it (it applies at once), Enter or Esc goes back; the values are written to `settings.ini` (`[audio] volume`, `[play] approach_ms`, `[timing] audio_offset_ms`). Control Settings opens the 9-lane Button Controls: Left/Right pick a button slot, Up/Down change its pad button, T (pad Triangle) restores the defaults, Enter or Esc goes back; saved as `[controls] pad_lanes`.
- `--ui SCREEN:LAYOUT[:CLIP]` (developer) shows any ui2 layout, e.g. `--ui 10_top_menu:uil0001_TopMenu00`; `--ui-hide`/`--ui-hide-path` hide nodes or placed instances.
- `--calibrate` plays 24 clicks; press Space on each. The median offset is stored as `timing.audio_offset_ms` and applied to judgement.

- The game folder must be the user's own dump (checked: `sce_sys/param.sfo` names CUSA24620; `common/common.pac`, `sound/sound.pac`, `live/` exist). It is saved to `settings.ini`, so later runs need no `--game`. Song add-ons are read from the dump's `sound/dlc/`.
- Writes `sif.log` and `settings.ini` in `<exe>/data/` (or `--data DIR`).
- `--show` draws one texture, e.g. `chr/c11/c11.pac/model.pac/texture.texs/chika_body_01` (default: a member card picture).
- `--movie` plays one of the game's movies instead, looping, e.g. `--movie movie/monitor_001.usm --play none`; `sif.log` gets the average decode time.
- `--play` plays one cue, e.g. `sys:11_gatya` (default: song 0010 from `music`).
- `--crash` forces an access violation and writes `sif-crash-*.dmp` to the data folder (crash-reporting test).
- `--frames N` closes after N frames.

`settings.ini` keys: `[display] width, height, fullscreen, vsync`, `[audio] volume, output_device (not used yet), buffer_frames`, `[game] folder, dlc_folder` (your own add-ons: extracted add-on folders or `.pkg` files; empty = the `Extracted` and `DLC` folders next to the dump), `[timing] audio_offset_ms, input_offset_ms` (for M4).

## License

MIT for this project's code (`LICENSE`); third-party notices in `THIRD_PARTY.md`.
