"""Dump SIF `Scor` charts (ARC tag `scd`, in live/<id>/<id>.pac/score.pac) to CSV with times in milliseconds.

Scor (M2, all 11,589 files: size and count always consistent; see docs/formats/scd.md):
  0x00 "Scor", 0x04 u32 file size, 0x08 u32 event count, 0x0C u32 first tempo (us per quarter note),
  0x10 u32 ticks per quarter (480 in all files), 0x14 u32 ?, 0x18 u32 ?, 0x1C u32 channel (= the _chN file suffix),
  0x20 u32 ticks per bar (1920/1440/960), events from 0x40, 32 bytes each:
  u16 id, u16 kind, u32 tick, then kind 0 (note): u32 note, u32 velocity, u32 duration ticks, u32 type;
  kind 2 (tempo): u32 us per quarter; kind 4 (marker): u32 0, u32 0, u32 length ticks, u32 value.
  Meaning of note numbers and types per channel is not known yet (U5, M4); the dump keeps raw values.
Usage: py -3.14 -I chart_dump.py <out.csv> <file.scd> [...]   (prints a one-line summary per file)
"""
import csv
import struct
import sys
from pathlib import Path


def parse(b: bytes) -> tuple[dict, list]:
    assert b[:4] == b"Scor", b[:4]
    size, count, tempo, ppq, _a, _b, channel, bar = struct.unpack_from("<8I", b, 4)
    assert size == len(b) == 0x40 + 32 * count, (size, len(b), count)
    events = []
    for i in range(count):
        ident, kind, tick, *rest = struct.unpack_from("<HHI6I", b, 0x40 + 32 * i)
        events.append({"id": ident, "kind": kind, "tick": tick, "f2": rest[0], "f3": rest[1], "f4": rest[2], "f5": rest[3]})
    events.sort(key=lambda e: (e["tick"], e["kind"] != 2))  # tempo changes first at equal ticks
    # tick -> ms through the tempo map
    us_per_q, last_tick, last_ms = tempo, 0, 0.0
    for e in events:
        last_ms += (e["tick"] - last_tick) * us_per_q / ppq / 1000
        last_tick = e["tick"]
        e["ms"] = round(last_ms, 3)
        if e["kind"] == 2:
            us_per_q = e["f2"]
    return {"tempo": tempo, "ppq": ppq, "channel": channel, "bar_ticks": bar}, events


def main() -> None:
    out = Path(sys.argv[1])
    with open(out, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["file", "channel", "id", "kind", "tick", "ms", "note_or_tempo", "velocity", "duration", "type"])
        for p in map(Path, sys.argv[2:]):
            head, events = parse(p.read_bytes())
            notes = [e for e in events if e["kind"] == 0]
            last = events[-1]["ms"] / 1000 if events else 0
            print(f"{p.name}: ch{head['channel']} {len(notes)} notes, {len(events) - len(notes)} other, "
                  f"{60e6 / head['tempo']:.1f} BPM, last event {last:.1f} s")
            for e in events:
                w.writerow([p.name, head["channel"], e["id"], e["kind"], e["tick"], e["ms"], e["f2"], e["f3"], e["f4"], e["f5"]])


if __name__ == "__main__":
    # self-check: tempo 500000 us/q (120 BPM), a note at tick 480 = 500 ms, tempo halves, note at 960 = 750 ms
    ev = [(1, 2, 0, 500000, 0, 0, 0), (2, 0, 480, 60, 100, 120, 1), (3, 2, 480, 250000, 0, 0, 0), (4, 0, 960, 61, 100, 120, 1)]
    body = b"".join(struct.pack("<HHI6I", i, k, t, a, b, c, d, 0, 0) for i, k, t, a, b, c, d in ev)
    hdr = b"Scor" + struct.pack("<8I", 0x40 + len(body), len(ev), 500000, 480, 0, 0, 1, 1920) + bytes(0x40 - 36)
    _, e = parse(hdr + body)
    assert [x["ms"] for x in e if x["kind"] == 0] == [500.0, 750.0], e
    main()
