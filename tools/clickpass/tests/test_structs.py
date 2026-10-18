import os
import shutil
import tempfile
import unittest
from unittest import mock

from clickpass.structs import Field, LayoutError, checked_layout, layout, struct_size


def by_name(fields):
    return {f.name: f for f in fields}


class RealHeadersTest(unittest.TestCase):
    def test_veh_layout(self):
        self.assertEqual(struct_size("Veh"), 0x34)
        fields = by_name(layout("Veh"))
        self.assertEqual(fields["x"], Field(0, 2, "int16_t", "x"))
        self.assertEqual(fields["dmg_incurred"].offset, 0x10)
        self.assertEqual(fields["order"].offset, 0x11)
        self.assertEqual(fields["waypoint_x[0]"], Field(0x14, 2, "int16_t", "waypoint_x[0]"))
        self.assertEqual(fields["waypoint_x[3]"].offset, 0x1A)
        self.assertNotIn("waypoint_x", fields)

    def test_nested_structs_and_2d_arrays_flattened(self):
        self.assertEqual(struct_size("PlayerData"), 0x20CC)
        fields = by_name(layout("PlayerData"))
        self.assertEqual(fields["soc_effect_active.economy"], Field(0x278, 4, "int", "soc_effect_active.economy"))
        self.assertEqual(fields["goals[1].x"].offset, 0x19FC + 0x10 + 4)
        self.assertEqual(fields["saved_queue_name[1][2]"].offset, 0x470 + 24 + 2)

    def test_dump_section_structs(self):
        # the structs the dump's GameState, VehPrototypes and Monuments sections are read with
        block = 0x9A6490  # game_data()'s state block, which ClickpassGameState mirrors
        self.assertEqual(struct_size("ClickpassGameState"), 0x398)
        fields = by_name(layout("ClickpassGameState"))
        for name, address in (("game_state", 0x9A64C0), ("turn", 0x9A64D4), ("faction_rankings[0]", 0x9A64EC),
                              ("secret_projects.human_genome_project", 0x9A6514),
                              ("secret_projects.user_defined_64", 0x9A6610), ("governor_faction_id", 0x9A6614),
                              ("tech_achieved[0]", 0x9A6670), ("save_path[0]", 0x9A66C9),
                              ("save_path[255]", 0x9A67C8), ("climate_value_a", 0x9A67CC),
                              ("sunspot_duration", 0x9A6800), ("more_rules", 0x9A681C), ("unk_9A6820[1]", 0x9A6824)):
            self.assertEqual(fields[name].offset, address - block, name)
        self.assertEqual(struct_size("VehPrototype"), 0x34)
        self.assertEqual(struct_size("Monument"), 0x4F4)
        self.assertEqual(by_name(layout("Monument"))["records[20].unk_2[7]"].offset, 20 * 0x38 + 0x34)


class SyntheticHeadersTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.env = mock.patch.dict(os.environ, {"OSRC": self.tmp})
        self.env.start()

    def tearDown(self):
        self.env.stop()
        shutil.rmtree(self.tmp)

    def write_header(self, text):
        with open(os.path.join(self.tmp, "t.h"), "w", encoding="utf-8") as f:
            f.write(text)

    def test_natural_alignment_from_osrc(self):
        self.write_header("struct Padded {\n    uint8_t a; // comment\n    int b;\n    int16_t c;\n};\n")
        self.assertEqual(layout("Padded"), [
            Field(0, 1, "uint8_t", "a"),
            Field(4, 4, "int", "b"),
            Field(8, 2, "int16_t", "c"),
        ])
        self.assertEqual(struct_size("Padded"), 12)

    def test_one_line_typedef_struct_does_not_swallow_next_struct(self):
        self.write_header("typedef struct char256 { char str[256]; } char256;\n"
                          "struct Node {\n    void *vtable;\n    Node *next;\n};\n")
        self.assertEqual(layout("Node"), [Field(0, 4, "void*", "vtable"), Field(4, 4, "Node*", "next")])

    def test_unknown_member_type_raises(self):
        self.write_header("struct Bad {\n    int a;\n    Sprite s;\n};\n")
        with self.assertRaises(LayoutError):
            layout("Bad")

    def test_unknown_struct_raises(self):
        self.write_header("struct Known {\n    int a;\n};\n")
        with self.assertRaises(LayoutError):
            layout("Unknown")

    def test_checked_layout_size_mismatch_raises(self):
        self.write_header("struct Four {\n    int a;\n};\n")
        self.assertEqual(checked_layout("Four", 4), [Field(0, 4, "int", "a")])
        with self.assertRaises(LayoutError):
            checked_layout("Four", 8)


if __name__ == "__main__":
    unittest.main()
