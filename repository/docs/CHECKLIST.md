# Release checklist (M9)

A person's check of every screen, done once on the Release build (`work\build\release\sif.exe`). Tick a box when the screen looks and behaves right; write a short note under a box that does not.

The routes (`tools/routes.py`, see [BUILD.md](BUILD.md)) already check that each flow works and what it logs. This list is about what the routes cannot see: how things look, sound and feel. Where a route saved a picture, it is named after **Shot**. With `--shots`, the pictures are in `work\routes\<data folder>\shots\<route>\`, and `work\routes\shots_sheet.png` collects them on one page.

**Checked by the user:** 2026-11-10, everything except three items; those three (the top-menu wheel, the end of the song, Game Settings) after fixes F1-F4, 2026-11-14. Every box is ticked.

Your own play is the reference. [PLAY.md](PLAY.md) lists the keys and what is not built yet; the items here do not ask for those.

## Build and first start

- [x] **Build:** [BUILD.md](BUILD.md)'s steps build `sif.exe` without errors.
- [x] **First start** with an empty data folder (`sif.exe --data work\try`): a box explains that no game folder is set, with **Choose folder...** and **Quit**.
- [x] **Wrong folder:** choosing a folder that is not the dump (for example one with only a `.pkg`) shows the reason and lets you choose again.
- [x] **Right folder:** choosing `CUSA24620`, or the folder above it, starts the game. The next start skips the box.
- [x] **Double-clicking** `sif.exe` opens the title screen. No developer view.

## Boot (route `boot`; Shot: `boot_logo`, `boot_menu`)

- [x] The CRI and Square Enix logos, then the notice, then the title screen; any key moves on.
- [x] The title screen looks like the game's: logo, background, "press any key".

## Top menu (routes `boot`, `talk`; Shot: `boot_menu`)

- [x] **The wheel:** Left/Right turns it, every icon shows, the submenu fans beside the current icon (2 to 5 entries), Up/Down picks an entry and the focused banner turns coloured.
- [x] **The header:** name plate, accolade and badge.
- [x] **Voice:** Honoka greets you by the time of day; T plays another line. (Since M13 the greeting is your chosen member's.)
- [x] **Esc** leaves the game.

## Song select (routes `career_1_play`, `select_back`)

- [x] **Song list:** covers, titles, and each chart's best rank after you have played it.
- [x] **The four steps:** song, difficulty, stage, costume. Each is clear on screen, and Enter moves on.
- [x] **Esc** goes back one step; at the song list it returns to the top menu.
- [x] **Add-ons:** the songs and costumes of your own add-ons appear and can be played.

## Live (routes `live_mus`, `live_aqours`, `live_saintsnow`, `play_keys`; Shot: `live_mus`, `live_aqours`, `live_saintsnow`)

- [x] **Stage and members:** the stage, the members dancing in time, the song's camera.
- [x] **Stage screens:** they play the song's video, moving with the music.
- [x] **Indicators and notes:** nine indicators with their button icons. Notes come out from under the microphone and are the indicator's size when they arrive.
- [x] **Holds:** a white band with a solid white end cap; both turn pink while held.
- [x] **Simultaneous notes:** brackets that alternate orange and blue. A pair of holds has brackets at both ends, in opposite colours.
- [x] **A hold ending on a press:** when a hold ends just as another note is to be pressed (0010 HYPER at 13.3 s: the ↓ hold ends on an R1 press), the hold's end cap and the other note both wear brackets, in the same colour.
- [x] **Judgement text** at each lane (PERFECT, GREAT …), and the **hit sounds**, one per judgement.
- [x] **Combo** low in the centre, see-through.
- [x] **Top bar:** score, the rank gauge, LOVELIVE! letters lighting up with the rhythm icons.
- [x] **Finale:** after all nine icons, the finale section with yellow notes and the higher score.
- [x] **End of the song:** ALL PERFECT or FULL COMBO when earned, then LIVE CLEAR once the music is over; the members keep dancing, the screen fades to white, then the result (about 10 s in all).
- [x] **Timing:** presses on the beat give PERFECT; if not, `sif.exe --calibrate` fixes it.
- [x] **Smoothness:** no stutter, and the music never drifts from the notes.

## Result (route `career_1_play`; Shot: `career_result`)

- [x] Score, max combo, the five counts and the rank match the play.
- [x] NEW marks a new best only.
- [x] Any key returns to the song select. (Since M11 the first key turns to Items Obtained; see below.)

## Costumes (routes `costumes`, `materials`; Shot: `costumes`, `costumes_made`, `materials`)

- [x] **Costume Materials:** the materials you own, for µ's and Aqours.
- [x] **Create/Change Costumes:** each costume's state (Obtained, Ready to Create, Insufficient Materials, download only).
- [x] **Making a costume** uses the materials, and the costume becomes Obtained.
- [x] **Putting a costume on** a member (PageUp/PageDown changes the member): the member wears it in the next live. (Since M13 the screen shows your current member; R2 changes her, and L1 is the special colour.)

## Task List (route `career_3_missions`; Shot: `missions`)

- [x] Tabs, progress and pages.
- [x] A completed mission's reward can be claimed once (GET! → COMPLETED).

## View Live Show, photos and profile cards (M12; routes `live_view`, `photo_gallery`, `member_card_collection`, `profile`, `profile_menu`, `profile_create`, `live_intro`; Shot: `view_live`, `view_member`, `view_end`, `photo_gallery`, `member_card`, `profile_menu_card`, `profile_edit`, `live_intro`)

- [x] **View Live Show:** Play → View Live Show has no difficulty step; its Prepare shows the camera help; the live has no notes and the camera bar at the top.
- [x] **Cameras:** R1 switches MV / Member / View; the Member camera shows a member's face from the front and follows her (D-pad switches members); arrows, L1 and L2 move the camera.
- [x] **Photos:** ○ takes a photo (the picture has no camera info), up to 5; the thumbnails show at the bottom left.
- [x] **End the Live Show:** the song, stage and member card, the photos large and small; □ marks favourites; after Enter the photos are in `data/photos`.
- [x] **Photo Gallery:** newest first, favourites marked and counted (25 at most), Delete refuses a favourite, Space shows a photo full screen.
- [x] **Member Card:** member tabs; owned costumes' cards face up, the others as card backs.
- [x] **Profile Card menu:** no photo: Create is not selectable; with a photo: Create opens the editor; the saved card shows on the left.
- [x] **Edit Profile Card:** the tabs and lists; a landscape frame turns the card; the photo position moves the photo; up to three skill cards; Finish Editing saves, and the card is still there after a restart.
- [x] **Live intro:** before a played live, the member card, the skill cards and "Profile Card Support" with two NPC cards of the song's group (each a whole card picture, e.g. "Umi's fan").

## Missions complete (M15; routes `career_3_missions`, `career_1_play`, `live_view`, `talk`; Shot: `missions`, `missions_aqours`, `missions_aqours_member`)

- [x] **Member tab:** only your current member's missions, with her name in them ("Go Chika!", "Chatting with Chika").
- [x] **Group tab:** the current mode's missions; the µ's and Aqours lists keep their own progress (play a µ's live, then check that Aqours' "Play a Live Show!" did not move).
- [x] **A series** shows one step: after claiming "play 1 time", the next step appears in its place.
- [x] **Songs and Costumes tabs:** only the current group's songs and the costumes its members wear.
- [x] **Reward pictures** with "x N": skill cards, items, boosters, accolade plates, name plates, and the member's card in a costume reward.
- [x] **Counting:** a finished View Live Show, photos, the Member camera's minutes on a member, the top menu's phrases and costumes, a made profile card and collected materials move their missions.
- [x] **Rewards N** on the tabs matches the plates showing GET!.

## Friendship, Wai-Wai! Boosters and birthday events (M14; routes `career_1_play`, `profile_menu`, `boosters`, `events`; Shot: `career_items`, `profile_menu_card`, `boosters_menu`, `boosters_prepare`, `boosters_items`, `events_menu`, `events_list`, `events_page`, `events_received`)

- [x] **Friendship:** after a played live, Items Obtained shows "Friendship Points Obtained", "Lv." and "To Next Level" with the bar; more lives raise the level; the profile card's heart shows her level.
- [x] **Login bonus:** the day's first visit to the top menu adds a booster (or 10 key materials on even days); the header shows "Wai-Wai! Booster N".
- [x] **Using a booster:** Prepare shows "Wai-Wai! Booster / You Have N"; □ (Q) shows "Used" and N − 1; the live gives about 10 more material units; the header shows the new count.
- [x] **Ongoing Events:** Event → Ongoing Events lists the current mode's members with their event dates (e.g. Chika "Jul 25th 0:00 to Aug 1st 23:59") and banners; Enter opens a member's page with her points and two reward cards.
- [x] **A running event** (start with `--date` inside a member's window, e.g. `--date 2026-07-28` for Chika in Aqours mode): "Now Open!" on her plate and her banner on the top menu; lives with her add birthday points; at 1000, "Receive rewards" gives her two skill cards.
- [x] **Task List:** the login, booster and Friendship missions count, and a booster reward raises the header count.

## 3D navigator, member choice and Trying It On (M13; routes `navigator`, `navigator_special`, `member_choice`, `live_member`, `try_on`, `live_special`; Shot: `navigator_honoka`, `navigator_chika`, `member_list`, `member_profile`, `member_umi`, `member_aqours`, `member_list_aqours`, `try_on_worn`, `try_on_next`, `try_on_special`, `try_on_menu`)

- [x] **Top menu:** your member stands in 3D on the right, in her own room and her costume; she stands, now and then plays a small idle move, and moves while she talks (greeting, T).
- [x] **R2 (End): Switch Members:** "Choose a Member." with the faces and names; L1/R1 change between µ's and Aqours (with Saint Snow); Cross opens her profile (school year, birthday, blood type, favourite and hated food, sub-unit) and "Continue with this member?"; OK puts her in the room.
- [x] **L2 (Home): Toggle Mode:** µ's ↔ Aqours at once, with that mode's member; the header's logo and plate follow her group.
- [x] **Kept:** after a restart the same mode and member are back.
- [x] **She plays the lives:** the live intro shows her member card; her skill cards and photos are hers.
- [x] **Create/Change Costumes:** she stands on the left in 3D; moving the focus dresses her in that costume ("Trying It On"), also one not made yet; Esc and back: she wears her own costume again.
- [x] **Special colours:** L1 on a costume that has one ("Change to special color") shows the other colours, the hint turns to "Default"; Enter keeps it, and the top menu and the next live show her in it.

## Profile Card (route `profile`; Shot: `profile`)

- [x] The Accolade, Name Plate and Badge tabs. (Since M12 Collection → Profile Card opens a menu first; Change Accolade or Name Plate is its first button.)
- [x] Choosing an item updates the preview and the top-menu header, and it is still chosen after a restart.

## Home charts: Play Live Show (M10; routes `home_select`, `home_unlocked`, `live_home`, `controls_home`; Shot: `home_difficulty`, `home_unlocked`, `home_live`, `live_home`, `controls_home`)

- [x] **Menu:** Play → Play Live Show opens the song select with the pink "Play Live Show:" header; Play Live Show (AC) keeps the arcade select with the green "Play Live Show(AC):" header.
- [x] **Difficulty step:** four cards, BEGINNER, STANDARD, HYPER, ADVENTURE, with the song's levels; STANDARD is chosen first.
- [x] **ADVENTURE lock:** it shows a padlock and "Unlock by getting A or higher in HYPER", and opens after an A (or better) on that song's HYPER.
- [x] **The live:** six rings: blue L1 and R1 in the top corners, Left and Circle at the sides, Down and Cross low. L1/R1 notes are blue. BEGINNER and STANDARD use only L1, Down, Cross and R1.
- [x] **Keys:** `S D F J K L` on the keyboard; L1, Left, Down, Cross, Circle, R1 on the controller.
- [x] **Control Settings:** the list has BEGINNER/STANDARD, HYPER/ADVENTURE and the AC row; Modify on a home row opens the 6-lane Button Controls, and a change there is used in that row's lives.
- [x] **Records:** each home difficulty keeps its own best rank and score.

## Skill and member cards (M11; routes `skill_collection`, `skill_select`, `live_skill`, `career_1_play`, `member_cards`; Shot: `skill_collection`, `skill_select`, `prepare_set`, `skill_banner`, `career_items`, `costume_step`)

- [x] **Prepare for the Live Show:** after the costume step; Skill Card in Use shows the played member's three cards (or "Skill not set."); Enter starts the live.
- [x] **Skill Card Selection:** only the played member's owned cards; Space on a slot then on a card sets it (a star marks cards in use); Page Down removes; OK keeps the choice after a restart.
- [x] **Collection → Skill Card:** member tabs (L1/R1, Tab for the group), owned cards face up, the others as card backs, Skill Info with the level.
- [x] **A skill in a live:** with Judge Support set, its banner appears when the first Rhythm Icon of phase 1 is hit, and slightly late presses are judged better for the card's seconds.
- [x] **Items Obtained:** after the results, a key shows the skill card the live dropped (picture, member, skill, level; "Lv. 1 → Lv. 2" for a copy) and the materials with ×gain and (total).
- [x] **Drops:** materials grow with the rank (about 5-8 units at D, 36-40 at SSS); a costume made for the song gives more.
- [x] **Select Costume:** the costume's picture, effect and description, and the played member's member card large on the right and on each costume plate.

## Options (routes `options_1_change`, `options_2_kept`, `controls`; Shot: `options_sound`, `control_settings`, `controls`)

- [x] **Sound:** the volume changes at once.
- [x] **Game Settings:** Rhythm Icon Speed (Slow to Fast) and timing offset take effect in the next live.
- [x] **Control Settings:** the controller's lane buttons can be changed, and T restores the defaults.
- [x] **Settings are kept** after a restart.

## Calibration (route `calibrate`)

- [x] `sif.exe --calibrate`: 24 clicks; pressing Space on them stores an offset, and judgement feels right afterwards.

## Languages (routes `lang_cn`, `lang_tw`, `lang_jp`; Shot: `lang_cn`, `lang_tw`, `lang_jp`)

- [x] `--lang cn`, `tw` and `jp` show the menus in that language.

## Controller

- [x] **Menus:** a DualShock 4 or another SDL gamepad works: D-pad, Cross to confirm, Circle to go back, Start.
- [x] **Lives:** the default lanes L2, L1, Left, Down, Right/Square, Cross, Circle, R1, R2.

## Docs

- [x] [PLAY.md](PLAY.md) matches what you saw, and its "not built yet" list is fair.

---

Checked by: ____________  Date: ____________  Build: `git log --oneline -1` = ____________
