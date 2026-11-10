# Release checklist (M9)

A person's check of every screen, done once on the Release build (`work\build\release\sif.exe`). Tick a box when the screen looks and behaves right; write a short note under a box that does not.

The routes (`tools/routes.py`, see [BUILD.md](BUILD.md)) already check that each flow works and what it logs. This list is about what the routes cannot see: how things look, sound and feel. Where a route saved a picture, it is named after **Shot**. With `--shots`, the pictures are in `work\routes\<data folder>\shots\<route>\`, and `work\routes\shots_sheet.png` collects them on one page.

Your own play is the reference. [PLAY.md](PLAY.md) lists the keys and what is not built yet; the items here do not ask for those.

## Build and first start

- [ ] **Build:** [BUILD.md](BUILD.md)'s steps build `sif.exe` without errors.
- [ ] **First start** with an empty data folder (`sif.exe --data work\try`): a box explains that no game folder is set, with **Choose folder...** and **Quit**.
- [ ] **Wrong folder:** choosing a folder that is not the dump (for example one with only a `.pkg`) shows the reason and lets you choose again.
- [ ] **Right folder:** choosing `CUSA24620`, or the folder above it, starts the game. The next start skips the box.
- [ ] **Double-clicking** `sif.exe` opens the title screen. No developer view.

## Boot (route `boot`; Shot: `boot_logo`, `boot_menu`)

- [ ] The CRI and Square Enix logos, then the notice, then the title screen; any key moves on.
- [ ] The title screen looks like the game's: logo, background, "press any key".

## Top menu (routes `boot`, `talk`; Shot: `boot_menu`)

- [ ] **The wheel:** Left/Right turns it, Up/Down picks an entry, the focus is clear.
- [ ] **The header:** name plate, accolade and badge.
- [ ] **Voice:** Honoka greets you by the time of day; T plays another line.
- [ ] **Esc** leaves the game.

## Song select (routes `career_1_play`, `select_back`)

- [ ] **Song list:** covers, titles, and each chart's best rank after you have played it.
- [ ] **The four steps:** song, difficulty, stage, costume. Each is clear on screen, and Enter moves on.
- [ ] **Esc** goes back one step; at the song list it returns to the top menu.
- [ ] **Add-ons:** the songs and costumes of your own add-ons appear and can be played.

## Live (routes `live_mus`, `live_aqours`, `live_saintsnow`, `play_keys`; Shot: `live_mus`, `live_aqours`, `live_saintsnow`)

- [ ] **Stage and members:** the stage, the members dancing in time, the song's camera.
- [ ] **Stage screens:** they play the song's video, moving with the music.
- [ ] **Indicators and notes:** nine indicators with their button icons. Notes come out from under the microphone and are the indicator's size when they arrive.
- [ ] **Holds:** a white band with a solid white end cap; both turn pink while held.
- [ ] **Simultaneous notes:** brackets that alternate orange and blue. A pair of holds has brackets at both ends, in opposite colours.
- [ ] **Judgement text** at each lane (PERFECT, GREAT …), and the **hit sounds**, one per judgement.
- [ ] **Combo** low in the centre, see-through.
- [ ] **Top bar:** score, the rank gauge, LOVELIVE! letters lighting up with the rhythm icons.
- [ ] **Finale:** after all nine icons, the finale section with yellow notes and the higher score.
- [ ] **End of the song:** LIVE CLEAR, then the result.
- [ ] **Timing:** presses on the beat give PERFECT; if not, `sif.exe --calibrate` fixes it.
- [ ] **Smoothness:** no stutter, and the music never drifts from the notes.

## Result (route `career_1_play`; Shot: `career_result`)

- [ ] Score, max combo, the five counts and the rank match the play.
- [ ] NEW marks a new best only.
- [ ] Any key returns to the song select.

## Costumes (routes `costumes`, `materials`; Shot: `costumes`, `costumes_made`, `materials`)

- [ ] **Costume Materials:** the materials you own, for µ's and Aqours.
- [ ] **Create/Change Costumes:** each costume's state (Obtained, Ready to Create, Insufficient Materials, download only).
- [ ] **Making a costume** uses the materials, and the costume becomes Obtained.
- [ ] **Putting a costume on** a member (PageUp/PageDown changes the member): the member wears it in the next live.

## Task List (route `career_3_missions`; Shot: `missions`)

- [ ] Tabs, progress and pages.
- [ ] A completed mission's reward can be claimed once (GET! → COMPLETED).

## Profile Card (route `profile`; Shot: `profile`)

- [ ] The Accolade, Name Plate and Badge tabs.
- [ ] Choosing an item updates the preview and the top-menu header, and it is still chosen after a restart.

## Options (routes `options_1_change`, `options_2_kept`, `controls`; Shot: `options_sound`, `controls`)

- [ ] **Sound:** the volume changes at once.
- [ ] **Game Settings:** note speed and timing offset take effect in the next live.
- [ ] **Control Settings:** the controller's lane buttons can be changed, and T restores the defaults.
- [ ] **Settings are kept** after a restart.

## Calibration (route `calibrate`)

- [ ] `sif.exe --calibrate`: 24 clicks; pressing Space on them stores an offset, and judgement feels right afterwards.

## Languages (routes `lang_cn`, `lang_tw`, `lang_jp`; Shot: `lang_cn`, `lang_tw`, `lang_jp`)

- [ ] `--lang cn`, `tw` and `jp` show the menus in that language.

## Controller

- [ ] **Menus:** a DualShock 4 or another SDL gamepad works: D-pad, Cross to confirm, Circle to go back, Start.
- [ ] **Lives:** the default lanes L2, L1, Left, Down, Right/Square, Cross, Circle, R1, R2.

## Docs

- [ ] [PLAY.md](PLAY.md) matches what you saw, and its "not built yet" list is fair.

---

Checked by: ____________  Date: ____________  Build: `git log --oneline -1` = ____________
