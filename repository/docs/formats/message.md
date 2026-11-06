# `message.pac`: the game's message text

Decoded in M7 (`analysis/202611021940-m7-systems-breadth/`). Reader: `src/game/text/messages.{h,cpp}` (`sif::Messages`). Test: `tests/unit/message_test.cpp`.

## Framing

- `message/message.pac` is an ARC table of 4 nameless `pac` entries: the language packages, in the order Japanese, English, simplified Chinese, traditional Chinese.
- Each language package is an ARC of **816** nameless `bin` entries, one per message. The message id is the entry index.
- The blocks in both ARCs are: u32 content size, u32 `4`, then the content.
  - **Pitfall:** the usual ARC block header is u32 size, u16 header length, u16 flags. Read that way, the header length is 4, so the content appears to start 4 bytes early and to end 4 bytes short.
  - That is why texts read with `pac_list.py`/`pac_extract.py` (and the generic `Vfs` path) look cut ("English Not Defi").

## A message

| Offset | Type | Meaning |
|---|---|---|
| 0x00 | u32 | Kind: 0 for text (795 of 816), 1 for 21 others (e.g. Japanese 1 "■", 13 "x") |
| 0x04 | u16 | Offset of the text |
| 0x06 | u16 × 3 | Character count (e.g. 10 for "New Player", 17 for "Unregistered text") |
| 0x0C | u32 | 0 |
| 0x10 | count × (u16 flag, u16 offset) | One row per character. The offset is relative to the row itself and points at an 8-byte record. Rows with flag 0 share the first record; rows with flag 1 have their own |
| … | 8-byte records | u32 (0 in the samples) and `df 07 0a ff` (*Hypothesis:* a per-character style or colour) |
| text offset | UTF-8, NUL | The text |

- Line breaks are written as `\n` and some characters as `\uXXXX` (e.g. `　`, the ideographic space). The reader turns both into the characters.
- **English** fills only part of the table. Untranslated messages read "English Not Defined", and song names read "Eng Music Not Defined" (the Japanese 300 is "START:DASH!!").

## Samples

| Id | Japanese | English |
|---|---|---|
| 2 | ラブライブ！スクールアイドルフェスティバル | English Not Defined |
| 6 | 見習いプレイヤー | New Player |
| 7 | ゲストプレイヤー | Guest player |
| 9 | ＯＫ | OK |
| 10 | キャンセル | Cancel |
| 11 | はい | Yes |
| 700 | 現在のポイント | Current Progress |
| 814 | 協力してくれる\nプロフィールカード | Profile Card Support |

## Open

- Which message a UI text box or talk line uses is decided by the eboot or the talk data (not traced).
- Message 815 ends "(NPC Profile Ca" in every language: cut in the data itself, or framed differently.
- The per-character records (styles or ruby?), and kind 1.
