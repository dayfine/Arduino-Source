"""Route 203 from the Diamond decomp, against a checked-in grid and what the console showed.

The fixtures are the bytes this needs, cut from the pret files: Diamond's
tile attributes of land_data narc_0016 (Jubilife's edge), narc_0019 and
narc_0020 (Route 203), and Platinum's height data of map_data_019 and 020
(Diamond keeps the same levels in a layout decomp.py does not read). The
grid is the rectangle BDSP AutoStory's Route203.json uses: world x 191..251,
z 740..763, so (x, y) here is that file's (x, y).
"""
import struct
import unittest
from pathlib import Path

from devtools.maps import decomp

HERE = Path(__file__).parent
FIXTURES = HERE / 'fixtures' / 'route203'
RECT = (191, 740, 61, 24)
LANDS = {(23, 5): 16, (23, 6): 19, (23, 7): 20}     # (row, column) of matrix 000 -> land id
PLATINUM_CACHE = Path.home() / '.cache' / 'maps' / 'decomp' / 'pret' / 'pokeplatinum' / 'res' / 'field' / 'maps' / 'data'
#  The Ravaged Path's lower cell has open tiles outside the cave at two
#  heights (rows 14 and 15, columns 0..13): void the player never reaches.
EXEMPT = {'map_data_436.bin': {((x, 14), (x, 15)) for x in range(14)}}


class FixtureGame:
    def land_data(self, land_id):
        attrs = (FIXTURES / f'land_data_narc_{land_id:04d}.attrs').read_bytes()
        return list(struct.unpack('<1024H', attrs))


def world():
    maps = [None] * (30 * 30)
    for (row, col), land in LANDS.items():
        maps[row * 30 + col] = land
    return decomp.World(FixtureGame(), decomp.Matrix(0, 30, 30, maps))


def grid():
    rows, _ = decomp.terrain(world(), RECT)
    return [''.join(r) for r in rows]


def neighbours(rows, pos, banned=()):
    x, y = pos
    for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
        if 0 <= ny < len(rows) and 0 <= nx < len(rows[0]) and rows[ny][nx] in '."W' and (nx, ny) not in banned:
            yield nx, ny


def reachable(rows, start, banned=()):
    seen, todo = {start}, [start]
    while todo:
        for n in neighbours(rows, todo.pop(), banned):
            if n not in seen:
                seen.add(n)
                todo.append(n)
    return seen


class Route203GridTest(unittest.TestCase):
    def test_matches_the_checked_in_grid(self):
        expected = (HERE / 'route203_grid.txt').read_text(encoding='utf-8').splitlines()
        self.assertEqual(grid(), expected)


class Route203ConsoleTest(unittest.TestCase):
    """What runs r75, r77 and r78 saw (2026-10-31/11-01)."""
    POCKET = {(x, y) for x in range(30, 36) for y in range(4, 7)}
    STAIRS = {(36, 5), (36, 6)}
    GATE_EXIT = (55, 9)

    def setUp(self):
        self.rows = grid()
        #  The item ball north of the pond (zone_event narc_0329) stands on (31, 5).
        self.rows[5] = self.rows[5][:31] + '#' + self.rows[5][32:]

    def test_gate_exit_is_walkable(self):
        self.assertIn(self.rows[9][55], '."W')

    def test_pocket_is_reached_only_by_the_stairs(self):
        from_gate = reachable(self.rows, self.GATE_EXIT)
        self.assertIn((30, 5), from_gate)
        self.assertNotIn((30, 5), reachable(self.rows, self.GATE_EXIT, self.STAIRS))
        pocket = reachable(self.rows, (30, 5), self.STAIRS)
        self.assertEqual(pocket, self.POCKET - {(31, 5)})
        exits = {n for p in pocket for n in neighbours(self.rows, p)} - pocket
        self.assertEqual(exits, self.STAIRS)

    def test_west_from_the_gate_exit_stops_at_the_cliff(self):
        x, y = self.GATE_EXIT
        while self.rows[y][x - 1] in '."W':
            x -= 1
        self.assertEqual((x, y), (38, 9))


class HeightGuardTest(unittest.TestCase):
    def test_route203_cliffs_are_all_blocked_tiles(self):
        game = FixtureGame()
        for land in (19, 20):
            plates = decomp.decode_bdhc((FIXTURES / f'map_data_{land:03d}.bdhc').read_bytes())
            self.assertEqual(decomp.height_walls(game.land_data(land), decomp.tile_heights(plates)), [])

    def test_route203_levels_and_stairs(self):
        heights = decomp.tile_heights(decomp.decode_bdhc((FIXTURES / 'map_data_020.bdhc').read_bytes()))
        level = lambda x, z: heights[z * 32 + x]
        #  Local to cell 020 (world x 224..255): the pocket's level, the
        #  stairs up from it, and the grass by the Gate.
        self.assertEqual(level(2, 9), [32.0])
        self.assertEqual([round(level(x, 9)[0]) for x in (3, 4)], [40, 56])
        self.assertEqual(level(5, 9), [64.0])

    def test_a_step_up_without_a_wall_is_found(self):
        plates = [(-256 * 4096, -256 * 4096, 0, 256 * 4096, (0, 4096, 0), -16 * 4096),
                  (0, -256 * 4096, 256 * 4096, 256 * 4096, (0, 4096, 0), -48 * 4096)]
        walls = decomp.height_walls([0] * 1024, decomp.tile_heights(plates))
        self.assertEqual(len(walls), 32)
        self.assertEqual(walls[0], ((15, 0), (16, 0), 16.0, 48.0))

    def test_rejects_other_layouts(self):
        with self.assertRaises(ValueError):
            decomp.decode_bdhc(b'XXXX' + bytes(12))

    @unittest.skipUnless(PLATINUM_CACHE.is_dir(), 'no pokeplatinum land data in ~/.cache/maps/decomp')
    def test_cached_platinum_maps(self):
        for path in sorted(PLATINUM_CACHE.glob('map_data_*.bin')):
            with self.subTest(path.name):
                data = path.read_bytes()
                attrs = decomp.decode_land_data(data)
                walls = decomp.height_walls(attrs, decomp.tile_heights(decomp.decode_bdhc(decomp.land_bdhc(data))))
                self.assertEqual({(a, b) for a, b, _, _ in walls} - EXEMPT.get(path.name, set()), set())


if __name__ == '__main__':
    unittest.main()
