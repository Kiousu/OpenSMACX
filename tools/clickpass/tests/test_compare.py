import contextlib
import io
import os
import shutil
import struct
import tempfile
import unittest
from unittest import mock

from clickpass.compare import Difference, diff_dumps, main, read_ignore
from clickpass.dump import Dump, DumpError, Section, read_dump
from clickpass.structs import LayoutError

VEH_SIZE = 0x34
DMG_INCURRED = 0x10
WAYPOINT_X_2 = 0x18


def section(name, type_name, elem_size, elements):
    data = b"".join(elements)
    assert all(len(e) == elem_size for e in elements)
    return (name.encode("ascii").ljust(24, b"\0") + type_name.encode("ascii").ljust(24, b"\0")
            + struct.pack("<II", len(elements), elem_size) + data)


def veh(dmg_incurred=0, waypoint_x_2=0):
    data = bytearray(VEH_SIZE)
    data[DMG_INCURRED] = dmg_incurred
    struct.pack_into("<h", data, WAYPOINT_X_2, waypoint_x_2)
    return bytes(data)


def vehs(*elements):
    return section("Vehs", "Veh", VEH_SIZE, list(elements))


def globals_(**values):
    """The Globals section and the GlobalNames section naming its elements."""
    return [section("Globals", "u32", 4, [struct.pack("<I", v) for v in values.values()]),
            section("GlobalNames", "char32", 32, [n.encode("ascii").ljust(32, b"\0") for n in values])]


class CompareTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.files = 0

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def write_dump(self, *sections, step=0, magic=b"OSXDUMP1"):
        self.files += 1
        path = os.path.join(self.tmp, "dump_%d.bin" % self.files)
        with open(path, "wb") as f:
            f.write(magic + struct.pack("<II", step, len(sections)) + b"".join(sections))
        return path

    def dump(self, *sections):
        return read_dump(self.write_dump(*sections))

    def diff(self, a_sections, b_sections, ignore=frozenset(), **kwargs):
        a = self.dump(*a_sections)
        b = self.dump(*b_sections)
        return [str(d) for d in diff_dumps(a, b, set(ignore), **kwargs)]

    def test_read_dump_sections(self):
        path = self.write_dump(vehs(veh(), veh(dmg_incurred=3)), *globals_(turn=10), step=7)
        dump = read_dump(path)
        self.assertEqual(dump.step, 7)
        self.assertEqual(list(dump.sections), ["Vehs", "Globals", "GlobalNames"])
        self.assertEqual(dump.sections["Vehs"], Section("Veh", 2, VEH_SIZE, veh() + veh(dmg_incurred=3)))
        self.assertEqual(dump.sections["Globals"], Section("u32", 1, 4, struct.pack("<I", 10)))

    def test_read_dump_rejects_bad_magic_and_truncation(self):
        with self.assertRaises(DumpError):
            read_dump(self.write_dump(vehs(veh()), magic=b"OSXDUMP0"))
        path = self.write_dump(vehs(veh()))
        with open(path, "r+b") as f:
            f.truncate(os.path.getsize(path) - 1)
        with self.assertRaises(DumpError):
            read_dump(path)

    def test_identical_dumps_no_differences(self):
        sections = [vehs(veh(), veh(dmg_incurred=3)), *globals_(turn=10)]
        self.assertEqual(self.diff(sections, sections), [])

    def test_diff_names_field(self):
        self.assertEqual(self.diff([vehs(veh(), veh(dmg_incurred=3))],
                                   [vehs(veh(), veh(dmg_incurred=2))]),
                         ["Vehs[1].dmg_incurred: 3 vs 2"])

    def test_diff_count_mismatch(self):
        self.assertEqual(self.diff([vehs(veh(), veh())], [vehs(veh(), veh(), veh())]),
                         ["Vehs.count: 2 vs 3"])

    def test_missing_section_is_count_difference(self):
        self.assertEqual(self.diff([vehs(veh())], [vehs(veh()), *globals_(turn=10)]),
                         ["Globals.count: missing vs 1", "GlobalNames.count: missing vs 1"])

    def test_ignore_skips_field(self):
        a = [vehs(veh(dmg_incurred=3, waypoint_x_2=5))]
        b = [vehs(veh(dmg_incurred=2, waypoint_x_2=6))]
        self.assertEqual(self.diff(a, b), ["Vehs[0].dmg_incurred: 3 vs 2", "Vehs[0].waypoint_x[2]: 5 vs 6"])
        self.assertEqual(self.diff(a, b, {("Vehs", "dmg_incurred")}), ["Vehs[0].waypoint_x[2]: 5 vs 6"])
        # an array name without an index ignores every element of it
        self.assertEqual(self.diff(a, b, {("Vehs", "dmg_incurred"), ("Vehs", "waypoint_x")}), [])

    def test_globals_by_name(self):
        self.assertEqual(self.diff(globals_(game_state=4, turn=10), globals_(game_state=4, turn=11)),
                         ["Globals.turn: 10 vs 11"])

    def test_bytes_section_by_offset(self):
        a = bytes([0, 1, 2, 3, 0x1F, 5, 6, 7])
        b = bytes([0, 1, 2, 3, 0x20, 5, 6, 7])
        self.assertEqual(self.diff([section("Random", "bytes", 8, [a])], [section("Random", "bytes", 8, [b])]),
                         ["Random+0x04: 1f vs 20"])
        self.assertEqual(self.diff([section("Random", "bytes", 8, [a])], [section("Random", "bytes", 8, [b])],
                                   {("Random", "+0x04")}), [])

    def test_text_section_compared_as_text(self):
        def text(value, size=16, tail=b""):
            raw = value.encode("ascii") + b"\0" + tail
            return section("SavePath", "text", size, [raw.ljust(size, b"\0")])
        self.assertEqual(self.diff([text("<game>\\a")], [text("<game>\\b")]), ["SavePath: <game>\\a vs <game>\\b"])
        # what follows the end of the text is not part of it
        self.assertEqual(self.diff([text("same")], [text("same", tail=b"junk")]), [])
        self.assertEqual(self.diff([text("x")], [text("y")], {("SavePath", "")}), [])
        with self.assertRaises(DumpError):  # one string per section
            read_dump(self.write_dump(section("SavePath", "text", 4, [b"ab\0\0", b"cd\0\0"])))

    def test_char32_text_after_the_end_ignored(self):
        a = section("PopupLabel", "char32", 32, [b"REALLYQUIT".ljust(32, b"\0")])
        b = section("PopupLabel", "char32", 32, [b"REALLYQUIT\0junk".ljust(32, b"\0")])
        c = section("PopupLabel", "char32", 32, [b"".ljust(32, b"\0")])
        self.assertEqual(self.diff([a], [b]), [])
        self.assertEqual(self.diff([a], [c]), ["PopupLabel[0]: REALLYQUIT vs "])

    def test_padding_reported_by_offset(self):
        with open(os.path.join(self.tmp, "pad.h"), "w", encoding="utf-8") as f:
            f.write("struct Padded {\n    uint8_t a;\n    int b;\n};\n")
        with mock.patch.dict(os.environ, {"OSRC": self.tmp}):
            self.assertEqual(self.diff([section("Pads", "Padded", 8, [bytes(8)])],
                                       [section("Pads", "Padded", 8, [bytes([1, 0, 7, 0, 2, 0, 0, 0])])]),
                             ["Pads[0].a: 0 vs 1", "Pads[0]+0x02: 00 vs 07", "Pads[0].b: 0 vs 2"])

    def test_limit(self):
        a = [vehs(veh(dmg_incurred=1), veh(dmg_incurred=1), veh(dmg_incurred=1))]
        b = [vehs(veh(dmg_incurred=2), veh(dmg_incurred=2), veh(dmg_incurred=2))]
        self.assertEqual(self.diff(a, b, limit=2),
                         ["Vehs[0].dmg_incurred: 1 vs 2", "Vehs[1].dmg_incurred: 1 vs 2"])

    def test_limit_below_one_rejected(self):
        # a limit of 0 or less would report "no differences" for dumps that differ
        a = self.dump(vehs(veh(dmg_incurred=1)))
        b = self.dump(vehs(veh(dmg_incurred=2)))
        for limit in (0, -1):
            with self.assertRaises(ValueError):
                diff_dumps(a, b, set(), limit)
            with self.assertRaises(ValueError):
                diff_dumps(a, a, set(), limit)
        pa, pb = self.write_dump(vehs(veh(dmg_incurred=1))), self.write_dump(vehs(veh(dmg_incurred=2)))
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as raised:
            main([pa, pb, "--limit", "0"])
        self.assertEqual(raised.exception.code, 2)

    def test_elem_size_mismatch_raises(self):
        short_veh = section("Vehs", "Veh", VEH_SIZE - 4, [bytes(VEH_SIZE - 4)])
        with self.assertRaises(LayoutError):
            read_dump(self.write_dump(short_veh))
        dump = Dump(0, {"Vehs": Section("Veh", 1, VEH_SIZE - 4, bytes(VEH_SIZE - 4))})
        with self.assertRaises(LayoutError):
            diff_dumps(dump, dump, set())

    def test_difference_str(self):
        self.assertEqual(str(Difference("Vehs", 412, "dmg_incurred", 3, 2)), "Vehs[412].dmg_incurred: 3 vs 2")
        self.assertEqual(str(Difference("Globals", None, "turn", 10, 11)), "Globals.turn: 10 vs 11")

    def test_read_ignore(self):
        path = os.path.join(self.tmp, "ignore.tsv")
        with open(path, "wb") as f:
            f.write(b"section\tfield\treason\r\nPlayers\tsome_ptr\theap pointer\r\n\r\n")
        self.assertEqual(read_ignore(path), {("Players", "some_ptr")})
        shipped = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "ignore.tsv")
        # only R13's stored save path, which the dump also writes with the folder named (SavePath)
        self.assertEqual(read_ignore(shipped), {("GameState", "save_path")})

    def test_main_prints_differences(self):
        a = self.write_dump(vehs(veh(dmg_incurred=3)))
        b = self.write_dump(vehs(veh(dmg_incurred=2)))
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(main([a, b]), 1)
            self.assertEqual(main([a, a]), 0)
        self.assertEqual(out.getvalue().splitlines(), ["Vehs[0].dmg_incurred: 3 vs 2", "no differences"])


if __name__ == "__main__":
    unittest.main()
