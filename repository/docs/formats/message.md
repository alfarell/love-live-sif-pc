# `message.pac`: the game's message text

Decoded in M7 (`analysis/202611021940-m7-systems-breadth/`). Reader: `src/game/text/messages.{h,cpp}` (`sif::Messages`); UI lookup: `src/game/text/localizer.{h,cpp}` (`sif::Localizer`). Test: `tests/unit/message_test.cpp`.

## Framing

- `message/message.pac` is an ARC table of 4 nameless `pac` entries: the language packages, in the order Japanese, English, simplified Chinese, traditional Chinese.
- Each language package is an ARC of **816** nameless `bin` entries, one per message. The message id is the entry index and is the same in every language.
- The blocks in both ARCs are: u32 content size, u32 `4`, then the content.
  - **Pitfall:** the usual ARC block header is u32 size, u16 header length, u16 flags. Read that way, the header length is 4, so the content appears to start 4 bytes early and to end 4 bytes short.
  - That is why texts read with `pac_list.py`/`pac_extract.py` (and the generic `Vfs` path) look cut ("English Not Defi").

## A message

| Offset | Type | Meaning |
|---|---|---|
| 0x00 | u32 | Kind: 0 for text (795 of 816), 1 for 21 single-symbol fragments (e.g. Japanese 1 "■", 13 "×") |
| 0x04 | u16 | Offset of the text |
| 0x06 | u16 | Style rows: one per character, except line breaks (see below) |
| 0x08 | u16 | Characters in the text, line breaks included |
| 0x0A | u16 | UTF-8 bytes in the text, line breaks included |
| 0x0C | u32 | 0 |
| 0x10 | rows × (u16 flags, u16 offset) | One row per character. The offset is relative to the row and points at a style record |
| after the rows | 8-byte records | u32 type, u32 value |
| text offset | UTF-8, NUL | The text |

The header counts are therefore **`(rows, characters, bytes)`**, not three character counts. Japanese 814 is `(16, 17, 49)`: "協力してくれる\nプロフィールカード" has 17 characters (one is the line break) and 49 UTF-8 bytes, but only 16 style rows because a line break has no row. English 814 is `(20, 20, 20)`.

### Style rows

| flags | Meaning |
|---|---|
| 0 | Shared record: the rows point at one 8-byte record (observed in English 6 and 814). A row may also point exactly at the text offset, meaning it has no record at all (all Japanese 6 rows, Chinese/Taiwanese 6) |
| 1 | The row owns its 8-byte record |
| 2 | The row owns a 16-byte pair of records |

### Style records

Each record is `u32 type, u32 value` (little-endian):

| type | Meaning |
|---|---|
| 0 | Colour, *Hypothesis* ARGB: `00000000df070aff` (English text, blue-ish), `00000000333333ff`, `000000001d1c1dff`, `0000000000000000` |
| 1 | Size: 9 (body), 16 and 24. Japanese 613 "Lv.%d" gives 16 for "Lv." and 24 for "%d", so it is a font size |
| 2 | Only in the kind-1 fragments, always value 0 (`0200000000000000`) |

A flag-2 row's pair is an extra record followed by the row's normal record. English 814 rows 17–19 are `(type 0, 0xff0a07df)` then `(type 1, 9)`; Japanese 1 is `(type 0, 0)` then `(type 2, 0)`. What the extra record overrides is still open.

- Kind 1 (21 messages in Japanese, e.g. the symbols/initials "■", "×", "歌", "凛") is structurally normal: one row, a flag-2 pair, and a one-character text. The reader handles it like any other message.
- Line breaks are written as `\n` and some characters as `\uXXXX` (e.g. `　`, the ideographic space). The reader turns both into the characters.

## Translation coverage and fallback

- **English** fills only part of the table: 59 messages are placeholders, 5 "English Not Defined", 52 "Eng Music Not Defined" (song names) and 2 "Eng UI Not Defined". Japanese 300 is "START:DASH!!".
- Simplified and traditional Chinese have the same shape: `Schinese`/`SC Music`/`SC UI Not Defined` and `Tchinese`/`TC Music`/`TC UI Not Defined`.
- `Messages::Get` returns the selected language, then falls back to Japanese when the entry is empty or ends in "Not Defined". `Messages::Japanese` always returns the Japanese text.

## Using the messages on screens

The `ui_text.bin` tables keep **Japanese defaults even in the `_en`/`_cn`/`_tw` packages**. `Localizer` builds a map from exact Japanese message text to message id and replaces a screen's default with `Messages::Get(id)`; unrecognized defaults (dates, counters, the port's own values) stay as they are. Text the port supplies itself (options row labels) uses an explicit id.

Mapped so far (exact Japanese default -> id):

| UI text | Message id |
|---|---|
| メニュー (options title plate, box 0) | 706 |
| ライブ全般 (options title plate, box 1) | 707 |
| リズムアイコンの速度 / 判定のタイミング調整 | 711 / 712 |
| BGM音量 | 718 |
| 変更する | 737 |
| ライブプレイ / ライブプレイ(AC) | 755 / 756 |
| やや速い / すべての画面に表示する / プレイモード個別設定 | 731 / 721 / 708 |
| 現在のポイント / ポイント | 580 and 700 (same text) / 702 |
| 選択中 / 絞り込み・ソート | 541 / 647 |
| 使用プロフィールカード / 好きな食べ物 / 持っている数 | 606 / 135 / 608 |
| 材料情報 / みんなのプロフィールカードを受信する / おススメ！… | 610 / 650 / 625 |
| 楽曲 / 衣装 / ステージ / セット切替 / もっている材料 | 634 / 635 / 636 / 624 / 646 |
| result: 今までの記録: / 取得友情ポイント / 次のレベルアップまで | 542 / 546 / 547 |
| result: スキルカード / ステージ・衣装: / 今回撮影した枚数: / BDイベント参加中 | 550 / 567 / 575 / 577 |
| ingame: 撮影枚数 / (NPC Profile Card) | 657 / 815 |

A default with extra punctuation (e.g. `作詞:` vs the message `作詞`) is not matched; the screen keeps its default unless the port supplies that text.

## Samples

| Id | Japanese | English |
|---|---|---|
| 2 | ラブライブ！スクールアイドルフェスティバル | English Not Defined (falls back to Japanese) |
| 6 | 見習いプレイヤー | New Player |
| 7 | ゲストプレイヤー | Guest player |
| 9 | ＯＫ | OK |
| 10 | キャンセル | Cancel |
| 11 | はい | Yes |
| 300 | START:DASH!! | Eng Music Not Defined (falls back to Japanese) |
| 700 | 現在のポイント | Current Progress |
| 814 | 協力してくれる\nプロフィールカード | Profile Card Support |
| 815 | (NPC Profile Card) | (NPC Profile Card) |

## Open

- Which message a UI text box uses is now resolved for the boxes that keep a Japanese default (exact-text match). The port-supplied values still need explicit ids, and talk lines (`talk.pac`) are not mapped.
- The flag-2 pair's extra record (type 0 then type 1/2) and type 2's meaning.
- Colour values are assumed ARGB; the renderer does not use them yet.
