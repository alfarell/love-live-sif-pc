"""Fail if files about to be committed look like game data, executables or key material (CLAUDE.md rule 7).

Checks the git index (staged files) by default, or every tracked + untracked-not-ignored file with --all.
Flags: known game/console magic bytes, SELF/ELF/PKG headers, protected file names, and binaries over a size limit.
Usage: py -3.14 -I no_assets_check.py <project_dir> [--all] [--max-mb 2]
Exit code 0 = clean, 1 = suspicious files found (listed).
"""
import subprocess
import sys
from pathlib import Path

MAGICS = {
    b"ARC\0": "SIF ARC archive", b"cmp\0": "SIF cmp archive", b"AFS2": "CRI AWB audio", b"CRID": "CRI USM video",
    b"@UTF": "CRI table (ACB/CPK)", b"HCA\0": "CRI HCA audio", b"\x4f\x15\x3d\x1d": "PS4 SELF executable",
    b"\x7fELF": "ELF executable", b"\x7fCNT": "PS4 PKG", b"\0PSF": "param.sfo", b"PSAR": "PSARC archive",
    b"Ig\x0a\xed": "Iggy UI file", b"RIF\0": "license file", b"keys": "keystone", b"btx\0": "SIF texture",
}
NAMES = {"eboot.bin", "keystone", "license.dat", "license.info", "param.sfo", "npbind.dat", "nptitle.dat"}
TEXT_SUFFIXES = {".py", ".md", ".txt", ".cpp", ".h", ".hpp", ".c", ".cmake", ".json", ".csv", ".glsl", ".vert", ".frag", ".comp",
                 ".yml", ".yaml", ".ini", ".gitignore", ".rc", ".in"}


def git(project: Path, *args: str) -> list[str]:
    out = subprocess.run(["git", "-C", str(project), *args], capture_output=True, check=True).stdout.decode("utf-8")
    return [line for line in out.split("\0") if line]


def check(path: Path, max_bytes: int) -> str | None:
    if path.name.lower() in NAMES:
        return "protected file name"
    if not path.is_file():
        return None
    with open(path, "rb") as f:
        head = f.read(4)
    for magic, what in MAGICS.items():
        if head == magic:
            return what
    if path.suffix.lower() not in TEXT_SUFFIXES and path.stat().st_size > max_bytes:
        return f"binary over {max_bytes // (1 << 20)} MB"
    return None


def main() -> None:
    project = Path(sys.argv[1])
    max_bytes = int(float(sys.argv[sys.argv.index("--max-mb") + 1]) * (1 << 20)) if "--max-mb" in sys.argv else 2 << 20
    if "--all" in sys.argv:
        files = git(project, "ls-files", "-z", "--cached", "--others", "--exclude-standard")
    else:
        files = git(project, "diff", "--cached", "--name-only", "-z")
    bad = [(f, why) for f in files if (why := check(project / f, max_bytes))]
    for f, why in bad:
        print(f"SUSPICIOUS {f}: {why}")
    print(f"checked {len(files)} files, {len(bad)} suspicious")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    # self-check: a fake SELF header and a protected name are flagged, a text file is not
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        t = Path(tmp)
        (t / "a.bin").write_bytes(b"\x4f\x15\x3d\x1d" + b"\0" * 12)
        (t / "keystone").write_bytes(b"x")
        (t / "ok.md").write_text("ARC is fine in text")
        assert check(t / "a.bin", 1 << 20) == "PS4 SELF executable"
        assert check(t / "keystone", 1 << 20) == "protected file name"
        assert check(t / "ok.md", 1 << 20) is None
    main()
