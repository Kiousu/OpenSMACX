#!/usr/bin/env python3
"""Read SMACX .SAV files offline and report the game states the regression tests depend on.

A save is a dump of the game's own structures, so the struct layouts from src/*.h apply; only
the position of each block inside the file had to be found (see docs/regression-testing.md,
"Save sweep"). Verified against the in-game survey on 13 saves.

    py -3 -I tools/savescan.py <save.SAV>...          one report per save
    py -3 -I tools/savescan.py --table <save.SAV>...  one line per save: coverage flags
"""
import struct
import sys
import os

PLAYERS_DATA = 0x6C4          # PlayerData[8], stride 0x20CC (mirrors 0x96C9E0)
PLAYERS = 0x10D24             # Player[8] rules, stride 0x59C (mirrors 0x946A50)
MAP_GLOBALS = 0x13A24         # mirrors 0x949870 (MapLongitudeBounds ...)
TILES = 0x144C8               # Map[W/2 * H], stride 0x2C
GLOBALS_BIAS = 0x9A6168       # file offset = exe address - this, for the 0x9A64xx block
SP_TABLE = 0x9A6514 - GLOBALS_BIAS
VEH_COUNT = 0x9A64C8 - GLOBALS_BIAS
BASE_COUNT = 0x9A64CC - GLOBALS_BIAS
TURN = 0x9A64D4 - GLOBALS_BIAS
FACTIONS_STATUS = 0x9A64E8 - GLOBALS_BIAS
PROTO_TO_VEHS = 0x7DB0        # Vehs start this far after VehPrototypes[0]
VEHS_TO_BASES = 0x2BC         # Bases start this far after the last Veh
TAIL = 0x21E4                 # bytes after the last Base

SP = {"HGP": 0, "VW": 6, "PTS": 7, "LV": 14, "SAC": 25, "CI": 26}
FAC = {"punish": 23, "brood": 35, "rec": 6, "holo": 11, "paradise": 12, "genejack": 22,
       "research_hosp": 17, "nanohosp": 18}
RFLAG_WORMPOLICE = 0x40
PLAN_COMBAT_MAX = 3           # offensive, defensive, combat, reconnaissance


def u32(d, o): return struct.unpack_from("<I", d, o)[0]
def i32(d, o): return struct.unpack_from("<i", d, o)[0]
def i16(d, o): return struct.unpack_from("<h", d, o)[0]
def cstr(d, o, n): return d[o:o + n].split(b"\0", 1)[0].decode("latin-1", "replace")


def has_fac(d, base_off, fac_id):
    bit = fac_id - 1
    return (d[base_off + 0x8C + bit // 8] >> (bit % 8)) & 1


def scan(path):
    d = open(path, "rb").read()
    if d[:6] != b"TERRAN":
        raise ValueError("not a SMACX save")
    w, h = i32(d, MAP_GLOBALS), i32(d, MAP_GLOBALS + 4)
    veh_count, base_count, turn = i32(d, VEH_COUNT), i32(d, BASE_COUNT), i32(d, TURN)
    human_bits = d[FACTIONS_STATUS]
    proto0 = d.find(b"Colony Pod\0")
    if proto0 < 0:
        raise ValueError("prototype table not found")
    vehs = proto0 + PROTO_TO_VEHS
    bases = vehs + veh_count * 0x34 + VEHS_TO_BASES
    if bases + base_count * 0x134 + TAIL != len(d):
        raise ValueError("base block does not end %#x before EOF (got %#x)" % (TAIL, len(d) - (bases + base_count * 0x134)))

    def tile(x, y):
        return TILES + (y * (w // 2) + x // 2) * 0x2C

    sp = {name: i32(d, SP_TABLE + idx * 4) for name, idx in SP.items()}
    sp_faction = {name: (d[bases + bid * 0x134 + 4] if 0 <= bid < base_count else None) for name, bid in sp.items()}

    factions = {}
    for f in range(1, 8):
        pd = PLAYERS_DATA + f * 0x20CC
        pl = PLAYERS + f * 0x59C
        fac = {
            "id": f,
            "noun": cstr(d, pl + 0x2E4, 24),
            "human": bool(human_bits & (1 << f)),
            "diff": u32(d, pd + 8),
            "bases": u32(d, pd + 0x105C),
            "se_talent": i32(d, pd + 0x258), "se_police": i32(d, pd + 0x260),
            "se_eff": i32(d, pd + 0x250), "econ": u32(d, pd + 0x22C),
            "rule_talent": i32(d, pl + 0x4E4), "rule_drone": i32(d, pl + 0x4E0),
            "wormpolice": bool(i32(d, pl + 0x508) & RFLAG_WORMPOLICE),
            "units": 0, "away": 0, "staple": 0, "assim": 0,
        }
        for k in FAC:
            fac[k] = 0
        factions[f] = fac
    for b in range(base_count):
        o = bases + b * 0x134
        f = d[o + 4]
        if f not in factions:
            continue
        fac = factions[f]
        fac["staple"] += d[o + 8] != 0
        fac["assim"] += d[o + 7] != 0
        for k, fid in FAC.items():
            fac[k] += has_fac(d, o, fid)
    for v in range(veh_count):
        o = vehs + v * 0x34
        f = d[o + 0xE]
        if f not in factions:
            continue
        x, y, proto = i16(d, o), i16(d, o + 2), i16(d, o + 0xA)
        if not (0 <= x < w and 0 <= y < h and 0 <= proto < 512):
            continue
        fac = factions[f]
        fac["units"] += 1
        plan = d[proto0 + proto * 0x34 + 0x2A]
        if plan <= PLAN_COMBAT_MAX and struct.unpack_from("<b", d, tile(x, y) + 7)[0] != f:
            fac["away"] += 1
    alive = [f for f in factions.values() if f["bases"] or f["units"]]
    return {"path": path, "turn": turn, "w": w, "h": h, "bases": base_count, "vehs": veh_count,
            "sp": sp, "sp_faction": sp_faction, "factions": alive}


def flags(r):
    fs = r["factions"]
    out = []
    if any(f["away"] for f in fs): out.append("AWAY")
    if any(f["staple"] for f in fs): out.append("STAPLE")
    if any(f["assim"] for f in fs): out.append("ASSIM")
    for k in ("punish", "brood", "rec", "holo", "paradise", "genejack"):
        if any(f[k] for f in fs): out.append(k.upper())
    for name in SP:
        if r["sp"][name] >= 0: out.append(name)
    if any(f["wormpolice"] for f in fs): out.append("WORMPOLICE")
    if any(f["rule_talent"] for f in fs): out.append("RULE_TALENT")
    if any(f["rule_drone"] for f in fs): out.append("RULE_DRONE")
    if any(f["se_police"] <= -3 for f in fs): out.append("POLICE<=-3")
    if any(f["human"] and f["diff"] != 3 for f in fs): out.append("DIFF%d" % next(f["diff"] for f in fs if f["human"]))
    return out


def report(r):
    print("%s: turn %d, map %dx%d, %d bases, %d units" % (os.path.basename(r["path"]), r["turn"], r["w"], r["h"], r["bases"], r["vehs"]))
    print("  secret projects:", ", ".join("%s base %d (faction %s)" % (n, b, r["sp_faction"][n]) for n, b in r["sp"].items() if b >= 0) or "none of the tracked ones")
    for f in r["factions"]:
        rules = []
        if f["rule_talent"]: rules.append("talent %d" % f["rule_talent"])
        if f["rule_drone"]: rules.append("drone %d" % f["rule_drone"])
        if f["wormpolice"]: rules.append("WORMPOLICE")
        print("  faction %d %-13s %-5s diff %d bases %3d units %4d away %3d | SE talent %+d police %+d eff %+d econ %d | %s | staple %d assim %d punish %d brood %d rec %d holo %d paradise %d genejack %d"
              % (f["id"], f["noun"], "human" if f["human"] else "AI", f["diff"], f["bases"], f["units"], f["away"],
                 f["se_talent"], f["se_police"], f["se_eff"], f["econ"], ", ".join(rules) or "no rules",
                 f["staple"], f["assim"], f["punish"], f["brood"], f["rec"], f["holo"], f["paradise"], f["genejack"]))
    print("  coverage:", " ".join(flags(r)))


def main(argv):
    table = "--table" in argv
    paths = [a for a in argv if not a.startswith("--")]
    for p in paths:
        try:
            r = scan(p)
        except Exception as e:  # keep going over a folder
            print("%s: ERROR %s" % (os.path.basename(p), e))
            continue
        if table:
            print("%-45s turn %3d %3dx%-3d bases %3d units %4d | %s" % (os.path.basename(p)[:45], r["turn"], r["w"], r["h"], r["bases"], r["vehs"], " ".join(flags(r))))
        else:
            report(r)


if __name__ == "__main__":
    main(sys.argv[1:])
