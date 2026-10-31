# Love Live! School Idol Festival (PS4) : PC Port

Reverse-engineering project to build a PC port of the PS4 game **Love Live! School Idol Festival** (CUSA24620), so it runs natively on PC using the game's own PS4 assets and `eboot.bin`.

The approach is modeled on **pt-pc**, an existing project that runs the PS4 game "PT" on PC from a PS4 dump. We study how pt-pc works, then work out what it takes to do the same for this game.

## Status

Not started. No analysis has been done yet. Next step: read pt-pc and document its architecture.

## Folder layout

| Folder | Purpose |
|---|---|
| `analysis/` | Findings: how pt-pc works, what SIF contains, comparison between them |
| `plan/` | Roadmap, milestones, task list, decision log |
| `repository/` | Our port code and tools |

Naming rule for `analysis/` and `plan/`: each topic gets a folder named `<timestamp>-<topic>` (timestamp `YYYYMMDDHHmm`, e.g. `202610311300-analyze-game`), and each result inside is a markdown file named `<timestamp>-analysis.md` or `<timestamp>-plan.md`:

```
analysis/
└── 202610311300-analyze-game/
    ├── 202610311300-analysis.md
    └── 202610311422-analysis.md
plan/
└── 202610311300-build-the-port-version/
    └── 202610311300-plan.md
```

Related folders outside this project (read-only inputs):

| Path | Contents |
|---|---|
| `D:\Games\PS4\Love Live School Idol Festival` | PS4 game assets: PKG `Love.Live.School.Idol.Festival-CUSA24620.v1.02.pkg`, DLC folder, game dump with `eboot.bin` in `CUSA24620`, extracted DLC in `Extracted` |
| `D:\Games\PS4\pt-pc` | Reference repository: PT port to PC |
| `D:\Games\PS4\shadPS4` | shadPS4 emulator source (GPL-2.0): emulation reference and NID names |
| `D:\Games\PS4\pt-pc-game` | User's own P.T. copy (PKG and `CUSA01127` dump), the input pt-pc reads |

## Plan in brief

1. **Understand pt-pc:** executable loading, system library emulation, graphics, audio, input, file mapping, build; separate PT-specific from generic parts.
2. **Analyze SIF:** inventory the PKG, DLC, and dump; examine `eboot.bin` and modules; list imported system functions; identify engine and asset formats.
3. **Gap analysis:** per subsystem, mark as covered, needs adaptation, needs new work, or unknown; turn into milestones.
4. **Implement:** load the eboot, reach main, first frame, input, audio, then playable and polished.

## Rules

- The assets folder, pt-pc, the P.T. game folder, and shadPS4 are read-only; nothing in them is modified.
- No game assets, `eboot.bin`, or keys are stored in this repository. The port is meant to load from the user's own legally obtained dump.

## More

- `INSTRUCTIONS.md`: full project instructions
- `CLAUDE.md`: guidance for Claude sessions in this folder
