# SIF `.pac` archives: `ARC\0` and `cmp\0`

Status: **verified on all 3,552 `.pac` files of CUSA24620 v1.02** (M1, 2026-10-31): 16,650 ARC tables, 119,372 entries, 0 parse failures, 0 hash mismatches. Field meanings marked *Hypothesis* are not yet confirmed from the eboot.

Readers: `tools/pac_list.py` (list), `tools/pac_extract.py` (extract), `tools/arc_names.py` (name hash). All values are little-endian unless noted.

## `cmp\0` compression wrapper

| Offset | Type | Field |
|---|---|---|
| 0x00 | char[4] | `cmp\0` |
| 0x04 | char[4] | method: `zlib` or `lzma` |
| 0x08 | u32 | packed size (= file or payload size − 16) |
| 0x0C | u32 | unpacked size |
| 0x10 | bytes | stream |

- `zlib`: a normal zlib stream (`78 9c`). 8,292 uses (44 top-level files, the rest nested).
- `lzma`: an LZMA-alone stream **without its 8-byte size field**: 5 property bytes (`5d 00 00 01 00`: lc=3 lp=0 pb=2, 64 KiB dictionary), then the coded data. To decode with a standard LZMA-alone decoder, insert the unpacked size as a u64 after the 5 property bytes. 2 uses: `common/common.pac`, `common/common2.pac`.
- The unpacked data is again an `ARC\0` in every case seen.

## `ARC\0` table

| Offset | Type | Field |
|---|---|---|
| 0x00 | char[4] | `ARC\0` |
| 0x04 | bytes[2] | `00 01` in all 16,650 tables (version, *Hypothesis*) |
| 0x06 | u16 | entry count *n* (0 occurs for empty nested tables) |
| 0x08 | u16 | entry count again (always equal) |
| 0x0A | bytes[6] | zero |
| 0x10 | char[32] | archive name, NUL-padded (e.g. `c11.pac`, `FirehawkArchiveDatas`) |
| 0x30 | u32[n] | entry data offsets, ascending, relative to the table start |

Entry records follow the offset array, starting at the next 0x20 boundary. Each record:

| Offset | Type | Field |
|---|---|---|
| 0x00 | char[4] | tag, NUL-padded (`tex`, `mdl`, `mot`, `scd`, `bin`, `acb`, `SHP`, `ARC`-carrying tags such as `texs`, `mots`, `mdls`, ...) |
| 0x04 | u16 | group or ID (*Hypothesis*: e.g. `0x032C` = 812 for `ef_812_ff_text00.efxa`; often 0) |
| 0x06 | u16 | entry index (*Hypothesis*: equals the record's position in the samples checked) |
| 0x08 | u32 | name hash (below); 0 for nameless entries |
| 0x0C | u16 | zero |
| 0x0E | u16 | name length including the NUL (1 for nameless entries) |
| 0x10 | u32 | zero |
| 0x14 | char[] | name (ASCII in all 83,998 named entries) |

The record is padded to the next 0x20 boundary.

### Entry data

At each `offsets[i]`:

| Offset | Type | Field |
|---|---|---|
| 0x00 | u32 | payload size |
| 0x04 | u16 | header length: 0x80 (4 for the 4 nameless entries in `message/message.pac`) |
| 0x06 | u16 | flags: 1 exactly when the payload is a `cmp\0` wrapper (8,248 of 8,248), else 0 |
| header length | bytes | payload |

Entry data blocks are padded to 0x80 in the samples checked. Payloads are either a leaf file or another `ARC\0` / `cmp\0` (nesting depth up to 3 below the top-level file).

### Name hash

```
h = 0
for each byte c of the name: h = (h * 33 + c) mod 2^32
hash = (h + (h >> 5)) mod 2^32
```

This is djb2 without the usual 5381 seed, plus a final `h += h >> 5`. It matches all 83,998 named entries. It was found from the data: names that differ by one digit have hash differences of 1, 34, 1123, 37060, ... (each 33 × previous + 1) by position from the end. Nameless entries (26,196 `SHP` and 880 `SARC` shader parts in `shader/ps4/sh_custom.pac`, 4 `pac` sub-tables in `message/message.pac`) store hash 0, so there are no hidden names to recover.

## Leaf payloads (inner magic, counts from the full listing)

| Tag | Count | Inner magic |
|---|---|---|
| `tex` | 40,101 | `btx\0` (8,248 of them inside `cmp`) |
| `SHP` | 26,196 | none (shader program parts) |
| `scd` | 11,589 | `Scor` |
| `btl` | 6,911 | `BTL\0` |
| `mot` | 4,707 | `BMAR` |
| `mdl` | 3,534 | `BMAR` |
| `bin` | 2,081 | 1,698 `BLBN`, 335 `BTL\0`, 36 Iggy (`49 67 0a ed`), 12 other |
| `acb` | 1,232 | 1,229 `ACGP`, 3 CRI `@UTF` (*Hypothesis*: `ACGP` is a game-own format, not CRI; check in M2) |
| `SARC` | 880 | text starting with the shader name |
| `efx` | 333 | `01 02 08 05` |
| `canm` | 312 | `BSCM` |

Total leaves: 97,980 (89,732 stored plain + 8,248 inside `cmp`).
