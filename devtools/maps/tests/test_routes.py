import json
import struct
import tempfile
import unittest
from pathlib import Path

from devtools.maps import route_maps, route_render, route_trace
from devtools.maps.picture import Picture

#  A 10 x 6 FireRed-style town: walls round it, grass, a sign, a door, a
#  ledge, a mat, and a gap in the north wall onto Route 9.
FRLG_TOWN = {
    'name': 'TestTown', 'width': 10, 'height': 6,
    'tiles': [
        '####.#####',
        '#........#',
        '#..""""..#',
        '#..~~....#',
        '#JJJ.S.D.#',
        '####v#####',
    ],
    'warps': [{'x': 7, 'y': 4, 'dest_map': 'TestHouse', 'dest_warp': 0}],
    'objects': [{'x': 8, 'y': 1, 'kind': 'boy', 'flag': '', 'wander_x': 0, 'wander_y': 0}],
    'connections': [], 'points': [], 'flag_sets': [],
}
FRLG_ROUTE = {'name': 'Route9', 'width': 10, 'height': 4, 'tiles': ['#........#'] * 4}
FRLG_HOUSE = {'name': 'TestHouse', 'width': 6, 'height': 5, 'tiles': ['######', '#....#', '#....#', '#....#', '###v##']}
#  The shared schema (BDSP): '~' is tall grass here, 'v' a ledge.
BDSP_TOWN = {
    'name': 'Test Field', 'source': {'decomp_file': 'x', 'revision': 'y', 'date': 'z'},
    'tiles': [
        '########',
        '#......#',
        '#..~~..#',
        '#......#',
        '##vvv.D#',
        '########',
    ],
    'objects': [{'name': 'lass', 'kind': 'npc', 'x': 1, 'y': 1}],
}


def stamp(lines):
    return [f'2026-11-01 09:00:{i:02d}.000000 - [Console 0]: {line}' if not line.startswith('    ') else line
            for i, line in enumerate(lines)]


FRLG_LOG = stamp([
    '<b>Starting Program: PokemonFRLG:AutoStory</b>',
    '[Checkpoint] 00.05: start, 2 events',
    '[Event] 00.05/2 to_route: start',
    'Route: walk west 4 -> TestTown (4, 1)',
    'Route: walk south 3 -> TestTown (4, 4)',
    'Leg: a wild battle after 900ms, stopping the leg.',
    'Route: stopped part-way, finding the player again at TestTown (8, 3); walk east 6 (to wall) -> TestTown (6, 2); walk south 9 (to wall) -> TestTown (8, 3)',
    'Route: walk east 6 (to wall) -> TestTown (6, 2)',
    'Route: walk south 9 (to wall) -> TestTown (8, 3)',
    'Route: walk west 4 -> TestTown (4, 3)',
    'Route: walk south 1 -> TestTown (4, 4)',
    'Route: walk north 4 -> Route9 (4, 3)',
    '[Event] 00.05/2 to_route: done',
    '[Event] 00.05/3 house: start',
    'Route: plan, 3 steps: walk south 4 -> TestTown (4, 4); walk east 3 -> TestTown (7, 4); walk south 1 (warp) -> TestHouse (3, 3)',
    'Route: walk south 4 -> TestTown (4, 4)',
    'Route: walk east 3 -> TestTown (7, 4)',
    'Leg: a message after 400ms, stopping the leg.',
    'OperationFailedExceptionWithScreenshot: walk_overworld_route(): stopped part-way on walk east 3 -> TestTown (7, 4), with no walls near it to find the player again.',
    '[Event] 00.05/3 house: failed: operation_failed: walk_overworld_route(): stopped part-way',
])

BDSP_LOG = stamp([
    '<b>Starting Program: PokemonBDSP:AutoStory</b>',
    '[Event] 00.04b/2 grass: start',
    '[Route] To the grass: 3 legs planned',
    '[Route] To the grass: plan from Test Field (1, 3), Test Field (1, 2):',
    '    east 5 (to wall) to Test Field (6, 3), Test Field (6, 2)',
    '    north 2 (to wall) to Test Field (6, 1)',
    '    west 2 (open) to Test Field (4, 1)',
    '[Route] Leg 1/3: east 5 (to wall) to Test Field (6, 3), Test Field (6, 2)',
    '[Walk] To the grass: east 5 (to wall): 400ms, screen change 80.5.',
    '[Walk] To the grass: east 5 (to wall): blocked after 2000ms, 1 wild battle(s).',
    '[Route] Leg 2/3: north 2 (to wall) to Test Field (6, 1)',
    '[Route] To the grass: north 2: stopped early after about 1 tiles, at Test Field (6, 2); planning again.',
    '[Route] To the grass: plan from Test Field (6, 2):',
    '    west 4 (to wall) to Test Field (2, 2)',
    '[Route] Leg 1/1: west 4 (to wall) to Test Field (2, 2)',
    '[Walk] To the grass: west 4 (to wall): blocked after 1500ms, 0 wild battle(s).',
    '[Route] To the grass: arrived at Test Field (2, 2).',
    '[Event] 00.04b/2 grass: done',
])


#  The shared route log lines (Pokemon/Navigation/Pokemon_RouteLog.h), as
#  FRLG's walker writes them: one tile, a stop, holds that find the player.
SHARED_FRLG_LOG = stamp([
    '<b>Starting Program: PokemonFRLG:AutoStory</b>',
    '[Event] 00.05/2 to_route: start',
    '[Route] to_route: plan from TestTown (8, 1):',
    '    west 4 (open) to TestTown (4, 1)',
    '    south 3 (open) to TestTown (4, 4)',
    '    north 4 (open) to Route9 (4, 3)',
    '[Route] Leg 1/3: west 4 (open) to TestTown (4, 1)',
    '[Route] to_route: west 4 (open): ended at TestTown (4, 1) (walked).',
    '[Route] Leg 2/3: south 3 (open) to TestTown (4, 4)',
    'Leg: a wild battle after 900ms, stopping the leg.',
    '[Route] to_route: south 3 (open): ended at an unknown tile (unknown: stopped part-way).',
    '[Route] to_route: plan from an unknown tile:',
    '    east 6 (to wall) to TestTown (6, 2)',
    '    south 9 (to wall) to TestTown (8, 3)',
    '    west 4 (open) to TestTown (4, 3)',
    '    south 1 (open) to TestTown (4, 4)',
    '    north 4 (open) to Route9 (4, 3)',
    '[Route] Leg 1/5: east 6 (to wall) to TestTown (6, 2)',
    '[Route] to_route: east 6 (to wall): ended at an unknown tile (unknown: a hold from an unknown tile).',
    '[Route] Leg 2/5: south 9 (to wall) to TestTown (8, 3)',
    '[Route] to_route: south 9 (to wall): ended at TestTown (8, 3) (walked).',
    '[Route] to_route: found again at TestTown (8, 3), after 2 hold(s).',
    '[Route] Leg 3/5: west 4 (open) to TestTown (4, 3)',
    '[Route] to_route: west 4 (open): ended at TestTown (4, 3) (walked).',
    '[Route] Leg 4/5: south 1 (open) to TestTown (4, 4)',
    '[Route] to_route: south 1 (open): ended at TestTown (4, 4) (walked).',
    '[Route] Leg 5/5: north 4 (open) to Route9 (4, 3)',
    '[Route] to_route: north 4 (open): ended at Route9 (4, 3) (assumed: 1 wild battle(s) on the way).',
    '[Route] to_route: arrived at Route9 (4, 3).',
    '[Event] 00.05/2 to_route: done',
    '[Event] 00.05/3 house: start',
    '[Route] house: plan from TestTown (4, 4):',
    '    east 3 (open) to TestTown (7, 4)',
    '    south 1 (warp) to TestHouse (3, 3)',
    '    face west to TestHouse (3, 3)',
    '[Route] Leg 1/3: east 3 (open) to TestTown (7, 4)',
])


def maps():
    return {route_maps.key(m['name']): route_maps.parse(m) for m in (FRLG_TOWN, FRLG_ROUTE, FRLG_HOUSE, BDSP_TOWN)}


class MapsTest(unittest.TestCase):
    def test_each_schema_has_its_legend(self):
        frlg = route_maps.parse(FRLG_TOWN)
        self.assertEqual(frlg.kind(3, 2), route_maps.GRASS)
        self.assertEqual(frlg.kind(3, 3), route_maps.WATER)
        self.assertEqual(frlg.kind(1, 4), route_maps.LEDGE)
        self.assertEqual(frlg.kind(4, 5), route_maps.MAT)
        self.assertEqual(frlg.kind(7, 4), route_maps.DOOR)
        self.assertEqual(frlg.warps, [(7, 4, 'TestHouse')])
        bdsp = route_maps.parse(BDSP_TOWN)
        self.assertEqual(bdsp.kind(3, 2), route_maps.GRASS)
        self.assertEqual(bdsp.kind(2, 4), route_maps.LEDGE)
        self.assertEqual(bdsp.kind(-1, 0), route_maps.WALL)

    def test_names_from_logs_find_files(self):
        self.assertEqual(route_maps.key('Route 202'), route_maps.key('Route202'))
        self.assertIn(route_maps.key('Test Field'), maps())

    def test_load_dir_skips_other_json(self):
        with tempfile.TemporaryDirectory() as d:
            Path(d, 'TestTown.json').write_text(json.dumps(FRLG_TOWN))
            Path(d, 'notes.json').write_text('[1, 2]')
            self.assertEqual(list(route_maps.load_dir(d)), ['testtown'])


class FrlgTraceTest(unittest.TestCase):
    def setUp(self):
        runs = route_trace.parse(FRLG_LOG, maps())
        self.assertEqual(len(runs), 1)
        self.run = runs[0]

    def test_walks_are_story_events(self):
        self.assertEqual(self.run.game, 'PokemonFRLG')
        self.assertEqual([w.event for w in self.run.walks], ['00.05/2 to_route', '00.05/3 house'])
        self.assertEqual([w.outcome for w in self.run.walks], ['done', 'failed'])

    def test_plan_leaves_out_the_holds_and_the_way_back(self):
        walk = self.run.walks[0]
        self.assertEqual([s.role for s in walk.steps], ['plan', 'plan', 'hold', 'hold', 'back', 'back', 'plan'])
        self.assertEqual([l.describe() for l in walk.plans[0].legs], ['walk west 4', 'walk south 3', 'walk north 4'])
        #  Worked back from the first step's end.
        self.assertEqual(walk.plans[0].start, [('TestTown', 8, 1)])

    def test_stop_and_reanchor(self):
        walk = self.run.walks[0]
        self.assertEqual(walk.steps[1].outcome, 'stopped')
        self.assertEqual(walk.steps[1].end, [])
        self.assertFalse(walk.steps[2].certain)
        self.assertTrue(walk.steps[3].certain)
        self.assertEqual(walk.steps[4].start, [('TestTown', 8, 3)])
        kinds = [(m.kind, m.at) for m in walk.marks]
        self.assertIn(('battle', [('TestTown', 4, 1)]), kinds)
        self.assertIn(('reanchor', [('TestTown', 8, 3)]), kinds)

    def test_plan_line_and_failure(self):
        walk = self.run.walks[1]
        self.assertEqual([l.describe() for l in walk.plans[0].legs], ['walk south 4', 'walk east 3', 'walk south 1 (warp)'])
        self.assertEqual(walk.steps[-1].outcome, 'stopped')
        self.assertIn('no walls near it', walk.error)
        failed = [m for m in walk.marks if m.kind == 'failed']
        self.assertEqual(len(failed), 1)
        self.assertEqual(failed[0].at, [('TestTown', 4, 4)])


    def test_a_reload_forgets_the_position(self):
        log = stamp([
            '<b>Starting Program: PokemonFRLG:AutoStory</b>',
            '[Event] 00.07/2 out: start',
            'Route: walk south 4 -> TestTown (4, 4)',
            '[Event] 00.07/2 out: done',
            '[Segment] 00: 00.07 failed, reloading for attempt 2/3',
            'Soft reset completed.',
            '[Checkpoint] 00.07: start, 17 events',
            '[Event] 00.07/3 in: start',
            'Route: walk south 1 (warp) -> TestHouse (3, 3)',
        ])
        walks = route_trace.parse(log, maps())[0].walks
        self.assertEqual(walks[1].plans[0].start, [])
        self.assertEqual(walks[1].attempt, 2)
        self.assertEqual(walks[1].outcome, 'open')


    def test_a_failed_warp_is_marked_where_the_step_started(self):
        log = stamp([
            '<b>Starting Program: PokemonFRLG:AutoStory</b>',
            '[Event] 00.07/15 gate: start',
            'Route: walk east 3 -> TestTown (7, 3)',
            'Route: walk south 1 (warp) -> TestHouse (3, 3)',
            'OperationFailedExceptionWithScreenshot: walk_overworld_route(): no fade in and out after walking onto the warp to TestHouse.',
            '[Event] 00.07/15 gate: failed: operation_failed: walk_overworld_route(): no fade',
        ])
        walk = route_trace.parse(log, maps())[0].walks[0]
        self.assertEqual(walk.steps[-1].outcome, 'failed')
        self.assertEqual([m.at for m in walk.marks if m.kind == 'failed'], [[('TestTown', 7, 3)]])


class BdspTraceTest(unittest.TestCase):
    def setUp(self):
        self.run = route_trace.parse(BDSP_LOG, maps())[0]

    def test_plan_with_continuation_lines_and_a_replan(self):
        self.assertEqual(len(self.run.walks), 1)
        walk = self.run.walks[0]
        self.assertEqual(walk.label, 'To the grass')
        self.assertEqual(walk.event, '00.04b/2 grass')
        self.assertEqual([p.reason for p in walk.plans], ['plan', 'replan'])
        self.assertEqual(len(walk.plans[0].legs), 3)
        self.assertEqual(walk.plans[1].start, [('Test Field', 6, 2)])

    def test_steps_and_what_was_seen(self):
        walk = self.run.walks[0]
        self.assertEqual([(s.leg.describe(), s.outcome) for s in walk.steps],
                         [('walk east 5 (to wall)', 'blocked'), ('walk north 2 (to wall)', 'stopped'),
                          ('walk west 4 (to wall)', 'blocked')])
        self.assertEqual(walk.steps[1].end, [('Test Field', 6, 2)])
        self.assertEqual(walk.outcome, 'done')
        self.assertEqual(sorted({m.kind for m in walk.marks}), ['arrived', 'battle', 'stop'])

    def test_json_round_trip(self):
        data = json.loads(json.dumps(route_trace.to_json([self.run])))
        self.assertEqual(data[0]['walks'][0]['steps'][0]['leg']['direction'], 'east')


class SharedLogTest(unittest.TestCase):
    """The route log lines both games' walkers now write."""

    def setUp(self):
        self.walks = route_trace.parse(SHARED_FRLG_LOG, maps())[0].walks

    def test_walks_plans_and_replans(self):
        self.assertEqual([(w.label, w.outcome) for w in self.walks], [('to_route', 'done'), ('house', 'open')])
        walk = self.walks[0]
        self.assertEqual([(p.reason, p.start) for p in walk.plans], [('plan', [('TestTown', 8, 1)]), ('replan', [])])
        self.assertEqual([l.describe() for l in self.walks[1].plans[0].legs],
                         ['walk east 3', 'walk south 1 (warp)', 'face west'])

    def test_how_each_leg_ended(self):
        walk = self.walks[0]
        self.assertEqual([(s.leg.describe(), s.basis, s.end) for s in walk.steps], [
            ('walk west 4', 'walked', [('TestTown', 4, 1)]),
            ('walk south 3', 'unknown', []),
            ('walk east 6 (to wall)', 'unknown', []),
            ('walk south 9 (to wall)', 'walked', [('TestTown', 8, 3)]),
            ('walk west 4', 'walked', [('TestTown', 4, 3)]),
            ('walk south 1', 'walked', [('TestTown', 4, 4)]),
            ('walk north 4', 'assumed', [('Route9', 4, 3)]),
        ])
        self.assertEqual(walk.steps[-1].why, 'assumed: 1 wild battle(s) on the way')
        self.assertIn(('reanchor', [('TestTown', 8, 3)]), [(m.kind, m.at) for m in walk.marks])

    def test_drawn_like_the_old_lines(self):
        paths, first, last = route_render.walked_paths(maps(), self.walks[0])
        walked = route_render.edge_counts(paths, 'TestTown')
        self.assertEqual(walked[((4, 3), (4, 4))], 1)
        self.assertEqual(walked[((4, 1), (5, 1))], 1)
        self.assertNotIn(((4, 1), (4, 2)), walked)    # the stopped leg, and the assumed walk north
        self.assertEqual(route_render.edge_counts(paths, 'TestTown', 'assumed')[((4, 3), (4, 4))], 1)
        self.assertEqual((first, last), (('TestTown', 8, 1), ('Route9', 4, 3)))


class PathTest(unittest.TestCase):
    def test_leg_on_one_map(self):
        leg = route_trace.Leg('walk', 'east', 5, True, False, [('Test Field', 6, 3), ('Test Field', 6, 2)])
        pieces, end, certain = route_render.leg_path(maps(), ('Test Field', 1, 2), leg)
        self.assertEqual(end, ('Test Field', 6, 2))
        self.assertEqual(pieces, [('Test Field', [(x, 2) for x in range(1, 7)])])
        self.assertTrue(certain)

    def test_leg_across_a_map_edge(self):
        leg = route_trace.Leg('walk', 'north', 3, False, False, [('Route9', 4, 2)])
        pieces, end, _ = route_render.leg_path(maps(), ('TestTown', 4, 1), leg)
        self.assertEqual(pieces, [('TestTown', [(4, 1), (4, 0)]), ('Route9', [(4, 3), (4, 2)])])

    def test_warp_ends_on_the_other_map(self):
        leg = route_trace.Leg('walk', 'south', 1, False, True, [('TestHouse', 3, 3)])
        pieces, end, _ = route_render.leg_path(maps(), ('TestTown', 7, 3), leg)
        self.assertEqual(pieces, [('TestTown', [(7, 3), (7, 4)]), ('TestHouse', [(3, 3)])])

    def test_unknown_start_is_unsure(self):
        leg = route_trace.Leg('walk', 'south', 9, True, False, [('TestTown', 8, 3)])
        pieces, end, certain = route_render.leg_path(maps(), None, leg)
        self.assertFalse(certain)
        self.assertEqual(pieces, [('TestTown', [(8, 0), (8, 1), (8, 2), (8, 3)])])

    def test_back_and_forth_counts_twice(self):
        walk = route_trace.parse(FRLG_LOG, maps())[0].walks[0]
        paths, first, last = route_render.walked_paths(maps(), walk)
        counts = route_render.edge_counts(paths, 'TestTown')
        #  South 1 from (4, 3) then north 4: the edge (4, 3)-(4, 4) twice.
        self.assertEqual(counts[((4, 3), (4, 4))], 2)
        self.assertEqual(counts[((4, 1), (5, 1))], 1)
        #  The stopped leg south from (4, 1) isn't counted, only the walk north to Route 9.
        self.assertEqual(counts[((4, 1), (4, 2))], 1)
        self.assertEqual(first, ('TestTown', 8, 1))
        self.assertEqual(last, ('Route9', 4, 3))


def png_size(data):
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    return struct.unpack('>II', data[16:24])


class RenderTest(unittest.TestCase):
    def test_picture_png_and_svg(self):
        pic = Picture(20, 10)
        pic.line(2, 5, 18, 5, '#ff0000', 3)
        pic.text(0, 0, 'Hi', '#000000')
        self.assertEqual(png_size(pic.png()), (20, 10))
        self.assertEqual(pic.pixel(10, 5), (255, 0, 0))
        self.assertEqual(pic.pixel(10, 9), (255, 255, 255))
        self.assertIn('<line', pic.svg())

    def test_run_writes_map_pictures_first_then_walks(self):
        run = route_trace.parse(FRLG_LOG, maps())[0]
        with tempfile.TemporaryDirectory() as d:
            pictures = route_render.render_run(run, maps(), d)
            names = [p[3] for p in pictures]
            self.assertEqual(names, ['001_00_05_2_to_route_TestTown', '001_00_05_2_to_route_Route9',
                                     '002_00_05_3_house_TestTown', '002_00_05_3_house_TestHouse'])
            overview = 'map_01_00_05_a1_TestTown'
            self.assertTrue(Path(d, overview + '.png').exists())
            png_size(Path(d, names[0] + '.png').read_bytes())
            index = Path(d, 'index.html').read_text()
            self.assertLess(index.index(overview), index.index(names[0]))
            self.assertLess(index.index(names[0]), index.index(names[2]))
            self.assertIn('failed', index)
            self.assertEqual(json.loads(Path(d, 'trace.json').read_text())[0]['run_id'], run.run_id)

    def test_attempts_group_walks_by_checkpoint_and_attempt(self):
        run = route_trace.parse(FRLG_LOG, maps())[0]
        groups = route_render.attempts(run)
        self.assertEqual([(c, a, [n for n, _ in w]) for c, a, w in groups], [('00.05', 1, [1, 2])])

    def test_view_crops_to_the_walk_and_zooms(self):
        big = route_maps.TileMap('Big', ['.' * 60] * 50, route_maps.SHARED_LEGEND)
        view = route_render.View(big, [(30, 20), (33, 20)])
        self.assertTrue(view.contains(30, 20) and view.contains(37, 20) and not view.contains(10, 20))
        self.assertEqual((view.w, view.h), (14, 10))
        self.assertEqual(view.tile, 40)  # the most it zooms

    def test_walked_edge_is_drawn_red(self):
        run = route_trace.parse(FRLG_LOG, maps())[0]
        pic = route_render.draw(run.walks[0], 'TestTown', maps()['testtown'], maps(), run.run_id, 1)
        view = route_render.View(maps()['testtown'], route_render._tiles_of(maps(), [run.walks[0]], 'TestTown'))
        x0, y = view.centre(5, 1)
        x1, _ = view.centre(6, 1)
        r, g, b = pic.pixel(int((x0 + x1) / 2), int(y + view.tile * 0.12))
        self.assertGreater(r, 200)
        self.assertLess(g, 80)

    def test_a_leg_finished_after_a_battle_is_assumed(self):
        log = stamp([
            '<b>Starting Program: PokemonFRLG:AutoStory</b>',
            '[Event] 00.05/2 grass: start',
            'Route: walk north 2 (to wall) -> TestTown (4, 1)',
            'Leg: a wild battle after 700ms, walking to the wall again.',
            'Route: walk east 3 -> TestTown (7, 1)',
            '[Event] 00.05/2 grass: done',
        ])
        walk = route_trace.parse(log, maps())[0].walks[0]
        self.assertEqual([(s.basis, s.why) for s in walk.steps],
                         [('assumed', 'assumed after battle'), ('walked', '')])
        paths, _, _ = route_render.walked_paths(maps(), walk)
        self.assertEqual(route_render.edge_counts(paths, 'TestTown', 'assumed'), {((4, 2), (4, 3)): 1, ((4, 1), (4, 2)): 1})
        pic = route_render.draw(walk, 'TestTown', maps()['testtown'], maps(), 'r', 1)
        self.assertIn('assumed after battle', pic.svg())

    def test_map_without_a_file(self):
        run = route_trace.parse(FRLG_LOG, {})[0]
        pic = route_render.draw(run.walks[0], 'TestTown', None, {}, run.run_id, 1)
        self.assertGreater(pic.width, 0)


if __name__ == '__main__':
    unittest.main()
