import os
import shutil
import tempfile
import unittest

from clickpass.tables import Row, bisect_halves, read_rows, reference_rows, write_table

REDIRECTS_TSV = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "redirects.tsv")


class TablesTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def write_bytes(self, name, data):
        path = os.path.join(self.tmp, name)
        with open(path, "wb") as f:
            f.write(data)
        return path

    def test_read_marks_approved(self):
        path = self.write_bytes("t.tsv",
                                b"0x001A6AC0\t?game_data@@YAHPAU_iobuf@@H@Z\tapproved\r\n"
                                b"0x00101DA0\t?plain@@YAXXZ\r\n")
        self.assertEqual(read_rows(path), [
            Row(0x001A6AC0, "?game_data@@YAHPAU_iobuf@@H@Z", True),
            Row(0x00101DA0, "?plain@@YAXXZ", False),
        ])

    def test_comments_and_blank_lines_skipped(self):
        path = self.write_bytes("t.tsv",
                                b"# header comment\r\n"
                                b"\r\n"
                                b"0x00000010\t?a@@YAXXZ\r\n"
                                b"   \r\n"
                                b"# --- section ---\r\n"
                                b"0x00000020\t?b@@YAXXZ\tapproved\r\n")
        self.assertEqual(read_rows(path), [
            Row(0x10, "?a@@YAXXZ", False),
            Row(0x20, "?b@@YAXXZ", True),
        ])

    def test_write_round_trip(self):
        rows = [
            Row(0x001D4560, "??0Heap@@QAE@XZ", False),
            Row(0x00101DA0, "?battle_compute@@YAXHHPAH0H@Z", True),
            Row(0x00000ABC, "?c@@YAXXZ", False),
        ]
        path = os.path.join(self.tmp, "out.tsv")
        write_table(rows, path)
        with open(path, "rb") as f:
            data = f.read()
        self.assertEqual(data,
                         b"0x001D4560\t??0Heap@@QAE@XZ\r\n"
                         b"0x00101DA0\t?battle_compute@@YAXHHPAH0H@Z\tapproved\r\n"
                         b"0x00000ABC\t?c@@YAXXZ\r\n")
        self.assertEqual(read_rows(path), rows)

    def test_reference_rows_only_approved(self):
        names = [row.name for row in reference_rows(read_rows(REDIRECTS_TSV))]
        self.assertCountEqual(names, [
            "?game_data@@YAHPAU_iobuf@@H@Z",
            "?battle_compute@@YAXHHPAH0H@Z",
            "?best_defender@@YAIIHH@Z",
            "?hex_cost@@YAHHHIIIIH@Z",
            "?wants_to_attack@@YAHIIH@Z",
            "?drone_riot@@YAXXZ",
        ])

    def test_bisect_halves_never_takes_approved(self):
        rows = [Row(i, "?f%d@@YAXXZ" % i, False) for i in range(7)]
        with self.assertRaises(ValueError):
            bisect_halves(rows[:3] + [Row(99, "?approved@@YAXXZ", True)])
        first, second = bisect_halves(rows)
        self.assertEqual(len(first), 3)
        self.assertEqual(len(second), 4)
        self.assertEqual(first + second, rows)


if __name__ == "__main__":
    unittest.main()
