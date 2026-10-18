import hashlib
import json
import os
import shutil
import tempfile
import unittest

from clickpass.files import MASKS_TSV, Mask, diff_files, manifest_for, masked_ranges, read_masks, run_copies


def run_dir(root, name, files, step=0):
    """A run folder with files_<step>.json listing `files` (path -> bytes, None for deleted, or an
    Exception for unreadable) and their copies; returns the manifest's path."""
    folder = os.path.join(root, name)
    os.makedirs(os.path.join(folder, "files"), exist_ok=True)
    entries = []
    for path, data in files.items():
        if data is None:
            entries.append({"path": path, "deleted": True})
        elif isinstance(data, Exception):
            entries.append({"path": path, "error": str(data)})
        else:
            sha = hashlib.sha256(data).hexdigest()
            with open(os.path.join(folder, "files", sha), "wb") as f:
                f.write(data)
            entries.append({"path": path, "size": len(data), "sha256": sha})
    manifest = os.path.join(folder, "files_%d.json" % step)
    with open(manifest, "w", encoding="utf-8") as f:
        json.dump({"step": step, "files": entries}, f)
    return manifest


SAVE = "game\\saves\\_cp_save.SAV"
INI = "game\\Alpha Centauri.Ini"


class MasksTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def table(self, text):
        path = os.path.join(self.tmp, "masks.tsv")
        with open(path, "wb") as f:
            f.write(text.encode("ascii"))
        return path

    def test_read_masks(self):
        path = self.table("pattern\toffset\tlength\treason\r\n# a comment\r\n\r\n"
                          "*.SAV\t0x561\t256\tthe stored path\r\n"
                          "*\\Alpha Centauri.ini\tafter:Latest Save=\teol\tthe last save's path\r\n")
        self.assertEqual(read_masks(path), [Mask("*.SAV", 0x561, 256, "the stored path"),
                                            Mask("*\\Alpha Centauri.ini", "after:Latest Save=", "eol",
                                                 "the last save's path")])

    def test_bad_rows_rejected(self):
        for row in ("*.SAV\t0x561\t256", "*.SAV\t0x561\t256\t ", "*.SAV\tsoon\t256\twhy", "*.SAV\t0x561\tsome\twhy",
                    "*.SAV\t-1\t4\twhy", "*.SAV\t0\t0\twhy", "*.SAV\tafter:\t4\twhy", "\t0\t4\twhy"):
            with self.assertRaises(ValueError, msg=row):
                read_masks(self.table("pattern\toffset\tlength\treason\r\n" + row + "\r\n"))

    def test_shipped_masks(self):
        masks = read_masks(MASKS_TSV)
        self.assertTrue(all(m.reason for m in masks))
        save = bytes(0x20000)
        self.assertEqual(masked_ranges(SAVE, save, masks), [(0x561, 0x661), (0x144C0, 0x144C8)])
        ini = b"[Alpha Centauri]\r\nLatest Save=G:\\x\\saves\\_cp_save\r\nWindowsFileBox=0\r\n"
        start = ini.index(b"=G:") + 1
        self.assertEqual(masked_ranges(INI, ini, masks), [(start, ini.index(b"\r\nWindowsFileBox"))])
        self.assertEqual(masked_ranges("game\\logfile.txt", b"", masks), [])

    def test_after_text_masks_nothing_when_missing(self):
        masks = [Mask("*.ini", "after:Latest Save=", "eol", "why"), Mask("*.ini", "after:A=", 2, "why")]
        self.assertEqual(masked_ranges("game\\x.ini", b"B=1\r\nA=12345", masks), [(7, 9)])


class DiffFilesTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def diff(self, a, b, masks=(), limit=50):
        return [str(d) for d in diff_files(run_dir(self.tmp, "a", a), run_dir(self.tmp, "b", b), list(masks), limit)]

    def test_same_files_no_differences(self):
        files = {SAVE: b"abc", "game\\logfile.txt": b""}
        self.assertEqual(self.diff(files, dict(files)), [])
        self.assertEqual(self.diff({}, {}), [])

    def test_written_by_one_game_only(self):
        self.assertEqual(self.diff({SAVE: b"abc"}, {}), ["files[%s]: written (3 bytes) vs not written" % SAVE])
        self.assertEqual(self.diff({}, {"outside\\x.SAV": b"ab"}),
                         ["files[outside\\x.SAV]: not written vs written (2 bytes)"])
        self.assertEqual(self.diff({SAVE: None}, {SAVE: b"abc"}),
                         ["files[%s]: deleted vs written (3 bytes)" % SAVE])
        self.assertEqual(self.diff({SAVE: None}, {SAVE: None}), [])

    def test_byte_and_size_differences(self):
        self.assertEqual(self.diff({SAVE: b"\x01\x02\x03"}, {SAVE: b"\x01\x07\x03"}), ["files[%s]+0x01: 02 vs 07" % SAVE])
        self.assertEqual(self.diff({SAVE: b"\x01\x02"}, {SAVE: b"\x01\x02\x03"}), ["files[%s].size: 2 vs 3" % SAVE])
        self.assertEqual(self.diff({SAVE: bytes(4)}, {SAVE: b"\x09" * 4}, limit=2),
                         ["files[%s]+0x00: 00 vs 09" % SAVE, "files[%s]+0x01: 00 vs 09" % SAVE])

    def test_masked_bytes_not_compared(self):
        a, b = bytearray(16), bytearray(16)
        b[4:6] = b"\xff\xff"
        masks = [Mask("*.sav", 4, 2, "pointers")]  # (patterns ignore case)
        self.assertEqual(self.diff({SAVE: bytes(a)}, {SAVE: bytes(b)}, masks), [])
        b[6] = 1  # right after the mask
        self.assertEqual(self.diff({SAVE: bytes(a)}, {SAVE: bytes(b)}, masks), ["files[%s]+0x06: 00 vs 01" % SAVE])
        self.assertEqual(self.diff({SAVE: bytes(a)}, {SAVE: bytes(b)}, [Mask("*.ini", 4, 3, "x")]),
                         ["files[%s]+0x04: 00 vs ff" % SAVE, "files[%s]+0x05: 00 vs ff" % SAVE,
                          "files[%s]+0x06: 00 vs 01" % SAVE])

    def test_masked_line(self):
        masks = [Mask("game\\alpha centauri.ini", "after:Latest Save=", "eol", "own folder")]
        a = b"A=1\r\nLatest Save=G:\\ref\\saves\\_cp_save\r\nB=2\r\n"
        b = b"A=1\r\nLatest Save=G:\\osx\\saves\\_cp_save\r\nB=2\r\n"
        self.assertEqual(self.diff({INI: a}, {INI: b}, masks), [])
        self.assertEqual(self.diff({INI: a}, {INI: b.replace(b"B=2", b"B=3")}, masks),
                         ["files[%s]+0x%02X: 32 vs 33" % (INI, a.index(b"B=2") + 2)])

    def test_unreadable_file_never_compares_equal(self):
        self.assertEqual(self.diff({SAVE: OSError("in use")}, {SAVE: OSError("in use")}),
                         ["files[%s]: unreadable (in use) vs unreadable (in use)" % SAVE])

    def test_missing_list_or_copy_is_an_error(self):
        a = run_dir(self.tmp, "a", {SAVE: b"abc"})
        b = run_dir(self.tmp, "b", {SAVE: b"abd"})
        with self.assertRaises(OSError):
            diff_files(a, os.path.join(self.tmp, "b", "files_1.json"), [], 50)
        shutil.rmtree(os.path.join(self.tmp, "b", "files"))
        with self.assertRaises(ValueError):
            diff_files(a, b, [], 50)
        with self.assertRaises(ValueError):
            diff_files(a, a, [], 0)

    def test_manifest_for_and_copies(self):
        manifest = run_dir(self.tmp, "a", {SAVE: b"abc", INI: b"x", "game\\gone": None}, step=3)
        dump = os.path.join(self.tmp, "a", "dump_3.bin")
        self.assertEqual(manifest_for(dump), manifest)
        self.assertEqual(sorted(os.path.basename(p) for p in run_copies(manifest)),
                         sorted([hashlib.sha256(b"abc").hexdigest(), hashlib.sha256(b"x").hexdigest()]))


if __name__ == "__main__":
    unittest.main()
