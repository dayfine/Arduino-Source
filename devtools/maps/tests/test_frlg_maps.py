import unittest

from devtools.maps import frlg_maps

TILES = ['.....'] * 5


def event(**fields):
    obj = {'graphics_id': 'OBJ_EVENT_GFX_BUG_CATCHER', 'x': 2, 'y': 2, 'movement_type': 'MOVEMENT_TYPE_FACE_RIGHT',
           'movement_range_x': 1, 'movement_range_y': 1, 'trainer_type': 'TRAINER_TYPE_NORMAL',
           'trainer_sight_or_berry_tree_id': '3', 'flag': '0'}
    obj.update(fields)
    return obj


class TrainerTest(unittest.TestCase):
    def test_facing_trainer_looks_ahead(self):
        data = frlg_maps.map_object(event(), TILES)
        self.assertEqual(data['facing'], 'east')
        self.assertEqual(data['trainer'], {'sight': 3})

    def test_wandering_trainer_looks_both_ways(self):
        data = frlg_maps.map_object(event(movement_type='MOVEMENT_TYPE_WANDER_UP_AND_DOWN'), TILES)
        self.assertNotIn('facing', data)
        self.assertEqual(data['trainer'], {'sight': 3, 'looks': ['north', 'south']})
        self.assertEqual(data['wander'], {'x0': 2, 'y0': 1, 'x1': 2, 'y1': 3})

    def test_trainer_looking_around_looks_every_way(self):
        data = frlg_maps.map_object(event(movement_type='MOVEMENT_TYPE_LOOK_AROUND'), TILES)
        self.assertEqual(data['trainer']['looks'], frlg_maps.ALL_LOOKS)

    def test_others_are_no_trainers(self):
        for fields in ({'trainer_type': 'TRAINER_TYPE_NONE'}, {'trainer_sight_or_berry_tree_id': '0'}):
            data = frlg_maps.map_object(event(**fields), TILES)
            self.assertNotIn('trainer', data)
            self.assertNotIn('facing', data)


class EncounterTest(unittest.TestCase):
    #   MtMoon_1F's cave floor: behavior MB_CAVE (0x08), encounter type LAND.
    CAVE_FLOOR = 0x08 | (frlg_maps.TILE_ENCOUNTER_LAND << frlg_maps.ENCOUNTER_TYPE_SHIFT)

    def test_cave_floor_has_wild_encounters(self):
        self.assertEqual(frlg_maps.encounter_glyph('.', self.CAVE_FLOOR), '~')

    def test_floor_without_encounters_stays_open(self):
        self.assertEqual(frlg_maps.encounter_glyph('.', 0x08), '.')

    def test_walls_stay_walls(self):
        self.assertEqual(frlg_maps.encounter_glyph('#', self.CAVE_FLOOR), '#')


if __name__ == '__main__':
    unittest.main()
