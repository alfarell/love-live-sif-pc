"""Count game-specific marker terms per tracked source file of a git repository (read-only).

Usage: py -3.14 -I marker_scan.py <repo_dir> <out.csv> [--paths src tools ...]
Each marker group is a list of regexes; a file's class is the strongest group that hits it:
"game" (P.T.-specific) > "fox" (Fox Engine formats/hashes) > "none".
"""
import csv
import re
import subprocess
import sys
from pathlib import Path

GROUPS = {
    "game": [r"CUSA0(1127|1114|1098)", r"pt14_", r"chunk1\.psarc", r"texture\.qar", r"pathid_list",
             r"\b[Ll]isa\b|Lisa[A-Z_]|_lisa|lisa_", r"[Hh]allway", r"\bP\.T\.", r"PT_Save_Data", r"[Gg]ouge",
             r"\bJack\b", r"[Ss]ubliminal", r"[Hh]andy[ _]?[Ll]ight|[Ff]lashlight", r"och0_", r"SKL_\d{3}_",
             r"[Mm]aze_?[ABC]\b", r"\bnazo\b|Nazo"],
    "fox": [r"\b[Ff]ox[A-Z_]|\bfox::|foxfpk|FoxPackage", r"StrCode64|PathCode64", r"\.fpkd?\b", r"\bftexs?\b|\bFtex",
            r"\bfmdl\b|Fmdl", r"\bfox2\b|Fox2", r"\bqar\b|Qar", r"[Ww]wise|\.wem\b|\.bnk\b|HIRC", r"\.sbp\b|\.fsm\b|\.frig\b|gani",
            r"/Assets/|\bas/sh/", r"\buif\b|\buia\b|UIF|UIA\b|ffnt|FFNT", r"\bvfx_file|fsop"],
}
EXTS = {".cpp", ".h", ".hpp", ".mm", ".py", ".cs", ".glsl", ".vert", ".frag", ".comp", ".cmake", ".txt", ".lua"}


def main() -> None:
    repo, out = Path(sys.argv[1]), Path(sys.argv[2])
    paths = sys.argv[sys.argv.index("--paths") + 1:] if "--paths" in sys.argv else ["src"]
    files = subprocess.run(["git", "--no-optional-locks", "-C", str(repo), "ls-files", "-z", *paths],
                           capture_output=True, check=True).stdout.decode("utf-8").split("\0")
    compiled = {g: [re.compile(p) for p in ps] for g, ps in GROUPS.items()}
    rows = []
    for f in filter(None, files):
        p = repo / f
        if p.suffix.lower() not in EXTS and p.name != "CMakeLists.txt":
            continue
        text = p.read_text(encoding="utf-8", errors="replace")
        lines = text.count("\n") + 1
        hits = {g: sum(len(r.findall(text)) for r in rs) for g, rs in compiled.items()}
        cls = "game" if hits["game"] else "fox" if hits["fox"] else "none"
        rows.append([f, lines, hits["game"], hits["fox"], cls])
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["path", "lines", "game_hits", "fox_hits", "class"])
        w.writerows(rows)
    assert all(r[4] in ("game", "fox", "none") for r in rows)
    by = {}
    for path, lines, _, _, cls in rows:
        top = "/".join(path.split("/")[:3]) if path.startswith("src/engine") else "/".join(path.split("/")[:2])
        d = by.setdefault(top, {"game": [0, 0], "fox": [0, 0], "none": [0, 0]})
        d[cls][0] += 1
        d[cls][1] += lines
    print(f"{'folder':32} {'game f/lines':>16} {'fox f/lines':>16} {'none f/lines':>16}")
    for top, d in sorted(by.items()):
        print(f"{top:32} " + " ".join(f"{d[c][0]:>5}/{d[c][1]:>9}" for c in ("game", "fox", "none")))


if __name__ == "__main__":
    main()
