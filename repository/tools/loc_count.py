"""Count files and lines per language and per folder in a git repository.

Read-only: lists tracked files with `git --no-optional-locks ls-files` and reads them.
Usage: py -3 loc_count.py <repo_dir> <out.json> [--depth N]
Lines = physical lines (blank and comment lines included); binary files are skipped.
"""
import json
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

LANG = {
    ".cpp": "C++", ".cc": "C++", ".cxx": "C++", ".h": "C/C++ header", ".hpp": "C/C++ header",
    ".inl": "C/C++ header", ".c": "C", ".mm": "Objective-C++", ".m": "Objective-C",
    ".py": "Python", ".cs": "C#", ".lua": "Lua", ".sh": "Shell", ".bat": "Batch", ".cmd": "Batch",
    ".ps1": "PowerShell", ".cmake": "CMake", ".glsl": "GLSL", ".frag": "GLSL", ".vert": "GLSL",
    ".comp": "GLSL", ".hlsl": "HLSL", ".md": "Markdown", ".txt": "Text", ".json": "JSON",
    ".yml": "YAML", ".yaml": "YAML", ".rc": "Resource script", ".in": "Template",
    ".csproj": "MSBuild", ".manifest": "XML", ".map": "Linker map",
}


def lang_of(path: str) -> str:
    name = Path(path).name
    if name == "CMakeLists.txt":
        return "CMake"
    suffixes = Path(path).suffixes
    if suffixes and suffixes[-1] == ".in" and len(suffixes) > 1:
        return LANG.get(suffixes[-2], "Template")
    return LANG.get(Path(path).suffix.lower(), "other (" + (Path(path).suffix or name) + ")")


def main() -> None:
    repo, out = Path(sys.argv[1]), Path(sys.argv[2])
    depth = int(sys.argv[sys.argv.index("--depth") + 1]) if "--depth" in sys.argv else 2
    files = subprocess.run(["git", "--no-optional-locks", "-C", str(repo), "ls-files", "-z"],
                           capture_output=True, check=True).stdout.decode("utf-8").split("\0")
    by_lang = defaultdict(lambda: [0, 0])
    by_dir = defaultdict(lambda: defaultdict(lambda: [0, 0]))
    binary = []
    for f in filter(None, files):
        data = (repo / f).read_bytes()
        if b"\0" in data[:8192]:
            binary.append(f)
            continue
        lines = data.count(b"\n") + (1 if data and not data.endswith(b"\n") else 0)
        lang = lang_of(f)
        parts = f.split("/")
        folder = "/".join(parts[:min(depth, len(parts) - 1)]) or "(root)"
        by_lang[lang][0] += 1
        by_lang[lang][1] += lines
        by_dir[folder][lang][0] += 1
        by_dir[folder][lang][1] += lines
    result = {
        "repo": str(repo),
        "note": "physical lines of tracked text files; binary files listed separately",
        "by_language": {k: {"files": v[0], "lines": v[1]} for k, v in sorted(by_lang.items(), key=lambda kv: -kv[1][1])},
        "by_folder": {d: {"files": sum(v[0] for v in langs.values()), "lines": sum(v[1] for v in langs.values()),
                          "languages": {k: {"files": v[0], "lines": v[1]} for k, v in langs.items()}}
                      for d, langs in sorted(by_dir.items())},
        "binary_files": binary,
    }
    assert sum(v["files"] for v in result["by_language"].values()) + len(binary) == len(list(filter(None, files)))
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2), encoding="utf-8")
    for k, v in result["by_language"].items():
        print(f"{v['lines']:>9} {v['files']:>5}  {k}")
    print("---")
    for d, v in result["by_folder"].items():
        print(f"{v['lines']:>9} {v['files']:>5}  {d}")


if __name__ == "__main__":
    main()
