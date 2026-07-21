#!/usr/bin/env python3
"""One-time port of mcpp-index pkgs/c/compat.opencv.lua into this repo.

Reads the (machine-generated) descriptor via a real lua interpreter
(dump_descriptor.lua -> JSON) and emits:

  gen/common/            files byte-identical across ALL 3 OS blocks
                         (+ synth/ = outputs of the descriptor's embedded
                          build.mcpp: fonts, OpenCL kernel embeddings)
  gen/<os>/              everything else, path = generated_files key minus
                         the mcpp_generated/ prefix; plus tu/<grp>/ stubs
                         materialized from tu_manifest.txt; plus
                         INCLUDE_DIRS.txt consumed by the repo's build.mcpp
  <fragments-dir>/       base.toml / <os>.toml / features.toml — paste
                         material for mcpp.toml (path-rewritten)

Faithfulness rules: no value is invented or 'improved'; every rewrite is a
pure path-prefix mapping:
  */X            -> third_party/opencv-5.0.0/X       (tarball-relative)
  mcpp_generated/X -> gen/common/X or gen/<os>/X     (by the dedupe outcome)
Stub file bytes are kept identical to what the embedded build.mcpp emits
(cross-checked by actually compiling and running it).
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

OSES = ["linux", "macosx", "windows"]
VENDOR = "third_party/opencv-5.0.0"
GEN_PREFIX = "mcpp_generated/"


def sh(cmd, **kw):
    print("+", " ".join(str(c) for c in cmd))
    subprocess.run(cmd, check=True, **kw)


def expand_braces(pat):
    m = re.search(r"\{([^{}]*)\}", pat)
    if not m:
        return [pat]
    out = []
    for alt in m.group(1).split(","):
        out.extend(expand_braces(pat[: m.start()] + alt + pat[m.end():]))
    return out


def mangle(grp, target):
    return grp + "_" + target.replace("/", "_")


def stub_content(grp, target):
    # byte-identical to the embedded build.mcpp's emission
    return f'/* compat.opencv {grp} re-compile TU */\n#include "{target}"\n'


class Port:
    def __init__(self, desc, repo, fragments):
        self.desc = desc
        self.repo = Path(repo).resolve()
        self.frag = Path(fragments)
        self.mcpp = desc["mcpp"]
        self.staged = {o: {} for o in OSES}      # relpath -> content
        self.loc = {}                            # relpath -> "common" | os
        self.build_mcpp_src = None
        self.base_stubs = {o: [] for o in OSES}  # relpaths (staged keys)
        self.feat_stubs = {o: {} for o in OSES}  # feature -> [relpaths]

    # ── stage generated_files + stubs per OS ────────────────────────────
    def stage(self):
        for o in OSES:
            blk = self.mcpp[o]
            gf = blk["generated_files"]
            bm = gf["build.mcpp"]
            if self.build_mcpp_src is None:
                self.build_mcpp_src = bm  # linux copy is canonical
            elif bm != self.build_mcpp_src:
                # known benign: windows/mac snapshots differ in comment
                # mojibake (CP1252 round-trip) + a duplicated #include line
                import difflib
                n = sum(1 for l in difflib.unified_diff(
                    self.build_mcpp_src.splitlines(), bm.splitlines(), lineterm="", n=0)
                    if l.startswith(("+", "-")) and not l.startswith(("+++", "---")))
                print(f"NOTE: embedded build.mcpp for {o} differs from linux "
                      f"({n} changed lines, comments/includes only — using linux copy)")
            for path, content in gf.items():
                if path == "build.mcpp":
                    continue
                if path == GEN_PREFIX + "tu_manifest.txt":
                    self._stage_stubs(o, content)
                    continue
                assert path.startswith(GEN_PREFIX), path
                self.staged[o][path[len(GEN_PREFIX):]] = content

    def _stage_stubs(self, o, manifest):
        for line in manifest.splitlines():
            line = line.strip("\n")
            if not line or line.startswith("#"):
                continue
            feat = None
            if line.startswith("?"):
                feat, line = line[1:].split("\t", 1)
            grp, target = line.split("\t")
            rel = f"tu/{grp}/{mangle(grp, target)}"
            self.staged[o][rel] = stub_content(grp, target)
            if feat:
                self.feat_stubs[o].setdefault(feat, []).append(rel)
            else:
                self.base_stubs[o].append(rel)

    # ── dedupe: identical in ALL 3 blocks -> common ─────────────────────
    def dedupe(self):
        # opencv_modules.hpp is the ONE consumer-visible generated header and
        # must be OS-neutral. Snapshot drift: the macosx/windows snapshots were
        # captured from dnn-enabled reference builds and bake in
        # `#define HAVE_OPENCV_DNN`; the linux snapshot (dnn-off reference) does
        # not — HAVE_OPENCV_DNN correctly arrives via the dnn feature's defines
        # on every OS. Normalize on the linux copy (assert the premise below).
        MODHPP = "opencv2/opencv_modules.hpp"
        for o in OSES:
            assert "HAVE_OPENCV_DNN" in self.mcpp[o]["features"]["dnn"]["defines"], \
                f"{o} dnn feature lacks HAVE_OPENCV_DNN define"
            assert self.staged[o][MODHPP].replace("#define HAVE_OPENCV_DNN\n", "") \
                == self.staged["linux"][MODHPP], f"{o} opencv_modules.hpp drift beyond dnn"
            self.staged[o][MODHPP] = self.staged["linux"][MODHPP]
        allpaths = set()
        for o in OSES:
            allpaths |= self.staged[o].keys()
        for p in sorted(allpaths):
            vals = [self.staged[o].get(p) for o in OSES]
            if all(v is not None for v in vals) and vals[0] == vals[1] == vals[2]:
                self.loc[p] = "common"
            # else: per-OS; loc stays unset (resolved per staged tree)

    def where(self, o, rel):
        return "common" if self.loc.get(rel) == "common" else o

    # ── write gen/ trees ─────────────────────────────────────────────────
    def write_gen(self):
        counts = {"common": 0, **{o: 0 for o in OSES}}
        written_common = set()
        for o in OSES:
            for rel, content in self.staged[o].items():
                loc = self.where(o, rel)
                if loc == "common":
                    if rel in written_common:
                        continue
                    written_common.add(rel)
                out = self.repo / "gen" / loc / rel
                out.parent.mkdir(parents=True, exist_ok=True)
                out.write_text(content)
                counts[loc] += 1
        # the one consumer-visible generated header must be OS-neutral
        assert (self.repo / "gen/common/opencv2/opencv_modules.hpp").exists(), \
            "opencv_modules.hpp did not dedupe into gen/common"
        print("gen files:", counts)

    # ── run the descriptor's own build.mcpp once; harvest + cross-check ──
    def synth(self, scratch):
        scratch = Path(scratch)
        man = scratch / "man"
        out = scratch / "out"
        shutil.rmtree(scratch, ignore_errors=True)
        (man / "mcpp_generated").mkdir(parents=True)
        out.mkdir(parents=True)
        (scratch / "build_mcpp.cpp").write_text(self.build_mcpp_src)
        link = man / "opencv-5.0.0"
        link.symlink_to(self.repo / VENDOR)
        # feed the linux manifest; enable dnn so ?dnn-guarded stubs emit too
        (man / "mcpp_generated/tu_manifest.txt").write_text(
            self.mcpp["linux"]["generated_files"][GEN_PREFIX + "tu_manifest.txt"])
        exe = scratch / "synth"
        # honor $CXX; plain `c++` otherwise. (If your PATH shims c++/g++ to a
        # cross toolchain — e.g. xlings subos — export CXX to a native one.)
        cxx = os.environ.get("CXX", "c++")
        sh([cxx, "-std=c++23", "-O1", "-o", exe, scratch / "build_mcpp.cpp"])
        env = dict(os.environ, MCPP_MANIFEST_DIR=str(man), MCPP_OUT_DIR=str(out),
                   MCPP_FEATURE_DNN="1")
        env.pop("MCPP_FEATURE_UNIFONT", None)
        sh([exe], env=env, stdout=subprocess.DEVNULL)
        # harvest fonts + kernels into gen/common/synth (layout mirrors OUT)
        synth_dir = self.repo / "gen/common/synth"
        shutil.rmtree(synth_dir, ignore_errors=True)
        n = 0
        for f in sorted(out.rglob("*")):
            if not f.is_file():
                continue
            rel = f.relative_to(out)
            if rel.parts[0] == "tu":
                # cross-check stub bytes vs our python materialization
                srel = str(rel)
                want = self.staged["linux"].get(srel)
                assert want is not None, f"synth produced unexpected stub {srel}"
                assert want == f.read_text(), f"stub content mismatch: {srel}"
                continue
            dst = synth_dir / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(f, dst)
            n += 1
        print(f"synth harvested {n} files into gen/common/synth "
              f"(+ {sum(1 for _ in out.rglob('tu/**/*') if _.is_file())} stubs cross-checked)")

    # ── path rewrites ────────────────────────────────────────────────────
    def rw(self, o, g):
        if g.startswith("*/"):
            return [VENDOR + "/" + g[2:]]
        if g.startswith(GEN_PREFIX):
            rel = g[len(GEN_PREFIX):]
            # decide location per expanded member; split if they disagree
            locs = {}
            for e in expand_braces(rel):
                locs.setdefault(self.where(o, e), []).append(e)
            if len(locs) == 1:
                return [f"gen/{loc}/{rel}" for loc in locs]
            return [f"gen/{loc}/{e}" for loc, es in sorted(locs.items()) for e in es]
        return [g]  # '**/...' and plain globs pass through

    def rw_flags(self, o, entries):
        out = []
        for e in entries:
            e2 = dict(e)
            [e2["glob"]] = self.rw(o, e2["glob"])
            out.append(e2)
        return out

    # ── INCLUDE_DIRS.txt (consumed by the repo build.mcpp) ───────────────
    def include_dirs(self, o):
        blk = self.mcpp[o]
        priv, glob_dirs = [], []
        for d in blk["include_dirs"]:
            if d == "mcpp_generated" or d.startswith(GEN_PREFIX):
                rel = "" if d == "mcpp_generated" else d[len(GEN_PREFIX):]
                for loc in (o, "common"):
                    p = self.repo / "gen" / loc / rel if rel else self.repo / "gen" / loc
                    line = f"gen/{loc}/{rel}" if rel else f"gen/{loc}"
                    if p.is_dir() and line not in priv:
                        priv.append(line)
            elif d == "*":
                glob_dirs.append(VENDOR)
            elif d.startswith("*/"):
                glob_dirs.append(VENDOR + "/" + d[2:])
            else:
                glob_dirs.append(d)
        priv.append("gen/common/synth")  # replaces the old `-I $MCPP_OUT_DIR`
        f = self.repo / "gen" / o / "INCLUDE_DIRS.txt"
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_text("\n".join(priv) + "\n")
        return glob_dirs

    # ── TOML fragments ───────────────────────────────────────────────────
    @staticmethod
    def ts(s):
        return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'

    def tarr(self, xs, ind="    "):
        if not xs:
            return "[]"
        inner = ",\n".join(ind + "    " + self.ts(x) for x in xs)
        return "[\n" + inner + ",\n" + ind + "]"

    def emit_flag_entries(self, header, entries):
        out = []
        for e in entries:
            out.append(f"[[{header}]]")
            out.append(f'glob = {self.ts(e["glob"])}')
            for k in ("cflags", "cxxflags", "asmflags", "defines"):
                if e.get(k):
                    out.append(f"{k} = {self.tarr(e[k], '')}")
            out.append("")
        return "\n".join(out)

    def fragments(self):
        """Emit paste-ready TOML fragments for ALL THREE OS legs.

        - base.toml: global include_dirs + the UNION of the per-OS per-glob
          flag tables (same glob + identical payload across the OSes that
          carry it -> one entry; a same-glob conflict aborts with a report).
        - <os>.toml: [target.'cfg(<os>)'.build] cflags/cxxflags/ldflags/
          sources (+ that OS's base tu stubs).
        - features.toml: [features] with the OS-NEUTRAL dnn subset (entries
          whose rewritten path is identical for every OS that has dnn) +
          unified dnn flag table; the per-OS remainder (ISA dispatch TUs,
          mlas, feature-gated stubs) goes to gen/<os>/DNN_SOURCES.txt, which
          build.mcpp injects via mcpp:source= when the feature is active.
        """
        self.frag.mkdir(parents=True, exist_ok=True)
        CFG = {"linux": "linux", "macosx": "macos", "windows": "windows"}

        # global include dirs (also writes gen/<os>/INCLUDE_DIRS.txt)
        glob_dirs = self.include_dirs("linux")
        for o in ("macosx", "windows"):
            extra = set(self.include_dirs(o)) - set(glob_dirs)
            if extra:
                print(f"NOTE: {o} adds tarball include_dirs not in linux: {sorted(extra)}")

        # Per-OS per-glob define deltas that the manifest cannot express are
        # HOISTED to that OS's global cflags/cxxflags: each define name below
        # is read ONLY by the code group it came from (grep-verified against
        # the vendored tree), so package-global scope on that OS is
        # behavior-identical. Windows is NOT hoistable this way (it needs
        # REMOVALS of unix defines on shared globs) — parked on the mcpp
        # per-OS-flags issue.
        HOIST = {
            ("linux",  "**/modules/core/**"): ["OPENCV_ALLOCATOR_STATS_COUNTER_TYPE=long long"],
            ("linux",  "**/3rdparty/libpng/**"): ["PNG_INTEL_SSE_OPT=1"],
            ("linux",  "**/modules/videoio/**"): ["HAVE_CAMV4L2"],
            ("linux",  VENDOR + "/modules/core/src/alloc.cpp"): ["HAVE_MALLOC_H=1", "HAVE_MEMALIGN=1"],
            ("macosx", "**/modules/core/**"): ["OPENCV_ALLOCATOR_STATS_COUNTER_TYPE=int"],
            ("macosx", "**/modules/highgui/**"): ["HAVE_STDARG_H=1", "HAVE_UNISTD_H=1"],
            ("macosx", "**/3rdparty/libjpeg-turbo/**"): ["NEON_INTRINSICS"],
            ("macosx", "**/tu/jpeg12/**"): ["NEON_INTRINSICS"],
            ("macosx", "**/tu/jpeg16/**"): ["NEON_INTRINSICS"],
            ("macosx", "**/3rdparty/libpng/**"): ["PNG_ARM_NEON_OPT=2"],
        }
        DNN_HOIST = {
            ("linux",  "**/modules/dnn/**"): ["HAVE_FLATBUFFERS=1"],
        }
        self.hoisted = {o: [] for o in OSES}

        def apply_hoist(o, entries, table):
            out = []
            for e in entries:
                key = (o, e["glob"])
                if key in table:
                    e = dict(e)
                    defs = list(e.get("defines") or [])
                    for d in table[key]:
                        assert d in defs, (key, d)
                        defs.remove(d)
                        self.hoisted[o].append(d)
                    e["defines"] = defs
                out.append(e)
            return out

        def union_flags(per_os):
            """per_os: {os: [entry,...]} rewritten. Returns ordered union."""
            out, seen = [], {}
            conflicts = []
            for o in OSES:
                for e in per_os.get(o, []):
                    key = e["glob"]
                    body = {k: e.get(k) for k in ("cflags", "cxxflags", "asmflags", "defines")}
                    if key in seen:
                        if seen[key] != body:
                            conflicts.append((key, o, seen[key], body))
                    else:
                        seen[key] = body
                        out.append(e)
            if conflicts:
                for c in conflicts:
                    print("FLAG CONFLICT:", c[0], "in", c[1])
                    print("  first:", c[2])
                    print("  other:", c[3])
                raise SystemExit("cross-OS per-glob flag conflicts — resolve before emitting")
            return out

        # Windows per-glob flags CANNOT be expressed today: the manifest has
        # no [target.'cfg(os)'.build] flags key (descriptor-only #253
        # semantics), and windows needs different defines on the SAME globs
        # (WIN32/_CRT_* adds, unix HAVE_* removals). Union linux+macosx
        # (verified conflict-free); park the windows table in the fragments
        # for when mcpp grows per-OS flags (issue filed).
        base_union = union_flags({o: apply_hoist(o, self.rw_flags(o, self.mcpp[o]["flags"]), HOIST)
                                  for o in ("linux", "macosx")})
        (self.frag / "windows-flags-PARKED.toml").write_text(
            self.emit_flag_entries("build.flags", self.rw_flags("windows", self.mcpp["windows"]["flags"])))
        base = ["# [build].include_dirs additions (after \"include\"):",
                "include_dirs = " + self.tarr(["include", "gen/common"] + glob_dirs), "",
                "# base per-glob flags: union of the three OS tables (disjoint or identical):", "",
                self.emit_flag_entries("build.flags", base_union)]
        (self.frag / "base.toml").write_text("\n".join(base))

        def emit_os_sections():
            for o in OSES:
                blk = self.mcpp[o]
                srcs = []
                for g in blk["sources"]:
                    srcs += self.rw(o, g)
                for rel in self.base_stubs[o]:
                    srcs.append(f"gen/{self.where(o, rel)}/{rel}")
                hoist_flags = []
                for d in self.hoisted[o]:
                    if "-D" + d not in hoist_flags:
                        hoist_flags.append("-D" + d)
                body = [f"[target.'cfg({CFG[o]})'.build]",
                        "cflags   = " + self.tarr(blk["cflags"] + hoist_flags, ""),
                        "cxxflags = " + self.tarr(blk["cxxflags"] + hoist_flags, ""),
                        "ldflags  = " + self.tarr(blk.get("ldflags", []), ""),
                        "sources  = " + self.tarr(srcs), ""]
                (self.frag / f"{o}.toml").write_text("\n".join(body))

        # dnn feature: OS-neutral subset in the manifest, per-OS remainder in
        # gen/<os>/DNN_SOURCES.txt (build.mcpp injects when the feature is on)
        dnn_os = {}
        for o in OSES:
            feats = self.mcpp[o].get("features", {})
            if "dnn" not in feats:
                continue
            lst = []
            for g in feats["dnn"]["sources"]:
                lst += self.rw(o, g)
            for rel in self.feat_stubs[o].get("dnn", []):
                lst.append(f"gen/{self.where(o, rel)}/{rel}")
            dnn_os[o] = lst
            assert feats["dnn"]["defines"] == ["HAVE_OPENCV_DNN"] or \
                "HAVE_OPENCV_DNN" in feats["dnn"]["defines"], o
        common = [e for e in dnn_os[OSES[0]] if all(e in dnn_os[o] for o in dnn_os)]
        for o, lst in dnn_os.items():
            extra = [e for e in lst if e not in common]
            f = self.repo / "gen" / o / "DNN_SOURCES.txt"
            f.write_text("\n".join(extra) + ("\n" if extra else ""))
            print(f"dnn {o}: {len(common)} common + {len(extra)} per-OS -> gen/{o}/DNN_SOURCES.txt")
        for o in OSES:
            if o not in dnn_os:  # OS without dnn: empty list = feature off-limits there
                (self.repo / "gen" / o / "DNN_SOURCES.txt").write_text("")

        dnn_flags = union_flags({o: apply_hoist(o, self.rw_flags(o, self.mcpp[o]["features"]["dnn"]["flags"]), DNN_HOIST)
                                 for o in dnn_os if o != "windows"})
        if "windows" in dnn_os:
            (self.frag / "windows-dnn-flags-PARKED.toml").write_text(
                self.emit_flag_entries("features.dnn.flags",
                                       self.rw_flags("windows", self.mcpp["windows"]["features"]["dnn"]["flags"])))
        feats = ["[features]",
                 "unifont = { defines = [\"HAVE_UNIFONT\"] }", "",
                 "[features.dnn]",
                 "defines = " + self.tarr(["HAVE_OPENCV_DNN"], ""),
                 "# OS-neutral dnn sources; per-OS ISA dispatch/mlas/stubs are injected by",
                 "# build.mcpp from gen/<os>/DNN_SOURCES.txt (manifest [features] cannot be",
                 "# per-OS - descriptor-only #253 semantics).",
                 "sources = " + self.tarr(["src/dnn.cppm"] + common)]
        feats.append("")
        feats.append(self.emit_flag_entries("features.dnn.flags", dnn_flags))
        (self.frag / "features.toml").write_text("\n".join(feats))

        emit_os_sections()

        # record the neutral segment + deps for the mcpp.toml author
        neutral = {k: v for k, v in self.mcpp.items() if k not in OSES}
        neutral.pop("features", None)
        (self.frag / "neutral.json").write_text(json.dumps(neutral, indent=2))
        deps = {o: self.mcpp[o].get("deps") for o in OSES}
        assert len({json.dumps(d, sort_keys=True) for d in deps.values()}) == 1, deps
        (self.frag / "deps.json").write_text(json.dumps(deps, indent=2))
        print("fragments written to", self.frag)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--descriptor", required=True)
    ap.add_argument("--repo", default=".")
    ap.add_argument("--fragments", required=True)
    ap.add_argument("--scratch", required=True)
    args = ap.parse_args()

    here = Path(__file__).parent
    js = subprocess.run(["lua", str(here / "dump_descriptor.lua"), args.descriptor],
                        check=True, capture_output=True, text=True).stdout
    desc = json.loads(js)

    p = Port(desc, args.repo, args.fragments)
    p.stage()
    p.dedupe()
    p.write_gen()
    p.synth(args.scratch)
    p.fragments()
    print("OK")


if __name__ == "__main__":
    main()
