"""Cross-reference Thinker's named game-function addresses against the OpenSMACX patcher.

Usage: python xref_thinker.py <path-to-thinker-src> [<dumpbin exports.txt>]
Writes docs/candidates-thinker-xref.md and .tsv in this repo.
"""
import re, sys, os, collections, bisect

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
THINKER_SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, "..", "thinker", "src")
PATCHER = os.path.join(REPO, "tools", "opensmac_patcher.py")
EXPORTS = sys.argv[2] if len(sys.argv) > 2 else os.path.join(REPO, "build", "exports.txt")
THINKER_CPP = os.path.join(THINKER_SRC, "engine.cpp")
THINKER_H = os.path.join(THINKER_SRC, "engine.h")
OUT_MD = os.path.join(REPO, "docs", "candidates-thinker-xref.md")
OUT_TSV = os.path.join(REPO, "docs", "candidates-thinker-xref.tsv")
TEXT_LO, TEXT_HI = 0x401000, 0x401000 + 0x2629C5

# ---------- patcher: redirected functions ----------
redir = []  # (va, mangled, area)
area = "?"
for line in open(PATCHER, encoding="utf-8", errors="replace"):
    m = re.match(r"\s*# ([A-Z][A-Z _/&-]+)$", line)
    if m:
        area = m.group(1).strip(); continue
    m = re.search(r"bin_app\.seek\(0x([0-9A-Fa-f]+)\)\s*#\s*(\S+)", line)
    if m:
        redir.append((int(m.group(1), 16) + 0x400000, m.group(2), area))
redir.sort()
redir_vas = [r[0] for r in redir]

def demangle_simple(mangled):
    """Return (Class, name) from an MSVC mangled name, roughly."""
    m = re.match(r"\?\?0(\w+)@@", mangled)
    if m: return m.group(1), m.group(1)            # ctor
    m = re.match(r"\?\?1(\w+)@@", mangled)
    if m: return m.group(1), "~" + m.group(1)      # dtor
    m = re.match(r"\?(\w+)@(\w+)@@", mangled)
    if m: return m.group(2), m.group(1)
    m = re.match(r"\?(\w+)@@", mangled)
    if m: return "", m.group(1)
    return "", mangled

def norm(cls, name):
    return (cls + "_" + name if cls else name).lower()

redir_norm = {norm(*demangle_simple(mg)): va for va, mg, _ in redir}

# ---------- DLL exports (decompiled, possibly not redirected) ----------
export_norm = set()
if not os.path.exists(EXPORTS):
    print("warning: no exports file at", EXPORTS, "- name-match section will be empty")
for line in (open(EXPORTS, encoding="utf-8", errors="replace") if os.path.exists(EXPORTS) else []):
    m = re.match(r"\s*\d+\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(\S+)", line)
    if m:
        export_norm.add(norm(*demangle_simple(m.group(1))))

# ---------- OpenSMACX temp.cpp: pointers into not-yet-decompiled exe functions ----------
TEMP_CPP = os.path.join(REPO, "src", "temp.cpp")
osx_ref = {}  # va -> (name, is_OG)
for line in (open(TEMP_CPP, encoding="utf-8", errors="replace") if os.path.exists(TEMP_CPP) else []):
    m = re.match(r"\s*\w+\s*\*\s*(\w+)\s*=\s*\(\w+\s*\*\)\s*0x([0-9A-Fa-f]{8})\s*;", line)
    if not m: continue
    va = int(m.group(2), 16)
    if TEXT_LO <= va < TEXT_HI:
        name = m.group(1)
        osx_ref[va] = (name[:-3] if name.endswith("_OG") else name, name.endswith("_OG"))

# ---------- Thinker: named function pointers + typedefs ----------
typedefs = {}
for path in (THINKER_H, THINKER_CPP):
    for line in open(path, encoding="utf-8", errors="replace"):
        m = re.match(r"\s*typedef\s+(.+?)\(\s*(?:__cdecl|__thiscall|__stdcall|__fastcall)?\s*\*\s*(\w+)\s*\)\s*\((.*?)\)\s*;", line)
        if m:
            typedefs[m.group(2)] = (m.group(1).strip() + " (" + m.group(3).strip() + ")", line)
        else:
            m = re.match(r"\s*typedef\s+(.+?)\s*\(\s*\*\s*(\w+)\s*\)\s*\((.*?)\)\s*;", line)
            if m: typedefs[m.group(2)] = (m.group(1).strip() + " (" + m.group(3).strip() + ")", line)

thinker = {}  # va -> (name, typedef)
for line in open(THINKER_CPP, encoding="utf-8", errors="replace"):
    m = re.match(r"\s*(\w+)\s+(\w+)\s*=\s*\((\w+)\)\s*0x([0-9A-Fa-f]{6})\s*;", line)
    if not m: continue
    tname, name, cast, va = m.group(1), m.group(2), m.group(3), int(m.group(4), 16)
    if not (TEXT_LO <= va < TEXT_HI): continue
    if va not in thinker or len(name) > len(thinker[va][0]):
        thinker[va] = (name, cast)

# ---------- cross-reference ----------
redir_set = set(redir_vas)
all_known = sorted(set(redir_vas) | set(thinker))

cands = []
for va, (name, cast) in sorted(thinker.items()):
    if va in redir_set: continue
    i = bisect.bisect_left(redir_vas, va)
    before = redir[i-1] if i > 0 else None
    after = redir[i] if i < len(redir) else None
    d_before = va - before[0] if before else None
    d_after = after[0] - va if after else None
    if before and after:
        nearest = before if d_before <= d_after else after
    else:
        nearest = before or after
    j = bisect.bisect_right(all_known, va)
    size_est = (all_known[j] - va) if j < len(all_known) else None
    n = name.lower()
    decompiled = n in export_norm
    cands.append(dict(va=va, name=name, cast=cast, sig=typedefs.get(cast, ("?", ""))[0],
                      area=nearest[2], before=before, after=after, d_before=d_before,
                      d_after=d_after, size_est=size_est, decompiled=decompiled,
                      osx=osx_ref.get(va)))

by_area = collections.defaultdict(list)
for c in cands: by_area[c["area"]].append(c)

# "first batch": sandwiched between two redirected functions within 4 KB on each side,
# estimated size <= 0x200 bytes, and not already present in the DLL by name.
def batch_score(c):
    if c["d_before"] is None or c["d_after"] is None: return None
    if c["decompiled"]: return None
    if max(c["d_before"], c["d_after"]) > 0x1000: return None
    if c["size_est"] is None or c["size_est"] > 0x200: return None
    return c["size_est"] + (c["d_before"] + c["d_after"]) / 64.0

batch = sorted((c for c in cands if batch_score(c) is not None), key=batch_score)

WRAPPER_RE = re.compile(r"^(X_pop|X_pops|pops?_|pop_ask|popp|NetMsg_pop|Popup_start)")
def family(name):
    if WRAPPER_RE.match(name): return "popup wrappers"
    m = re.match(r"^([A-Za-z]+)_", name)
    return (m.group(1) + "_*") if m else name
fams = collections.OrderedDict()
for c in batch:
    fams.setdefault(family(c["name"]), []).append(c)

# ---------- output ----------
def dm(r): return "%s (0x%06X)" % (norm(*demangle_simple(r[1])), r[0]) if r else "-"

with open(OUT_TSV, "w", encoding="utf-8") as f:
    f.write("va\tthinker_name\tsignature\tarea\tprev_redirected\tdist_prev\tnext_redirected\tdist_next\tsize_est\tin_dll_exports\topensmacx_temp_ptr\n")
    for c in cands:
        f.write("0x%06X\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" % (
            c["va"], c["name"], c["sig"], c["area"], dm(c["before"]), c["d_before"], dm(c["after"]), c["d_after"],
            c["size_est"], "yes" if c["decompiled"] else "", (c["osx"][0] + ("_OG" if c["osx"][1] else "")) if c["osx"] else ""))

with open(OUT_MD, "w", encoding="utf-8") as f:
    f.write("# Unredirected functions named by Thinker, cross-referenced against the OpenSMACX patcher\n\n")
    f.write("Generated 2026-10-14 from `tools/opensmac_patcher.py` (v0.2.3, %d redirected functions) and Thinker v5.5 `src/engine.cpp` (%d named function pointers in .text).\n\n" % (len(redir), len(thinker)))
    f.write("- Thinker-named functions already redirected by the patcher: **%d**\n" % (len(thinker) - len(cands)))
    f.write("- Thinker-named functions **not** redirected (candidates below): **%d**\n" % len(cands))
    f.write("- Of those, a name match exists in the DLL's export table (decompiled but not patched in): **%d**\n\n" % sum(c["decompiled"] for c in cands))
    f.write("`area` = the patcher section of the nearest redirected neighbour, so it is a proximity hint, not a classification. "
            "`size_est` = bytes to the next known function start (Thinker or patcher), an upper bound on the function's size. "
            "Full table in `candidates-thinker-xref.tsv`.\n\n")

    f.write("## First-batch pool (%d functions), grouped by family\n\n" % len(batch))
    f.write("Pool = sandwiched between two redirected functions within 4 KB each side, estimated size <= 512 bytes, no name match in the DLL. "
            "Families are by Thinker name prefix. Popup wrappers (`X_pop*`, `pops_*`, `pop_ask*`) are listed last: each is a ~48-byte thunk around the same core, cheap but low value.\n\n")
    order = sorted(fams, key=lambda k: (k == "popup wrappers", -len(fams[k])))
    for k in order:
        cs = sorted(fams[k], key=lambda c: c["va"])
        f.write("### %s (%d, %d bytes est., 0x%06X-0x%06X)\n\n" % (k, len(cs), sum(c["size_est"] for c in cs), cs[0]["va"], cs[-1]["va"]))
        f.write("| VA | Thinker name | Thinker signature | est. size | prev redirected (dist) | next redirected (dist) |\n|---|---|---|---|---|---|\n")
        for c in cs:
            f.write("| 0x%06X | `%s` | `%s` | %d | %s (%d) | %s (%d) |\n" % (
                c["va"], c["name"], c["sig"], c["size_est"], dm(c["before"]), c["d_before"], dm(c["after"]), c["d_after"]))
        f.write("\n")

    osx_cands = [c for c in cands if c["osx"] and not c["osx"][1]]
    f.write("\n## Already referenced by OpenSMACX through `src/temp.cpp` pointers, not decompiled (%d)\n\n" % len(osx_cands))
    f.write("OpenSMACX code already calls these exe functions through typed pointers, so decompiling them removes a dependency on the original binary. Both names are given where they differ.\n\n")
    f.write("| VA | OpenSMACX name | Thinker name | Thinker signature | est. size | area |\n|---|---|---|---|---|---|\n")
    for c in sorted(osx_cands, key=lambda c: c["va"]):
        f.write("| 0x%06X | `%s` | `%s` | `%s` | %s | %s |\n" % (c["va"], c["osx"][0], c["name"], c["sig"], c["size_est"], c["area"]))
    unnamed = [(va, n) for va, (n, og) in sorted(osx_ref.items()) if va not in thinker and not og]
    f.write("\nTemp pointers at .text addresses Thinker does not name: %s\n" % (", ".join("`%s` (0x%06X)" % (n, va) for va, n in unnamed) or "none"))

    f.write("\n## Name matches: in DLL exports but not redirected (%d)\n\n" % sum(c["decompiled"] for c in cands))
    f.write("Thinker names these addresses with a name that also appears (as `Class_name` / `name`) in the DLL export table. OpenSMACX has decompiled these (they are in the DLL) but the patcher does not yet redirect the exe to them, so the exe still runs the original code. Marked where `src/temp.cpp` keeps an `_OG` pointer to the original for comparison testing. Do not re-decompile these; the open question is only why they are not yet redirected.\n\n")
    f.write("| VA | Thinker name | area | note |\n|---|---|---|---|\n")
    for c in cands:
        if c["decompiled"]:
            f.write("| 0x%06X | `%s` | %s | %s |\n" % (c["va"], c["name"], c["area"], "`_OG` pointer kept" if (c["osx"] and c["osx"][1]) else ""))

    f.write("\n## All candidates by nearest patcher area\n\n")
    for a in sorted(by_area, key=lambda a: min(c["va"] for c in by_area[a])):
        cs = by_area[a]
        f.write("### %s (%d candidates)\n\n" % (a, len(cs)))
        f.write("| VA | Thinker name | signature | est. size | dist prev | dist next |\n|---|---|---|---|---|---|\n")
        for c in cs:
            f.write("| 0x%06X | `%s` | `%s` | %s | %s | %s |\n" % (
                c["va"], c["name"], c["sig"], c["size_est"], c["d_before"], c["d_after"]))
        f.write("\n")

print("redirected:", len(redir), "thinker named:", len(thinker), "candidates:", len(cands),
      "decompiled-unredirected:", sum(c["decompiled"] for c in cands), "first batch:", len(batch))
print("areas:", {a: len(v) for a, v in sorted(by_area.items())})
