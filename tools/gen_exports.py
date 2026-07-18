#!/usr/bin/env python3
"""gen_exports.py — generate `export using cv::name;` lists from OpenCV headers.

Two inputs merge into src/gen_exports/<module>.inc:

1. GENERATED: OpenCV's own modules/python/src2/hdr_parser.py enumerates the
   CV_EXPORTS_W (python-wrapped) surface per public header — top-level
   functions, classes, enums and their enumerators, constants.
2. CURATED:   tools/curated/<module>.txt — names the wrapper generator cannot
   see (template typedef families Point2i/Vec3b/Matx33f, InputArray family,
   Ptr, traits, …). One name per line, `cv::`-relative, `#` comments.

Names are deduped across modules (first module wins, module order below), so
each entity is exported by exactly one home module. Sub-namespace names
(cv.ocl.X, cv.utils.X, …) group into nested namespace blocks; cv.detail and
private surfaces are dropped.

Usage: gen_exports.py [opencv-root]      (default: pinned official tarball
                                          via tools/fetch_upstream.sh)
"""
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MODULE_ORDER = ["core", "imgproc", "imgcodecs", "videoio", "highgui", "flann", "geometry"]
SKIP_HEADER_PARTS = ("detail", "legacy", "private", "cuda", "opencl", "hal",
                     "utils", "llapi", "parallel", "detection_based_tracker")
SKIP_NS = ("detail", "internal", "traits", "hal", "instr", "utils", "samples",
           "va_intel", "directx", "gapi")


def opencv_root() -> Path:
    if len(sys.argv) > 1:
        return Path(sys.argv[1]).resolve()
    fetch = REPO / "tools" / "fetch_upstream.sh"
    return Path(subprocess.check_output(["sh", str(fetch)], text=True).strip())


def module_headers(root: Path, mod: str):
    inc = root / "modules" / mod / "include" / "opencv2"
    for h in sorted(inc.rglob("*.hpp")):
        rel = h.relative_to(inc)
        if any(part in SKIP_HEADER_PARTS for part in rel.parts):
            continue
        if h.name.endswith(".inl.hpp") or h.name.endswith(".details.hpp"):
            continue
        yield h


def main() -> None:
    root = opencv_root()
    sys.path.insert(0, str(root / "modules" / "python" / "src2"))
    import hdr_parser

    out_dir = REPO / "src" / "gen_exports"
    out_dir.mkdir(parents=True, exist_ok=True)

    claimed: dict[str, str] = {}          # fq name -> owning module
    per_mod: dict[str, dict] = {}

    for mod in MODULE_ORDER:
        names = defaultdict(set)          # sub-namespace ("", "ocl", …) -> names
        skipped: list[str] = []

        def claim(fq: str, ns: str, name: str):
            if fq in claimed:
                return
            claimed[fq] = mod
            names[ns].add(name)

        classes: set[str] = set()         # dotted class paths ("softfloat", "ogl.Buffer")

        def add(dotted: str):
            """dotted: cv.Name / cv.sub.Name / cv.Class.Member (drop members)"""
            if " " in dotted or "operator" in dotted or "<" in dotted:
                skipped.append(f"odd-name: {dotted}")
                return
            parts = dotted.split(".")
            if parts[0] != "cv" or len(parts) < 2:
                skipped.append(f"non-cv: {dotted}")
                return
            mid = parts[1:-1]
            leaf = parts[-1]
            if any(ns in SKIP_NS for ns in mid):
                skipped.append(f"skipped-ns: {dotted}")
                return
            # class member — comes along with its class: any path component
            # that is a known class name or uppercase-initial marks a member
            for i in range(len(mid)):
                if mid[i][0].isupper() or ".".join(mid[: i + 1]) in classes:
                    return
            if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", leaf):
                skipped.append(f"odd-name: {dotted}")
                return
            claim("::".join(["cv"] + mid + [leaf]), "::".join(mid), leaf)

        parser = hdr_parser.CppHeaderParser(
            generate_umat_decls=False, generate_gpumat_decls=False,
            preprocessor_definitions={
                "CV_VERSION_MAJOR": 5, "CV_VERSION_MINOR": 0,
                "OPENCV_ABI_COMPATIBILITY": 500,
            })
        all_decls = []
        for header in module_headers(root, mod):
            try:
                decls = parser.parse(str(header), wmode=False)
            except Exception as e:  # tolerant: log and continue
                skipped.append(f"HEADER PARSE FAIL {header.name}: {e}")
                continue
            all_decls += decls
        # pass 1: collect class paths (incl. lowercase-named ones: softfloat…)
        for d in all_decls:
            kind = d[0].split()[0]
            if kind in ("class", "struct"):
                parts = d[0].split()[1].split(".")
                if parts[0] == "cv" and len(parts) > 1:
                    classes.add(".".join(parts[1:]))
        # pass 2: emit
        for d in all_decls:
            kind = d[0].split()[0]
            if kind in ("class", "struct"):
                dotted = d[0].split()[1]
                parts = dotted.split(".")
                if parts[0] != "cv" or len(parts) < 2:
                    continue
                mid, leaf = parts[1:-1], parts[-1]
                # nested class (any parent path is itself a class) rides along
                # with its outer class — skip
                if any(".".join(parts[1: i + 2]) in classes for i in range(len(mid))):
                    continue
                if not any(ns in SKIP_NS for ns in mid) \
                   and re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", leaf):
                    claim("::".join(["cv"] + mid + [leaf]), "::".join(mid), leaf)
                continue
            elif kind == "enum":
                tag = d[0].split()[1] if len(d[0].split()) > 1 else ""
                if tag and not tag.split(".")[-1].startswith("unnamed"):
                    add(tag)
                for e in d[3]:            # enumerators ride as const entries
                    add(e[0].split()[1])
            elif kind == "const":
                add(d[0].split()[1])
            else:                         # function entry: "cv.name"
                add(d[0])

        curated = REPO / "tools" / "curated" / f"{mod}.txt"
        if curated.exists():
            for line in curated.read_text().splitlines():
                line = line.split("#", 1)[0].strip()
                if not line:
                    continue
                fq = "cv::" + line
                parts = line.split("::")
                claim(fq, "::".join(parts[:-1]), parts[-1])

        # prune list: names that fail compile-verification (internal linkage,
        # header not in the module's GMF, …) — maintained by tools/prune_loop.py
        prune = REPO / "tools" / "curated" / f"{mod}.prune.txt"
        if prune.exists():
            for line in prune.read_text().splitlines():
                line = line.split("#", 1)[0].strip()
                if not line:
                    continue
                parts = line.split("::")
                ns, leaf = "::".join(parts[:-1]), parts[-1]
                if leaf in names[ns]:
                    names[ns].discard(leaf)
                    skipped.append(f"pruned (compile-verify): cv::{line}")

        per_mod[mod] = {"names": names, "skipped": skipped}

    for mod in MODULE_ORDER:
        names, skipped = per_mod[mod]["names"], per_mod[mod]["skipped"]
        lines = [f"// Generated by tools/gen_exports.py — export surface of opencv.{mod}.",
                 "// Regenerate: python3 tools/gen_exports.py   (do not edit by hand)"]
        total = 0
        for ns in sorted(names):
            fqns = ("cv::" + ns) if ns else "cv"
            lines.append(f"export namespace {fqns} {{")
            for n in sorted(names[ns]):
                lines.append(f"    using {fqns}::{n};")
                total += 1
            lines.append("}")
        (out_dir / f"{mod}.inc").write_text("\n".join(lines) + "\n")
        (out_dir / f"{mod}.skipped.txt").write_text("\n".join(sorted(set(skipped))) + "\n")
        print(f"{mod:10} {total:4} names   ({len(skipped)} skipped notes)")


if __name__ == "__main__":
    main()
