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
| `src/app` | Entry point, first run and screens (boot flow, menus, song select, live, result, options) |
| `docs` | `BUILD.md`, `PLAY.md`; `formats/`: format specs (`arc`, `blbn`, `btx`, `bmar`, `scd`, `cri`, `shaders` …) |
| `tools` | Python RE and data tools (`py -3.14 -I tools/<name>.py ...`); `tools/data` holds small generated metadata |

## Build and play

- [docs/BUILD.md](docs/BUILD.md): tools, the two build commands, common errors.
- [docs/PLAY.md](docs/PLAY.md): what you need (your own CUSA24620 dump), first start, controls, settings, what is not built yet, and the command line.
- [docs/CHECKLIST.md](docs/CHECKLIST.md): the release checklist, a person's check of every screen.

## License

MIT for this project's code (`LICENSE`); third-party notices in `THIRD_PARTY.md`.
