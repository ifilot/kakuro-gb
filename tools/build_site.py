#!/usr/bin/env python3
"""Builds the website -> _site/: the page (web/), the emulator
(web/vendor/binjgb), the ROM, the README's screenshots, the logo in the
Color's palette, the licenses and provenance.json. With --check it also
verifies that every file the page refers to is there and that the emulator
is the one web/vendor/binjgb/SOURCE.md names.

  python3 tools/build_site.py [--check]
"""
from __future__ import annotations

import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import gen_gfx  # noqa: E402

WEB = ROOT / "web"
SITE = ROOT / "_site"
ROM = ROOT / "build/kakuro.gb"
VENDOR = WEB / "vendor/binjgb"
SCREENSHOTS = ["start", "title", "playing", "combinations", "solved", "color_keypad"]
LOGO_COLOURS = [0xF8F0DC, 0xF0B070, 0xD8643C, 0x1E2440]    # video.c: CPAL_CLUE
LOGO_SCALE = 4


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def version() -> str:
    return re.search(r'#define VERSION "([^"]+)"', (ROOT / "src/version.h").read_text()).group(1)


def logo(path: Path) -> tuple[int, int]:
    """The title screens' logo (its cells, with a few dots of the band round
    them) as a PNG; returns its size."""
    img = gen_gfx.logo_image()
    rows = [y for y, row in enumerate(img) if any(v != gen_gfx.B for v in row)]
    cols = [x for x in range(160) if any(img[y][x] != gen_gfx.B for y in rows)]
    y0, y1, x0, x1 = rows[0] - 3, rows[-1] + 4, cols[0] - 3, cols[-1] + 4
    out = Image.new("RGB", (x1 - x0, y1 - y0))
    out.putdata([(c >> 16, (c >> 8) & 255, c & 255)
                 for y in range(y0, y1) for c in (LOGO_COLOURS[img[y][x]] for x in range(x0, x1))])
    out = out.resize((out.width * LOGO_SCALE, out.height * LOGO_SCALE), Image.NEAREST)
    out.save(path)
    return out.size


def git_commit() -> str | None:
    try:
        return subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True,
                              check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def check(page: str) -> list[str]:
    problems = []
    for ref in re.findall(r'(?:src|href)="([^"#:]+)"', page):
        target = SITE / ref
        if not (target / "index.html" if ref.endswith("/") else target).is_file():
            problems.append(f"index.html refers to {ref}, not in _site")
    source = (VENDOR / "SOURCE.md").read_text()
    for name in ("binjgb.js", "binjgb.wasm"):
        want = re.search(rf"`{re.escape(name)}` \| `([0-9a-f]{{64}})`", source)
        if not want or want.group(1) != sha256(VENDOR / name):
            problems.append(f"{name} does not match its SHA-256 in SOURCE.md")
    for code in ("app.js",):
        if subprocess.run(["node", "--check", str(SITE / code)], capture_output=True).returncode:
            problems.append(f"{code}: node --check failed")
    return problems


def main() -> int:
    if not ROM.is_file():
        print(f"{ROM.relative_to(ROOT)} is missing: run make first")
        return 1
    if SITE.exists():
        shutil.rmtree(SITE)
    (SITE / "vendor/binjgb").mkdir(parents=True)
    (SITE / "screenshots").mkdir()
    (SITE / "licenses").mkdir()

    for name in ("style.css", "app.js", "favicon.svg"):
        shutil.copy(WEB / name, SITE / name)
    for name in ("binjgb.js", "binjgb.wasm"):
        shutil.copy(VENDOR / name, SITE / "vendor/binjgb" / name)
    shutil.copy(ROM, SITE / "kakuro.gb")
    for name in SCREENSHOTS:
        shutil.copy(ROOT / "docs" / f"{name}.png", SITE / "screenshots" / f"{name}.png")
    shutil.copy(ROOT / "LICENSE", SITE / "licenses/game.txt")
    shutil.copy(VENDOR / "LICENSE", SITE / "licenses/binjgb.txt")
    w, h = logo(SITE / "logo.png")

    page = (WEB / "index.html").read_text()
    page = page.replace("__VERSION__", version()).replace("__LOGO_W__", str(w)).replace("__LOGO_H__", str(h))
    (SITE / "index.html").write_text(page)
    (SITE / ".nojekyll").write_text("")

    source = (VENDOR / "SOURCE.md").read_text()
    provenance = {
        "game": {"version": version(), "commit": git_commit(), "rom_sha256": sha256(ROM)},
        "emulator": {
            "name": "binjgb", "url": "https://github.com/binji/binjgb", "license": "MIT",
            "commit": re.search(r"commit\s+`([0-9a-f]{40})`", source).group(1),
            "sha256": {name: sha256(VENDOR / name) for name in ("binjgb.js", "binjgb.wasm")},
        },
    }
    (SITE / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    print(f"wrote {SITE.relative_to(ROOT)}/ (kakuro.gb {provenance['game']['rom_sha256'][:12]}…)")

    if "--check" in sys.argv:
        problems = check(page)
        for p in problems:
            print("  " + p)
        if problems:
            return 1
        print("site checked")
    return 0


if __name__ == "__main__":
    sys.exit(main())
