# CLAUDE.md

Guidance for Claude when working in this folder. See `INSTRUCTIONS.md` for the full project instructions and `README.md` for the overview.

## Project

PC port of the PS4 game **Love Live! School Idol Festival (CUSA24620)**, modeled on **pt-pc** (a PS4-to-PC port of the game "PT"). Method: analyze pt-pc, analyze the SIF dump, find the gaps, then build.

## Folders

| Path | Role | Access |
|---|---|---|
| `E:\Projects\Software Reverse Engineering\Love Live SIF (PS4)` (this folder) | Main project: docs, notes, plans, our code | Read/write |
| `D:\Games\PS4\Love Live School Idol Festival` | Game assets: PKG (`Love.Live.School.Idol.Festival-CUSA24620.v1.02.pkg`), DLC folder, game dump with `eboot.bin` under `CUSA24620`, extracted DLC under `Extracted` | **Read-only** |
| `D:\Games\PS4\pt-pc` | Reference repository (PT port) | **Read-only** |
| `D:\Games\PS4\shadPS4` | shadPS4 emulator source (GPL-2.0): emulation reference and NID names (`src/core/aerolib/aerolib.inl`) | **Read-only** |
| `D:\Games\PS4\pt-pc-game` | User's own P.T. copy (pt-pc's input): PKG `PS4_P.T._Playable_Teaser_Silent_Hills_Demo_CUSA01127.pkg` and dump `CUSA01127` with `eboot.bin` | **Read-only** |

Subfolders here:

- `analysis/` : findings about pt-pc and about SIF (architecture, formats, imports, comparisons).
- `plan/` : roadmap, milestones, task list, decision log.
- `repository/` : our own code for the port (loader, HLE, tools, build files).

## Rules

1. Never modify, move, or delete files in the assets folder, pt-pc, the P.T. game folder, or shadPS4.
2. Read the actual files before drawing conclusions. Cite file paths (and function/line where useful). Label guesses as guesses.
3. Work on files in place on the user's computer; don't copy large binaries into the cloud workspace unless a step truly needs it.
4. Put findings in `analysis/`, plans and decisions in `plan/`, code in `repository/`. Keep derived or extracted big files out of `repository/` source (use a `work/` subfolder that is not committed).
5. Keep game-specific patches separate from generic code.
6. Ask before bulk extraction, writes outside this folder, or installing heavy toolchains.
7. Don't put game assets, `eboot.bin`, keys, or other proprietary material into the repository or shared output. Only the user's own dumps; no help with piracy or online-service circumvention.

## Rules for `analysis/` and `plan/`

1. Every new analysis or plan topic gets its own folder.
2. Folder name: `<timestamp>-<topic>`, where the timestamp is `YYYYMMDDHHmm` in the user's local time (Asia/Jakarta) and the topic is lowercase kebab-case. Example: `202610311300-analyze-game`.
3. Inside the topic folder, each result is a markdown file named `<timestamp>-analysis.md` (in `analysis/`) or `<timestamp>-plan.md` (in `plan/`). The timestamp is when that file was created.
4. Follow-up work on the same topic goes in the same folder as a new timestamped file; don't overwrite earlier results. Start a new folder only for a new topic.
5. Get the real current time (for example with `date`) instead of guessing it.

```
analysis/
└── 202610311300-analyze-game/
    ├── 202610311300-analysis.md
    └── 202610311422-analysis.md
plan/
└── 202610311300-build-the-port-version/
    └── 202610311300-plan.md
```

## Style

- Reply in the user's language (English or Indonesian); write project docs in English unless asked otherwise.
- The user is a student: briefly explain reverse-engineering terms the first time they appear.
- Keep chat replies short; put detail in files and point to them.

## Workflow

1. At session start, read the newest files in `plan/` and `analysis/` (highest timestamps) for current status.
2. Continue from the next unchecked item in the plan.
3. After meaningful findings, update `analysis/` and `plan/` before ending.
4. Current phase order: (1) understand pt-pc, (2) analyze SIF, (3) gap analysis and plan, (4) implement milestone by milestone.
