"""Run the port's scripted routes (M9): every tests/routes/*.txt through `sif.exe --route`, each in a fresh data folder,
and print a pass/fail table. Exit code 0 = all passed, 1 = a route failed or could not run.

Usage: py -3.14 -I tools/routes.py --exe <sif.exe> --game <your CUSA24620 dump> [--only NAME ...] [--shots] [--out DIR]

  --only NAME   run only these routes (file names without .txt); default all, in name order
  --shots       run with a visible window so `shot` commands save real pictures (a hidden window's are black);
                default hidden. The window never needs focus: keys are injected inside the port
  --out DIR     where each route's data folder (settings.ini, save.txt, sif.log, shots/) goes; default ../work/routes
                (git-ignored). It is emptied per run; the user's own data folder is never used

Route header directives (comment lines the port ignores):
  #! seed NAME          copy tests/routes/seeds/NAME.txt to the data folder as save.txt before the run
  #! args ARG ...       extra sif.exe arguments, e.g. `#! args --lang cn`
  #! data NAME          routes with the same NAME share one data folder, run in name order (e.g. a save kept from
                        one route to the next); the first one starts it empty
Every data folder starts with a settings.ini that names the dump and sets the volume to 0 (routes are silent).
"""
import argparse
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
ROUTES = REPO / "tests" / "routes"


def directives(route: Path) -> dict:
    out = {"seed": None, "args": [], "data": None}
    for line in route.read_text(encoding="utf-8").splitlines():
        m = re.match(r"\s*#!\s*(\w+)\s*(.*)$", line)
        if not m:
            continue
        key, value = m.group(1), m.group(2).strip()
        if key == "args":
            out["args"] += value.split()
        elif key in out:
            out[key] = value
    return out


def route_timeout(route: Path) -> float:
    for line in route.read_text(encoding="utf-8").splitlines():
        m = re.match(r"\s*timeout\s+(\d+(?:\.\d+)?)(ms|s)\b", line)
        if m:
            return float(m.group(1)) / (1000 if m.group(2) == "ms" else 1)
    return 180.0


def seed_settings(data: Path, game: str) -> None:
    data.mkdir(parents=True, exist_ok=True)
    (data / "settings.ini").write_text(
        f'[audio]\nvolume = 0\n\n[game]\nfolder = "{game}"\ndlc_folder = ""\n', encoding="utf-8")


def result_line(log: Path) -> str:
    if not log.exists():
        return "no sif.log"
    lines = [l for l in log.read_text(encoding="utf-8", errors="replace").splitlines() if "route: PASS" in l or "route: FAIL" in l]
    return lines[-1].split("route: ", 1)[1] if lines else "no route result in sif.log"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--exe", required=True)
    ap.add_argument("--game", required=True)
    ap.add_argument("--only", nargs="*", default=[])
    ap.add_argument("--shots", action="store_true")
    ap.add_argument("--out", default=str(REPO.parent / "work" / "routes"))
    args = ap.parse_args()

    exe = Path(args.exe).resolve()
    if not exe.is_file():
        print(f"no such sif.exe: {exe}")
        return 1
    routes = sorted(ROUTES.glob("*.txt"))
    if args.only:
        routes = [r for r in routes if r.stem in args.only]
    if not routes:
        print("no routes to run")
        return 1
    out = Path(args.out).resolve()
    started_shared: set[str] = set()
    rows, failed = [], 0
    for route in routes:
        d = directives(route)
        data = out / (d["data"] or route.stem)
        if not d["data"] or d["data"] not in started_shared:  # a fresh folder per route, or per shared group
            shutil.rmtree(data, ignore_errors=True)
            seed_settings(data, args.game)
            if d["data"]:
                started_shared.add(d["data"])
        if d["seed"]:
            shutil.copyfile(ROUTES / "seeds" / f"{d['seed']}.txt", data / "save.txt")
        (data / "sif.log").unlink(missing_ok=True)
        cmd = [str(exe), "--data", str(data), "--route", str(route), "--shots", str(data / "shots" / route.stem)]
        if not args.shots:
            cmd.append("--hidden")
        cmd += d["args"]
        t0 = time.monotonic()
        try:
            code = subprocess.run(cmd, timeout=route_timeout(route) + 60, capture_output=True).returncode
        except subprocess.TimeoutExpired:
            code = None
        seconds = time.monotonic() - t0
        result = result_line(data / "sif.log")
        ok = code == 0 and result.startswith("PASS")
        failed += not ok
        why = "" if ok else ("killed after its timeout" if code is None else f"exit {code}: {result}")
        rows.append((route.stem, "PASS" if ok else "FAIL", f"{seconds:5.1f} s", why))
        print(f"{route.stem:<20} {'PASS' if ok else 'FAIL'}  {seconds:5.1f} s  {why}", flush=True)
    print(f"\n{len(rows) - failed} of {len(rows)} routes passed; logs and shots in {out}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
