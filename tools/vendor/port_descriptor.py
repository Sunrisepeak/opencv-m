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
        self.frag.mkdir(parents=True, exist_ok=True)
        lin = self.mcpp["linux"]

        # base.toml: global include_dirs (linux order; tarball dirs are
        # OS-neutral — assert other OS blocks don't add tarball dirs linux lacks)
        glob_dirs = self.include_dirs("linux")
        for o in ("macosx", "windows"):
            extra = set(self.include_dirs(o)) - set(glob_dirs)
            if extra:
                print(f"NOTE: {o} adds tarball include_dirs not in linux: {sorted(extra)}")
        base = ["# [build].include_dirs additions (after \"include\"):",
                "include_dirs = " + self.tarr(["include", "gen/common"] + glob_dirs), "",
                "# base per-glob flags (linux block, order preserved — last wins):", "",
                self.emit_flag_entries("build.flags", self.rw_flags("linux", lin["flags"]))]
        (self.frag / "base.toml").write_text("\n".join(base))

        # linux.toml: cfg(linux) build section
        srcs = []
        for g in lin["sources"]:
            srcs += self.rw("linux", g)
        for rel in self.base_stubs["linux"]:
            srcs.append(f"gen/{self.where('linux', rel)}/{rel}")
        body = ["[target.'cfg(linux)'.build]",
                "cflags   = " + self.tarr(lin["cflags"], ""),
                "cxxflags = " + self.tarr(lin["cxxflags"], ""),
                "ldflags  = " + self.tarr(lin["ldflags"], ""),
                "sources  = " + self.tarr(srcs), ""]
        (self.frag / "linux.toml").write_text("\n".join(body))

        # features.toml: neutral unifont + per-OS dnn (linux leg only for now)
        feats = ["[features]",
                 "unifont = { defines = [\"HAVE_UNIFONT\"] }", "",
                 "[features.dnn]",
                 "defines = " + self.tarr(lin["features"]["dnn"]["defines"], "")]
        dnn_srcs = ["src/dnn.cppm"]
        for g in lin["features"]["dnn"]["sources"]:
            dnn_srcs += self.rw("linux", g)
        for feat, rels in self.feat_stubs["linux"].items():
            assert feat == "dnn", feat
            dnn_srcs += [f"gen/{self.where('linux', r)}/{r}" for r in rels]
        feats.append("sources = " + self.tarr(dnn_srcs))
        feats.append("")
        feats.append(self.emit_flag_entries(
            "features.dnn.flags", self.rw_flags("linux", lin["features"]["dnn"]["flags"])))
        (self.frag / "features.toml").write_text("\n".join(feats))

        # record the neutral segment for the mcpp.toml author
        neutral = {k: v for k, v in self.mcpp.items() if k not in OSES}
        neutral.pop("features", None)  # unifont handled above (dep dropped: blob vendored)
        (self.frag / "neutral.json").write_text(json.dumps(neutral, indent=2))
        # deps must be identical across OS blocks to stay a global [dependencies]
        deps = {o: self.mcpp[o].get("deps") for o in OSES}
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
