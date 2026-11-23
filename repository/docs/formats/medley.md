# medley_data.bin (Live Show Medley)

`common/common.pac/medley.pac/medley_data.bin` is a BLBN file with one symbol, `ll3::medley::s_medley_datas`. It holds the recommended Set Lists of Live Show Medley: 20 rows of 0x120 bytes. The eboot loads it in `FUN_000ccc80`. Read by `src/game/medley/medley.cpp`.

| Offset | Type | Content |
|---|---|---|
| +0x00 | u32 | id 1-20 |
| +0x08 | 4 × u64 | file offsets of the name in Japanese, English, Chinese (simplified), Chinese (traditional) |
| +0x28 | u32 | group: 0 µ's (ids 1-10), 1 Aqours (ids 11-20) |
| +0x2c | u32 | the stage of the whole list (`stage/stNNN.pac`: 1-19, 101-112) |
| +0x30 | 20 × u32 | song ids; 0 = empty (the list ends there) |
| +0x80 | 20 × i32 | the costume (`dNNN`) per song; -1 = empty |
| +0xd0 | 20 × u32 | the special colour per song (0 = normal; 1-5 seen) |

## Related data in the eboot

- **Your own Set Lists:** in the game's save at `+0x531d8 + group × 0xf7c + slot × 0x18c`, 10 per group. Each is 20 entries of 16 bytes (chart, member, member card, colour), the count, the stage and the name. The port keeps its own format (`save.txt` lines `medley` and `medley_name`).
- **The camera between songs:** `s_medleyIntervalCameraAnimParams` in `common/common.pac/stage_common.pac/stage_params.bin`, rows of 0x18 bytes (`FUN_0023b590`). Not decoded.

Sources: `analysis/202611231413-live-show-medley/` (R1 eboot trace, R2 shadPS4 run).
