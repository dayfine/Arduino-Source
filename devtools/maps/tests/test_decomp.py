import json
import struct
import tempfile
import unittest
from pathlib import Path

from devtools.maps import decomp

#  One synthetic 32 x 32 cell, open ground but for the first row:
#  wall, tall grass, ledge south, ledge east, door, warp tile, water, cave wall, cave floor.
ROW0 = [0x8000, 0x02, 0x3b, 0x38, 0x8069, 0x6e, 0x10, 0x8008, 0x08]
#  ... then the objects on top: an item (9), a signpost (10), a trainer (11), a wanderer (12).
EXPECTED_ROW0 = '#"v#DW##.#SN' + '.' * 20


def land_data(row0):
    attrs = [0] * 1024
    attrs[:len(row0)] = row0
    return struct.pack('<4I', 2048, 0, 0, 0) + struct.pack('<1024H', *attrs)


def write(root, path, data):
    path = Path(root) / path
    path.parent.mkdir(parents=True, exist_ok=True)
    if isinstance(data, bytes):
        path.write_bytes(data)
    else:
        path.write_text(data, encoding='utf-8')


def platinum_tree(root):
    write(root, 'include/data/map_headers.h', '''
    [MAP_HEADER_TEST_TOWN] = {
        .areaDataArchiveID = area_data_008,
        .mapMatrixID = map_matrix_000,
        .eventsArchiveID = events_test_town,
        .mapType = MAP_TYPE_OUTDOORS,
    },
    [MAP_HEADER_OTHER] = {
        .mapMatrixID = map_matrix_007,
        .eventsArchiveID = events_other,
    },
''')
    write(root, 'res/field/matrices/map_matrix_000.json', json.dumps({
        'name': 'map', 'headers': [['MAP_HEADER_TEST_TOWN', 'MAP_HEADER_EVERYWHERE']],
        'altitudes': [[0, 0]], 'maps': [['MAP_001', 'MAP_NONE']]}))
    write(root, 'res/field/maps/data/map_data_001.bin', land_data(ROW0))
    write(root, 'res/field/events/events_test_town.json', json.dumps({
        'bg_events': [{'script': 1, 'type': 2, 'x': 0, 'z': 2, 'y': 0, 'player_facing_dir': 'BG_EVENT_DIR_ALL'},
                      {'script': 2, 'type': 0, 'x': 0, 'z': 1, 'y': 0, 'player_facing_dir': 'BG_EVENT_DIR_ALL'}],
        'object_events': [
            {'id': 'LOCALID_ITEM_POTION', 'graphics_id': 'OBJ_EVENT_GFX_POKEBALL', 'movement_type': 'MOVEMENT_TYPE_NONE',
             'trainer_type': 'TRAINER_TYPE_NONE', 'hidden_flag': 'FLAG_OBTAINED_POTION', 'script': 7000, 'initial_dir': 0,
             'data': [0], 'movement_range_x': 0, 'movement_range_z': 0, 'x': 9, 'z': 0, 'y': 0},
            {'id': 'LOCALID_SIGN', 'graphics_id': 'OBJ_EVENT_GFX_ARROW_SIGNPOST', 'movement_type': 'MOVEMENT_TYPE_NONE',
             'trainer_type': 'TRAINER_TYPE_NONE', 'hidden_flag': '0', 'script': 3, 'initial_dir': 0,
             'data': [13], 'movement_range_x': 0, 'movement_range_z': 0, 'x': 10, 'z': 0, 'y': 0},
            {'id': 'LOCALID_LASS_SAMANTHA', 'graphics_id': 'OBJ_EVENT_GFX_LASS', 'movement_type': 'MOVEMENT_TYPE_LOOK_SOUTH',
             'trainer_type': 'TRAINER_TYPE_NORMAL', 'hidden_flag': '0', 'script': 'TRAINER_LASS_SAMANTHA', 'initial_dir': 1,
             'data': [4], 'movement_range_x': 0, 'movement_range_z': 0, 'x': 11, 'z': 0, 'y': 0},
            {'id': 'LOCALID_YOUNGSTER', 'graphics_id': 'OBJ_EVENT_GFX_YOUNGSTER', 'movement_type': 'MOVEMENT_TYPE_WANDER_WEST_AND_EAST',
             'trainer_type': 'TRAINER_TYPE_NONE', 'hidden_flag': '0', 'script': 1, 'initial_dir': 3,
             'data': [0], 'movement_range_x': 2, 'movement_range_z': 0, 'x': 12, 'z': 0, 'y': 0},
            {'id': 'LOCALID_FAR_AWAY', 'graphics_id': 'OBJ_EVENT_GFX_LASS', 'movement_type': 'MOVEMENT_TYPE_NONE',
             'trainer_type': 'TRAINER_TYPE_NONE', 'hidden_flag': '0', 'script': 1, 'initial_dir': 3,
             'data': [0], 'movement_range_x': 0, 'movement_range_z': 0, 'x': 40, 'z': 0, 'y': 0},
        ],
        'warp_events': [{'x': 5, 'z': 0, 'dest_header_id': 'MAP_HEADER_OTHER', 'dest_warp_id': 0},
                        {'x': 4, 'z': 1, 'dest_header_id': 'MAP_HEADER_OTHER', 'dest_warp_id': 1}],
        'coord_events': []}))


def diamond_tree(root):
    """The same map in pokediamond's binary files, with pokeplatinum's name tables beside them."""
    write(root, 'include/constants/maps.h', '#define MAP_EVERYWHERE 0\n#define MAP_TEST_TOWN 3\n#define MAP_OTHER 4\n')
    write(root, 'arm9/src/map_header.c', '''static const struct MapHeader sMapHeaders[] = {
    { NARC_area_data_narc_0000_bin, NARC_map_matrix_narc_0000_bin, NARC_zone_event_release_narc_0000_bin, 0 }, // MAP_EVERYWHERE
    { NARC_area_data_narc_0000_bin, NARC_map_matrix_narc_0000_bin, NARC_zone_event_release_narc_0001_bin, 0 }, // MAP_NOTHING
    { NARC_area_data_narc_0000_bin, NARC_map_matrix_narc_0000_bin, NARC_zone_event_release_narc_0002_bin, 0 }, // MAP_UNDERGROUND
    { NARC_area_data_narc_0008_bin, NARC_map_matrix_narc_0000_bin, NARC_zone_event_release_narc_0331_bin, 0 }, // MAP_TEST_TOWN
};
''')
    matrix = struct.pack('<5B', 2, 1, 1, 1, 3) + b'map' + struct.pack('<2H', 3, 0) + bytes([0, 0]) + struct.pack('<2H', 1, 0xffff)
    write(root, 'files/fielddata/mapmatrix/map_matrix/narc_0000.bin', matrix)
    write(root, 'files/fielddata/land_data/land_data_release/narc_0001.bin', land_data(ROW0))
    movement = ['MOVEMENT_TYPE_NONE'] + ['MOVEMENT_TYPE_UNUSED'] * 4 + ['MOVEMENT_TYPE_WANDER_WEST_AND_EAST'] + ['MOVEMENT_TYPE_UNUSED'] * 9 + ['MOVEMENT_TYPE_LOOK_SOUTH']
    gfx = ['OBJ_EVENT_GFX_UNUSED'] * 95
    gfx[4], gfx[20], gfx[87], gfx[94] = 'OBJ_EVENT_GFX_YOUNGSTER', 'OBJ_EVENT_GFX_LASS', 'OBJ_EVENT_GFX_POKEBALL', 'OBJ_EVENT_GFX_ARROW_SIGNPOST'
    write(root, 'generated/movement_types.txt', '\n'.join(movement))
    write(root, 'generated/object_events_gfx.txt', '\n'.join(gfx))

    def obj(gfx_id, movement_id, trainer, flag, data0, range_x, x, z):
        return struct.pack('<14HI', 0, gfx_id, movement_id, trainer, flag, 1, 0, data0, 0, 0, range_x, 0, x, z, 0)
    events = struct.pack('<I', 2)
    events += struct.pack('<HHIIIH2x', 1, 2, 0, 2, 0, 0) + struct.pack('<HHIIIH2x', 2, 0, 0, 1, 0, 0)
    events += struct.pack('<I', 5)
    events += obj(87, 0, 0, 1000, 0, 0, 9, 0) + obj(94, 0, 0, 0, 13, 0, 10, 0) + obj(20, 15, 1, 0, 4, 0, 11, 0)
    events += obj(4, 5, 0, 0, 0, 2, 12, 0) + obj(20, 0, 0, 0, 0, 0, 40, 0)
    events += struct.pack('<I', 2) + struct.pack('<4H4x', 5, 0, 4, 0) + struct.pack('<4H4x', 4, 1, 4, 1)
    events += struct.pack('<I', 0)
    write(root, 'files/fielddata/eventdata/zone_event_release/narc_0331.bin', events)


class TileCharTest(unittest.TestCase):
    def test_legend(self):
        self.assertEqual([decomp.tile_char(a)[0] for a in ROW0], list('#"v#DW##.'))
        self.assertEqual(decomp.tile_char(0x38), ('#', 'ledge jumped east, shown as a wall'))
        self.assertEqual(decomp.tile_char(0x5b), ('v', 'double ledge'))
        self.assertEqual(decomp.tile_char(0x63), ('D', None))   # cave mouth, walked into
        self.assertEqual(decomp.tile_char(0x5e), ('W', None))   # warp stairs

    def test_land_data_header_is_checked(self):
        with self.assertRaises(ValueError):
            decomp.decode_land_data(struct.pack('<4I', 1024, 0, 0, 0) + bytes(1024))


class DecodeTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def decoded(self, repo, rect=None):
        game = decomp.game_for(repo, self.root / 'cache', local=self.root)
        return decomp.decode(game, 'TEST_TOWN', rect, date='2026-10-31')

    def test_platinum_rows_and_objects(self):
        platinum_tree(self.root)
        d = self.decoded('pret/pokeplatinum')
        self.assertEqual(d.rect, (0, 0, 32, 32))
        self.assertEqual(d.cells, [(0, 0, 'map_data_001')])
        self.assertEqual(len(d.rows), 32)
        self.assertEqual(d.rows[0], EXPECTED_ROW0)
        #  The bg sign on open ground is listed, not drawn; the hidden item is ignored.
        self.assertEqual(d.rows[1], '.' * 32)
        self.assertEqual(d.notes, {'ledge jumped east, shown as a wall': [(3, 0)], 'water': [(6, 0)]})
        self.assertEqual(d.placed, [
            'warp tile (5, 0) to Other',
            'arrival tile (4, 1) to Other',
            'sign script on open ground (0, 1), not marked',
            "item on the ground (9, 0), as '#', hidden by FLAG_OBTAINED_POTION",
            'arrow signpost (10, 0)',
            'Lass Samantha (11, 0), looks south, sees 4',
        ])
        self.assertEqual(d.off_grid, [
            'Youngster wander west and east from (12, 0), columns 10 to 14, row 0, not a trainer: off the grid'])
        self.assertEqual(d.source_line,
                         'Map source: the decomp (map_data_001, 2026-10-31) with the objects from its events file.')

    def test_rect_clips_and_marks_missing_cells(self):
        platinum_tree(self.root)
        d = self.decoded('pret/pokeplatinum', rect=(8, 0, 30, 2))
        self.assertEqual(d.rows[0], '.#SN' + '.' * 20 + '??????')
        self.assertEqual(d.cells, [(0, 0, 'map_data_001')])
        self.assertEqual(d.placed, ["item on the ground (1, 0), as '#', hidden by FLAG_OBTAINED_POTION",
                                    'arrow signpost (2, 0)', 'Lass Samantha (3, 0), looks south, sees 4'])

    def test_cpp_block(self):
        platinum_tree(self.root)
        text = decomp.render_cpp(self.decoded('pret/pokeplatinum', rect=(0, 0, 12, 1)))
        lines = text.splitlines()
        self.assertEqual(lines[0], '//  Map source: the decomp (map_data_001, 2026-10-31) with the objects from')
        self.assertEqual(lines[1], '//      its events file.')
        self.assertIn('//  Decoded from Pokemon Platinum\'s collision table for this map (the', lines)
        prose = ' '.join(line[len('//  '):] for line in lines if line.startswith('//  '))
        self.assertIn('Local (x, y) = world (x - 0, z - 0); 12 x 1 tiles.', prose)
        self.assertTrue(all(len(line) <= 80 for line in lines))
        self.assertEqual(lines[-2], '    //     0         1')
        self.assertEqual(lines[-1], '        R"(#"v#DW##.#SN)",  //   0')

    def test_diamond_decodes_the_same_map(self):
        platinum_tree(self.root)
        expected = self.decoded('pret/pokeplatinum')
        diamond_tree(self.root)
        d = self.decoded('pret/pokediamond')
        self.assertEqual(d.rows, expected.rows)
        self.assertEqual(d.cells, [(0, 0, 'land_data narc_0001')])
        self.assertEqual(d.events_name, 'zone_event narc_0331')
        self.assertEqual(d.placed, [
            'warp tile (5, 0) to Other',
            'arrival tile (4, 1) to Other',
            'sign script on open ground (0, 1), not marked',
            "item on the ground (9, 0), as '#', hidden by flag 1000",
            'arrow signpost (10, 0)',
            'Lass (object 2) (11, 0), looks south, sees 4',
        ])
        self.assertEqual(d.off_grid, [
            'Youngster (object 3) wander west and east from (12, 0), columns 10 to 14, row 0, not a trainer: off the grid'])
        self.assertIn("Decoded from Pokemon Diamond's", decomp.render_cpp(d))


class SourceTest(unittest.TestCase):
    def test_fetches_once_into_the_cache(self):
        calls = []

        def fetch(repo, path):
            calls.append((repo, path))
            return b'data'

        with tempfile.TemporaryDirectory() as tmp:
            source = decomp.Source('pret/pokeplatinum', tmp, fetch=fetch)
            self.assertEqual(source.read('res/x.bin'), b'data')
            self.assertEqual(source.read('res/x.bin'), b'data')
            self.assertEqual(calls, [('pret/pokeplatinum', 'res/x.bin')])
            self.assertTrue((Path(tmp) / 'pret/pokeplatinum/res/x.bin').exists())


if __name__ == '__main__':
    unittest.main()
