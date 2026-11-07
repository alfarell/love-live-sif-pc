# Add-ons (DLC) and `common/common.pac/dlc.pac`

Read by `src/game/dlc/dlc.{h,cpp}`.

## What an add-on is

Each add-on (`*-DLC-LLSIFACHMMUSnnnn` for a song, `*-DLC-LLSIFACHMCOSnnnn` for a costume) holds only `sce_sys`:
- `param.sfo`
- `license.dat`
- `license.info`
- `icon0.png`
- `psreserved.dat`

It has no game data: the songs and costumes are already in the base dump. An add-on is the right to use them. The label that names the item is the last part of the add-on's content id: `JP0082-CUSA24620_00-LLSIFACHMMUS0001` → `LLSIFACHMMUS0001`.

The port reads the content id from two places:
- An extracted add-on: the `CONTENT_ID` key of `sce_sys/param.sfo`.
- A `.pkg` file: the header (`\x7FCNT`, content id at 0x40, 36 characters).

## Tables (BLBN, one symbol each)

| File | Symbol | Rows | Row | Fields used |
|---|---|---|---|---|
| `dlc_music_data.bin` | `ll3::dlc::s_dlc_music_datas` | 117 | 0x60 bytes | +0x00 id (200000 + song), +0x08 / +0x10 u64 offsets of the label's two halves (`LLSIFACH` + `MMUS0001`), +0x1C song id; then 8 u64 string offsets: the title and description in Japanese, English, simplified and traditional Chinese |
| `dlc_dress_data.bin` | `ll3::dlc::s_dlc_dress_datas` | 63 | 0x68 bytes | +0x00 id (100000 + costume × 10), label as above, +0x20 costume number (`dNNN`, the `s_dress_datas` id), +0x24 2 (not decoded); then the names and descriptions |
| `dlc_set_data.bin` | `ll3::dlc::s_dlc_set_datas` | | 0x88 bytes (*Hypothesis*) | bundles: +0x00 id (3xxxxx), label, then up to 6 song add-on ids (200010, 200720, …). Not used: the user's add-ons are single songs and costumes |

## Which items need an add-on

- **Songs:** 117 of the 125 songs need one. The base game holds 0120, 0130, 0340, 0670, 1120, 1320, 1370 and 1460.
- **Costumes:** the 63 costumes in the table need one. The others, including the school uniform `d000`, are base game.
- The label numbers are not the song numbers: `MMUS0001` is song 0010, `MMUS0002` is 0720, `MMUS1049` is 1550.
