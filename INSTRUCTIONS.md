# Project Instructions: Love Live SIF (PS4) PC Port

## 1. Goal

Build a PC port of the PS4 game **Love Live! School Idol Festival (CUSA24620)** that runs natively on PC using the game's own PS4 assets and `eboot.bin`.

The reference is **pt-pc**, an existing project that does this for the PS4 game "PT". The work has three parts:

1. Analyze pt-pc to understand how it works.
2. Identify which parts are generic (reusable) and which are specific to PT.
3. Design and build the equivalent for Love Live SIF.

## 2. Folder Layout

| Folder | Role | Access |
|---|---|---|
| `E:\Projects\Software Reverse Engineering\Love Live SIF (PS4)` | **Main project folder.** Holds project description, these instructions, analysis results, plans, notes, and our own code/tools. | Read/write |
| `D:\Games\PS4\Love Live School Idol Festival` | **Game assets.** Contains the PKG (`Love.Live.School.Idol.Festival-CUSA24620.v1.02.pkg`), the DLC folder, a dump of the game (same content as the PKG, extracted) containing `eboot.bin` under `CUSA24620`, and extracted DLC under `Extracted`. | **Read-only** |
| `D:\Games\PS4\pt-pc` | **Reference repository.** The "PT" PS4-to-PC port. Used to learn the approach. | **Read-only** |
| `D:\Games\PS4\shadPS4` | **Emulator reference.** shadPS4 source checkout (GPL-2.0). Used as the reference for PS4 system-library behaviour and for NID names (`src/core/aerolib/aerolib.inl`). Do not copy its code into the repository unless the GPL decision (plan Part 8) allows it. | **Read-only** |
| `D:\Games\PS4\pt-pc-game` | **P.T. game copy.** The user's own copy of the game pt-pc ports: PKG (`PS4_P.T._Playable_Teaser_Silent_Hills_Demo_CUSA01127.pkg`) and dump `CUSA01127` with `eboot.bin`. Used to compare against SIF and to check pt-pc's docs. | **Read-only** |

Rules:

- Never modify, move, or delete anything in the game assets folder, in pt-pc, in the P.T. game folder, or in shadPS4. Treat all four as read-only.
- All notes, analysis, scripts, and code we produce go in the main project folder.
- Large derived files (decrypted/decompressed binaries, extracted assets) go in a clearly named subfolder such as `work/` or `extracted/` inside the main project folder, not beside the originals.

## 3. Structure of the Main Project Folder

```
Love Live SIF (PS4)/
  INSTRUCTIONS.md   this file
  CLAUDE.md         guidance for Claude sessions
  README.md         project overview and status
  analysis/         findings (pt-pc, SIF, comparison)
  plan/             roadmaps, milestones, decisions
  repository/       our port code and tools
```

Rules for `analysis/` and `plan/`:

- Every new analysis or plan topic gets its own folder named `<timestamp>-<topic>` (timestamp `YYYYMMDDHHmm`, local time Asia/Jakarta; topic in lowercase kebab-case), for example `202610311300-analyze-game`.
- Inside it, each result is a markdown file named `<timestamp>-analysis.md` or `<timestamp>-plan.md` (timestamp of when the file was created). Follow-up work adds a new timestamped file in the same folder; earlier files are not overwritten.

```
analysis/
  202610311300-analyze-game/
    202610311300-analysis.md
    202610311422-analysis.md
plan/
  202610311300-build-the-port-version/
    202610311300-plan.md
```

Large derived files (decrypted binaries, extracted assets) go in a `work/` subfolder that is not committed, never next to the originals.

## 4. Working Method

### Phase 1: Understand pt-pc
Read the repository before drawing conclusions. Document:

- Overall architecture and entry point; how the PS4 executable is loaded (SELF/ELF handling, decryption if any, segment mapping, relocations).
- How PS4 system calls and libraries are handled (HLE of `libkernel`, `libSce*` modules, NID resolution, symbol stubs).
- How graphics are handled (GNM/GNMX to a PC API, shader translation, command buffer handling).
- Audio, input, filesystem/path mapping, threading/sync, memory model (including how PS4 address space assumptions are met on PC).
- Build system, dependencies, language, and how the user supplies the game assets.
- What is **PT-specific** (patches, hooks, hardcoded addresses, game-specific workarounds) versus **generic**.

### Phase 2: Analyze Love Live SIF
Using the assets folder:

- Inventory the PKG, DLC, and dump: file tree, sizes, formats, `param.sfo`/metadata, `eboot.bin` and any PRX/SPRX modules.
- Determine the engine/middleware (the game may be Unity, a custom engine, or other; verify from the binary and files, do not assume).
- List imported system libraries/functions (NIDs) and compare against what pt-pc already implements.
- Identify asset formats and how the game reads them.
- Record anything that differs from PT in loading, graphics usage, audio, input, or online/network features.

### Phase 3: Gap analysis and plan
Produce a table of every required subsystem with status: *already covered by pt-pc approach / needs adaptation / needs new work / unknown*. Turn it into ordered milestones (for example: load the eboot, reach main, get logging working, boot to first frame, then input, audio, and so on).

### Phase 4: Implementation
Build incrementally. Each milestone should be runnable and testable. Prefer adapting pt-pc's design over inventing a new one, and keep game-specific patches isolated from generic code.

## 5. How Claude Should Behave in This Project

- **Evidence over assumption.** Base claims on files actually read. Cite the file path (and function/line where relevant) for findings. If something is a guess, label it as a guess.
- **Read before concluding.** Inspect pt-pc source and the SIF files directly instead of relying on general knowledge of PS4 internals; use general knowledge to guide where to look.
- **Work on the user's computer.** Files are on the user's machine. Use the linked-computer shell to list, search, and read them in place. Stage a file to the cloud workspace only when a tool there is genuinely needed (for example a specific analysis library). Don't copy large binaries around.
- **Keep the main folder current.** After meaningful findings, write them to `analysis/` in the main folder (and mirror key summaries into the Claude Project docs). Add a new timestamped file under `plan/` when the plan changes.
- **Be explicit about uncertainty and blockers**, such as encrypted or compressed segments, unknown formats, missing keys, or unverified assumptions, and propose how to resolve them.
- **Small steps, verified.** Prefer small tools/scripts that produce reproducible output (for example a script that lists imports from the eboot) over one-off manual inspection. Save those scripts in `repository/`.
- **Ask before destructive or large actions** (bulk extraction, anything that writes outside the main folder, installing heavy toolchains).
- **Language.** Reply in the language the user writes in (English or Indonesian). Write project docs in English unless the user asks otherwise. Keep explanations clear, since the user is a student learning this area; briefly explain unfamiliar reverse-engineering concepts when they first come up.
- **Format.** Keep chat replies concise. Put detailed analysis in files, and in chat give the conclusion plus where the details are.

## 6. Legal and Ethical Boundaries

- Work only with the user's own legally obtained dumps and files for analysis, interoperability, and learning.
- Do not include game assets, `eboot.bin`, keys, or other copyrighted/proprietary material in the repository or in any shared output. The port should load assets from the user's own dump at runtime, as pt-pc does.
- Do not help obtain or distribute pirated game content, console keys, or firmware.
- Do not help circumvent online services, anti-cheat, or purchase/licensing checks for the purpose of piracy. Offline interoperability and preservation-style analysis is the scope.

## 7. Definition of Done (by stage)

1. **Analysis complete:** pt-pc architecture documented; SIF inventory and import list documented; gap table written.
2. **Boot:** SIF `eboot.bin` loads on PC and executes to a known point with logging.
3. **Render:** first frames displayed.
4. **Playable:** input, audio, and file access work well enough to reach and use the main game screens.
5. **Polish:** stability, performance, config, and documentation so someone else can build and run it with their own dump.

## 8. First Actions for a New Session

1. Read `README.md` and the newest files in `plan/` and `analysis/` in the main folder (and the Claude Project docs) to see current status.
2. Continue from the next unchecked item in the latest plan.
3. If nothing exists yet, start with Phase 1: list the top level of pt-pc, read its README and build files, and create an analysis topic folder (for example `<timestamp>-analyze-pt-pc`) with the overview inside.
