# Playing the SIF port

`sif.exe` (built as in [BUILD.md](BUILD.md)) plays "Love Live! School Idol Festival ~after school ACTIVITY~ Wai-Wai!Home Meeting!!" from **your own dump of the PS4 game** (CUSA24620). The port contains no game data. It reads the game's own files from your dump, and it never writes to that folder.

## What you need

- **Your own dump of the game:** a folder named `CUSA24620` with `sce_sys`, `common`, `sound`, `live` and the other game folders inside, made from your own copy. The port does not read `.pkg` files for the base game and does not help obtain or unpack games.
- **Optional, your own add-ons (DLC):** extracted add-on folders or add-on `.pkg` files.
  - By default the port looks in folders named `Extracted` and `DLC` next to `CUSA24620`.
  - Elsewhere: set `dlc_folder` in `settings.ini` (see below).
  - Songs whose music ships as add-ons are found in the dump's `sound/dlc/`.

## First start

Start `sif.exe`, for example by double-clicking it.

1. **The first time,** the port says that no game folder is set. Choose **Choose folder...** and pick your `CUSA24620` folder. Picking the folder that contains it also works. A wrong folder is refused with the reason; choose again or **Quit**.
2. **The folder is remembered** in `settings.ini`, and the game starts with its logos and title screen.

Next to `sif.exe`, a `data` folder is created:

| File | Content |
|---|---|
| `settings.ini` | Settings (below) |
| `save.txt` | Your progress, a plain-text save of the port's own |
| `sif.log` | What happened; useful when something goes wrong |
| `sif-crash-*.dmp` | Only after a crash |

To keep several setups apart, start with `--data <folder>`.

## Controls

| | Keyboard | Controller (any SDL3 gamepad, e.g. DualShock 4) |
|---|---|---|
| Move in menus | Arrow keys | D-pad |
| Confirm | Enter | Cross or Start |
| Back | Esc | Circle |
| The 9 lanes in a song, left to right | `A S D F Space J K L ;` | L2, L1, Left, Down, Right or Square (centre), Cross, Circle, R1, R2 |
| Leave a song | Esc | Start |
| Next page / tab (Task List, Profile, Costumes) | PageUp / PageDown | L1 / R1 |
| Another line from the navigator (top menu) | T | Triangle |

Options → Control Settings changes the controller's lane buttons.

## Playing

- **Top menu:** Left/Right turn the wheel, Up/Down pick an entry.
- **Playing a song:** Play leads to the song select. Pick a song, an arcade difficulty, a stage and a costume (Left/Right, then Enter at each step); the last Enter starts the live. Afterwards the result screen shows your score; any key returns to the song select.
- **Lives** show the stage with its screens playing the song's video, and the members dancing to the song's own camera.
- **Saves:** best scores, ranks and play counts are kept in `save.txt`. The song select shows each chart's best rank; the result screen marks new bests.
- **Costumes:**
  - every finished live gives materials;
  - Costumes → Costume Materials lists them;
  - Create/Change Costumes makes costumes and puts them on members (PageUp/PageDown changes the member).
- **Missions:** Event → Task List shows the missions and their progress; Enter claims a reward.
- **Profile:** Collection → Profile Card chooses the accolade, name plate and badge shown in the top menu.
- **Options:** Sound (volume) and Game Settings (note speed, timing offset).

**Timing feels late or early?** Start once with `sif.exe --calibrate` and press Space on each of the 24 clicks. The measured offset is saved and used from then on.

## Settings (`data/settings.ini`)

| Section | Keys |
|---|---|
| `[display]` | `width`, `height`, `fullscreen`, `vsync` |
| `[audio]` | `volume`, `buffer_frames` (`output_device` is not used yet) |
| `[game]` | `folder` (your `CUSA24620` dump), `dlc_folder` (your add-ons; empty = `Extracted` and `DLC` next to the dump) |
| `[play]` | `approach_ms` (note speed) |
| `[timing]` | `audio_offset_ms` (from calibration), `input_offset_ms` |
| `[controls]` | `pad_lanes` (the controller's lane buttons) |

## What the port does not have (yet)

- **Online features:** rankings, the add-on store and its trial movies, network events. There will be no purchasing.
- **Not built yet:**
  - home-version charts (3- and 6-lane modes);
  - skills and member or skill cards;
  - boosters, Friendship levels and events;
  - "Trying It On" and special costume colours;
  - the 3D navigator (the top menu shows Honoka's voice lines only).
- **Simplified:** some screen transitions, animations and small icons.

The project's roadmap lists the details.

## Command line (for testing)

```
sif.exe [--game DIR] [--data DIR] [--lang en|cn|tw|jp]
        [--title | --menu | --select | --live ID:VARIANT [--autoplay] [--stage stNNN] [--costume dNNN] [--member N]
         | --calibrate | --result-demo | --ui SCREEN:LAYOUT[:CLIP] | --show VFS_PATH | --movie VFS_PATH | --play SHEET:CUE|none]
        [--frames N] [--crash] [--route FILE [--shots DIR] [--hidden]]
```

| Option | Effect |
|---|---|
| (none) | Same as `--title`: logos, title screen, top menu |
| `--menu`, `--select` | Start at the top menu or the song select |
| `--lang` | UI language (default `en`) |
| `--live 10:4` | Play song 0010 on chart variant 04 directly (00-04 arcade difficulties) |
| `--autoplay` | Press every note on time |
| `--stage st001`, `--costume d003`, `--member 1` | Choose the stage, the costume, or a single member |
| `--game DIR` | Use this dump folder (saved like the first-run choice) |
| `--data DIR` | Use another data folder |
| `--frames N` | Close after N frames |
| `--route FILE`, `--shots DIR`, `--hidden` | Play a scripted route from `tests/routes/` (exit code 0 pass, 3 fail; `sif.log` says why), save its shots to DIR, without a visible window (shots then come out black) |
| `--result-demo`, `--ui …`, `--show …`, `--movie …`, `--play …`, `--crash` | Developer views: a fixed result screen, any UI layout, one texture, one movie, one sound, a crash-report test |
