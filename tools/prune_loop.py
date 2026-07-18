#!/usr/bin/env python3
"""prune_loop.py — compile-verify the export surface and prune failing names.

Runs `mcpp build`, parses gcc errors that point into src/gen_exports/*.inc,
appends the offending names to tools/curated/<module>.prune.txt (with the
reason as a comment), regenerates, and repeats until the build is green or an
iteration makes no progress (then it prints the残 errors and exits 1).

Usage: prune_loop.py [opencv-root]   (root forwarded to gen_exports.py)
"""
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
INC_ERR = re.compile(r"gen_exports/(\w+)\.inc:(\d+):(?:\d+:)? error: (.*)")

for iteration in range(1, 30):
    r = subprocess.run(["mcpp", "build"], cwd=REPO, capture_output=True, text=True)
    if r.returncode == 0:
        print(f"iteration {iteration}: build GREEN")
        sys.exit(0)
    errs = defaultdict(dict)   # module -> {lineno: reason}
    for line in (r.stderr + r.stdout).splitlines():
        m = INC_ERR.search(line)
        if m:
            errs[m.group(1)].setdefault(int(m.group(2)), m.group(3).strip())
    if not errs:
        print(f"iteration {iteration}: build failed but no .inc errors — manual attention needed")
        print("\n".join((r.stderr + r.stdout).splitlines()[-40:]))
        sys.exit(1)
    total = 0
    for mod, lines in errs.items():
        inc = (REPO / "src" / "gen_exports" / f"{mod}.inc").read_text().splitlines()
        prune_path = REPO / "tools" / "curated" / f"{mod}.prune.txt"
        old = prune_path.read_text() if prune_path.exists() else \
            "# names failing compile-verification — maintained by tools/prune_loop.py\n"
        add = []
        for ln, reason in sorted(lines.items()):
            src = inc[ln - 1].strip()
            m = re.match(r"using (?:cv::)?([A-Za-z0-9_:]+?);", src)
            if not m:
                continue
            name = m.group(1)
            if re.search(rf"^{re.escape(name.split('::')[-1])}\b", old, re.M) and name in old:
                continue
            add.append(f"{name}   # {reason[:90]}")
        if add:
            prune_path.write_text(old + "\n".join(add) + "\n")
            total += len(add)
            print(f"iteration {iteration}: {mod}: pruned {len(add)}")
    if total == 0:
        print(f"iteration {iteration}: no new prunes derivable — manual attention needed")
        print("\n".join((r.stderr + r.stdout).splitlines()[-40:]))
        sys.exit(1)
    subprocess.run([sys.executable, str(REPO / "tools" / "gen_exports.py"), *sys.argv[1:]],
                   check=True, cwd=REPO)
print("iteration limit reached")
sys.exit(1)
