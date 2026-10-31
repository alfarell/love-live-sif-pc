"""Compare a module's imports (dynlib.py --csv output) with the NIDs an HLE source tree registers.

Read-only on the source tree. "registered" = a LIB_FUNCTION("<nid>", ...) line exists (shadPS4 style); a
registered function may still be a stub, so this is an upper bound on real coverage.
Usage: py -3.14 -I nid_coverage.py <hle_src_dir> <out.csv> <imports.csv> [<imports.csv> ...]
"""
import csv
import re
import sys
from collections import Counter
from pathlib import Path

# last argument may be wrapped, e.g. ORBIS(posix_pthread_create), and may sit on the next line
LIB_FUNCTION = re.compile(r'LIB_FUNCTION\(\s*"([A-Za-z0-9+\-]{11})"\s*,\s*"([^"]+)"[^;]*?,\s*(?:[A-Z_]+\(\s*)?([A-Za-z_][\w:]*)\s*\)?\s*\)')
STUB_MARKERS = ("(STUBBED)", "(DUMMY)")


def body_of(text: str, func: str) -> str:
    """Text of the first definition `func(...) {...}` in text, by brace matching; '' if not found."""
    m = re.search(rf"\b{re.escape(func)}\s*\([^;{{]*\)\s*(?:noexcept\s*)?\{{", text)
    if not m:
        return ""
    depth, i = 0, m.end() - 1
    for j in range(i, len(text)):
        depth += {"{": 1, "}": -1}.get(text[j], 0)
        if depth == 0:
            return text[i:j + 1]
    return text[i:]


def main() -> None:
    src, out = Path(sys.argv[1]), Path(sys.argv[2])
    registered = {}
    for p in src.rglob("*.cpp"):
        text = p.read_text(encoding="utf-8", errors="replace")
        for nid, lib, func in LIB_FUNCTION.findall(text):
            if nid in registered:
                continue
            body = body_of(text, func.split("::")[-1])
            state = "unknown" if not body else "stub" if any(s in body for s in STUB_MARKERS) else "implemented"
            registered[nid] = (lib, p.relative_to(src).as_posix(), state)
        for nid, lib in re.findall(r'LIB_OBJ\(\s*"([A-Za-z0-9+\-]{11})"\s*,\s*"([^"]+)"', text):  # data objects
            registered.setdefault(nid, (lib, p.relative_to(src).as_posix(), "implemented"))
    rows = []
    for path in sys.argv[3:]:
        for r in csv.DictReader(open(path, encoding="utf-8")):
            if r["kind"] != "import":
                continue
            hit = registered.get(r["nid"])
            rows.append({"module_csv": Path(path).name, "library": r["library"], "nid": r["nid"], "name": r["name"],
                         "registered": "yes" if hit else "no", "state": hit[2] if hit else "missing",
                         "hle_file": hit[1] if hit else ""})
    with open(out, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    states = {}
    for r in rows:
        states.setdefault((r["module_csv"], r["library"]), Counter())[r["state"]] += 1
    assert sum(sum(c.values()) for c in states.values()) == len(rows)
    print(f"{len(registered)} NIDs registered in {src}")
    print(f"{'impl':>5} {'stub':>5} {'unkn':>5} {'miss':>5}  module / library")
    for k in sorted(states):
        c = states[k]
        print(f"{c['implemented']:5} {c['stub']:5} {c['unknown']:5} {c['missing']:5}  {k[0]} {k[1]}")
    t = Counter()
    for c in states.values():
        t.update(c)
    print(f"TOTAL implemented {t['implemented']}, stub {t['stub']}, unknown {t['unknown']}, missing {t['missing']} of {len(rows)}")


if __name__ == "__main__":
    main()
