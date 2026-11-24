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
| The 9 lanes of an arcade song (AC), left to right | `A S D F Space J K L ;` | L2, L1, Left, Down, Right or Square (centre), Cross, Circle, R1, R2 |
| The 6 lanes of a home song, left to right | `S D F J K L` | L1, Left, Down, Cross, Circle, R1 |
| Leave a song | Esc | Start |
| Next page / tab (Task List, Profile) | PageUp / PageDown | L1 / R1 |
| Top menu and Create/Change Costumes: Toggle Mode (µ's / Aqours) / Switch Members | Home / End | L2 / R2 |
| Create/Change Costumes: special colour | PageUp | L1 |
| Another line from the navigator (top menu) | T | Triangle |
| Favourite (Photo Gallery, End the Live Show); photo position (profile card editor) | Q | Square |
| View Live Show: camera mode / take photo / camera info | Page Down / P / Q | R1 / Circle / Square |
| View Live Show: move the camera / distance / height / switch members | Arrows / Page Up / Home / comma, period | Left stick / L1 / L2 / D-pad Left, Right |
| Live Show Medley: previous / next song / info | Page Up / Page Down / Q | L1 / R1 / Square |
| Live Show Medley Set Lists: start / edit / rename | Enter / Space / Q | Start (the game: touchpad) / Cross / Square |
| Set list editing: clear / insert / delete / change stage | Page Down / End / Home / Q | R1 / R2 / L2 / Square |

Options → Control Settings changes the controller's lane buttons, separately for BEGINNER/STANDARD, HYPER/ADVENTURE and the arcade charts (Modify on each row).

## Playing

- **Top menu:** Left/Right turn the wheel, Up/Down pick an entry. Your member stands in her room and talks to you (T: another line).
- **Your member:** R2 (End) opens "Choose a Member.": pick a member (L1/R1 for the other group), read her profile and confirm. L2 (Home) switches between µ's and Aqours (Saint Snow is with Aqours); each keeps its own member. Your member plays the lives, and her skill cards, photos and costumes are the ones used.
- **Playing a song:** Play → **Play Live Show** has the home charts (BEGINNER, STANDARD, HYPER, ADVENTURE, on 6 lanes); **Play Live Show (AC)** has the arcade charts (EASY to CHALLENGE, on 9 lanes). Pick a song, a difficulty, a stage and a costume (Left/Right, then Enter at each step). The costume step shows the played member's member card for each costume. Then **Prepare for the Live Show**: Enter (or LIVE START) starts the live, after a short screen with your member card, your skill cards and the two NPC profile cards that support the live. ADVENTURE opens once you get A or better on that song's HYPER. At the end ALL PERFECT or FULL COMBO shows if you earned it, then LIVE CLEAR. The result screen shows your score; a key turns to **Items Obtained** (the skill cards and materials the live gave; Left/Right pick a card), and another key returns to the song select.
- **Lives** show the stage with its screens playing the song's video, and the members dancing to the song's own camera.
- **Saves:** best scores, ranks and play counts are kept in `save.txt`. The song select shows each chart's best rank; the result screen marks new bests.
- **Skill cards:**
  - every finished live drops a skill card (a second one at rank S with Getting Skills Charge); a copy you already own raises its level, up to its maximum;
  - tasks give skill cards too;
  - Prepare for the Live Show → Skill Card Selection sets up to three of the played member's cards (Space sets a card, Page Down removes one);
  - Collection → Skill Card shows every member's cards (L1/R1 change the member, Tab the group);
  - in a live, a set card fires on its phase's skill note (the first Rhythm Icon of the phase) and its name shows at the top: Judge Support and Judge Plus help the timing, Combo Support keeps the combo on a Good, Long Notes Support finishes holds, Finale Support collects missed icons; material and drop skills act after the live.
- **Costumes:**
  - every finished live gives materials (more at higher ranks; a costume made for the song's materials helps);
  - Costumes → Costume Materials lists them;
  - Create/Change Costumes makes costumes and puts them on your member (R2 / L2 change her): she stands on the left trying on the focused costume, L1 (PageUp) shows a costume's special colour where it has one, and Enter keeps the costume and colour.
- **Missions:** Event → Task List shows the missions of your current member and mode: her group's, her own, her group's songs and the costumes she wears. A series of steps ("play 10 / 30 / 50 times") shows one step at a time. Each plate shows its progress and the reward's picture; Enter claims a completed one. The group missions count each group on its own (lives with a µ's member count for µ's). Lives, View Live Show, photos, the Member camera, the menu's talk and costumes, profile cards and the materials you collect all count; L2 on the top menu switches the list to the other group.
- **Friendship:** every played live gives your member Friendship Points (more on harder charts and higher ranks; the Friendship skill cards add to it). Items Obtained shows them with her level and the bar to the next one; her profile card's heart shows her level.
- **Wai-Wai! Boosters:** the header shows how many you hold (9 at most). The day's first visit to the top menu gives a login bonus: a booster or 10 key materials, in a 12-day cycle. In Prepare for the Live Show, □ (Q) uses one: the live then gives 10 more material units. Drop Support skill cards and some Task List rewards give boosters too.
- **Birthday events:** from 7 days before a member's birthday to the end of her birthday (by your computer's date), Event → Ongoing Events shows her event as "Now Open!" and the top menu shows its banner. Lives played with her add birthday points; at 1000 her page offers "Receive rewards": her two birthday skill cards and key materials. Each plate shows the member's event dates.
- **View Live Show and photos:** Play → **View Live Show** plays a song without notes. R1 (Page Down) switches the camera: MV (the song's camera), Member (one member close up; D-pad Left/Right or comma/period switch members) and View (the whole stage). In Member and View the arrows (left stick) move the camera, L1 (Page Up) moves it away (with Up: nearer), L2 (Home) raises it. ○ (P) takes a photo, up to 5 per live; □ (Q) hides the camera info. After the song, **End the Live Show** shows the photos: □ marks favourites, Enter saves them (up to 30; the oldest that is not a favourite makes room). The photos are `photos/NN.png` in the data folder.
- **Live Show Medley:** Play → **Live Show Medley** opens the Set Lists: L1/R1 switch between Your lists and the ten Recommended! ones, L2 between µ's and Aqours. Enter starts the chosen list after a confirmation. The songs play one after another on the list's stage, each with its own members in the list's costume; L1/R1 skip to the previous or next song, □ hides the info, and a 5-second title with the song's credits comes between songs. At the end, OPTIONS (Enter) offers to watch the list again, go back to the Set Lists or the top menu. On Your tab, Cross edits a list: Up/Down pick a line, Cross opens its slot (Up/Down pick Songs or Costumes, Left/Right change them, Enter keeps), R1 clears, R2 inserts and L2 deletes a line, □ changes the stage, Enter finishes and saves; □ outside editing renames the list with the keyboard. Medleys played to their end count for their missions.
- **Collection:**
  - **Photo Gallery** shows the photos, newest first: □ (Q) marks a favourite (25 at most), Delete removes one that is not a favourite, Space shows it full screen.
  - **Member Card** shows each member's member cards: the costumes you own face up (L1/R1 change the member, Tab the group).
  - **Profile Card** opens a menu: the current card on the left; **Change Accolade or Name Plate** chooses the accolade, name plate and badge shown in the top menu; **Create a New Profile Card** (once you have a photo) opens the editor.
- **Profile card editor:** Left/Right pick a tab (Card Frame, Photo, Accolade, Name Plate, Badge, Skill Card, Finish Editing), Down enters its list, Enter chooses. Each frame comes portrait and landscape. On the Photo tab □ (Q) adjusts the photo's position. Skill Card sets up to three of the member's cards (Enter again removes one). The Completed Image shows the card; **Finish Editing** saves it (one card in all; its photo is kept as `profile_card.png`).
- **Options:** Sound (volume) and Game Settings (Rhythm Icon Speed: Slow to Fast, as in the game; harder charts move faster at the same level; timing offset).

**Timing feels late or early?** Start once with `sif.exe --calibrate` and press Space on each of the 24 clicks. The measured offset is saved and used from then on.

## Settings (`data/settings.ini`)

| Section | Keys |
|---|---|
| `[display]` | `width`, `height`, `fullscreen`, `vsync` |
| `[audio]` | `volume`, `buffer_frames` (`output_device` is not used yet) |
| `[game]` | `folder` (your `CUSA24620` dump), `dlc_folder` (your add-ons; empty = `Extracted` and `DLC` next to the dump) |
| `[play]` | `note_speed` (Rhythm Icon Speed 0-4: Slow, A Bit Slow, Normal, A Bit Fast, Fast; an old `approach_ms` is converted once) |
| `[timing]` | `audio_offset_ms` (from calibration), `input_offset_ms` |
| `[controls]` | `pad_lanes` (the controller's 9 arcade lane buttons), `pad_lanes_home_easy` and `pad_lanes_home_hard` (the 6 home lane buttons for BEGINNER/STANDARD and HYPER/ADVENTURE); empty = the game's defaults |

## What the port does not have (yet)

- **Online features:** rankings, the add-on store and its trial movies, network events. There will be no purchasing.
- **Not built yet:**
  - received profile cards (online), the enlarged member card;
  - the skill voice and 3D skill effects, Shoutout, Member Focus's camera;
  - online events and rankings; the Friendship emblems on other players' cards;
  - the members' faces while they talk (lip sync) and blinking; the loading screen between members;
  - the "Change Costume?" question, the costume curtain;
  - the first-start member choice, and "You cannot play … with this member" for songs she does not sing.
- **Task List:** the "NEW" marks, the "Display by" order (□), the 3D member beside the list, and L2 / R2 inside the list (change the mode and member on the top menu). The Finale Focus, online and matching missions, and the HR member card missions, do not count yet. Missions that count from history (photos taken, phrases heard, costumes shown) start counting with this version; older lives are counted once from the save's records.
- **Not built either:** the song list per µ's / Aqours mode (all songs are listed in both), the "What will you do next?" box after a normal live, Prepare's Game Settings button and material panel, the login bonus pop-up, Player's Name, controller vibration.
- **Live Show Medley:** others' Set Lists (online); the card strip for picking a song, costume or stage (Left/Right step through them instead); the special colours in your lists; the 3D member on the end screen; the interval view is Sunshine Arena's beach shot on every stage.
- **Simplified:** some screen transitions, animations and small icons; a locked difficulty shows its padlock but is not darkened; the arcade CHALLENGE chart is open from the start (the game asks for A on EXTREME first).

The project's roadmap lists the details.

## Command line (for testing)

```
sif.exe [--game DIR] [--data DIR] [--lang en|cn|tw|jp]
        [--title | --menu | --select | --live ID:VARIANT [--autoplay] [--approach MS] [--stage stNNN] [--costume dNNN] [--member N]
         | --calibrate | --result-demo | --ui SCREEN:LAYOUT[:CLIP] | --show VFS_PATH | --movie VFS_PATH | --play SHEET:CUE|none]
        [--date YYYY-MM-DD] [--frames N] [--crash] [--route FILE [--shots DIR] [--hidden]]
```

| Option | Effect |
|---|---|
| (none) | Same as `--title`: logos, title screen, top menu |
| `--menu`, `--select` | Start at the top menu or the song select |
| `--lang` | UI language (default `en`) |
| `--live 10:4` | Play song 0010 on chart variant 04 directly (00-04 arcade difficulties) |
| `--autoplay` | Press every note on time |
| `--approach MS` | Note travel time in ms instead of the Rhythm Icon Speed (for tests) |
| `--stage st001`, `--costume d003`, `--member 1` | Choose the stage, the costume, or a single member |
| `--game DIR` | Use this dump folder (saved like the first-run choice) |
| `--data DIR` | Use another data folder |
| `--date 2026-07-28` | Use this date instead of the computer's (birthday events, the login bonus) |
| `--frames N` | Close after N frames |
| `--route FILE`, `--shots DIR`, `--hidden` | Play a scripted route from `tests/routes/` (exit code 0 pass, 3 fail; `sif.log` says why), save its shots to DIR, without a visible window (shots then come out black) |
| `--result-demo`, `--ui …`, `--show …`, `--movie …`, `--play …`, `--crash` | Developer views: a fixed result screen, any UI layout, one texture, one movie, one sound, a crash-report test |
