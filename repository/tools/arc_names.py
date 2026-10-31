"""SIF ARC entry name hash (u32 at record +0x08) and a check against a pac_list.py listing.

Hash (recovered in M1 from 83,998 named entries, 0 mismatches): djb2 without the 5381 seed, then h += h >> 5.
  h = 0; for each byte c of the UTF-8 name: h = h * 33 + c (mod 2^32); return (h + (h >> 5)) mod 2^32
Nameless entries (SHP/SARC shader parts, message.pac sub-tables) store hash 0, so there are no names to recover.
Usage: py -3.14 -I arc_names.py <pac-list.csv>        verify every named row
       py -3.14 -I arc_names.py --hash <name> [...]    print hashes
"""
import csv
import sys


def name_hash(name: str) -> int:
    h = 0
    for c in name.encode("utf-8"):
        h = (h * 33 + c) & 0xFFFFFFFF
    return (h + (h >> 5)) & 0xFFFFFFFF


def verify(listing: str) -> None:
    with open(listing, encoding="utf-8") as f:
        rows = [r for r in csv.DictReader(f) if r.get("hash")]
    named = [r for r in rows if r["name"]]
    bad = [r for r in named if name_hash(r["name"]) != int(r["hash"], 16)]
    nameless_nonzero = [r for r in rows if not r["name"] and int(r["hash"], 16)]
    print(f"{len(named)} named entries, {len(bad)} hash mismatches; "
          f"{len(rows) - len(named)} nameless, {len(nameless_nonzero)} of them with a non-zero hash")
    for r in (bad + nameless_nonzero)[:10]:
        print("  ", r["path"], r["hash"])
    sys.exit(1 if bad or nameless_nonzero else 0)


if __name__ == "__main__":
    # self-check: values read from the game's own records (sandbox/viewer/viewer.pac, ui/member_card/001/common.pac)
    assert name_hash("viewer_params.bin") == 0x95773AC2
    assert name_hash("mbc_0001_20_T") == 0xAC585A61
    if sys.argv[1:2] == ["--hash"]:
        for n in sys.argv[2:]:
            print(f"{name_hash(n):08x}  {n}")
    else:
        verify(sys.argv[1])
