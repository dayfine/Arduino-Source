import contextlib
import io
import json
import os
import tempfile
import unittest

from devtools.maps import leg_trace
from devtools.maps.leg_trace import DRIFT, LARGE, MASKED, OK

GOLDEN = os.path.join(os.path.dirname(__file__), '..', '..', '..', 'SerialPrograms', 'Source', 'PokemonBDSP',
                      'Programs', 'AutoStory', 'Simulation', 'leg_trace_00.04.golden.json')


def leg(event, number, direction, tiles, kind, end, known=None, ms=1000, interrupts=(), checkpoint='00.04', **extra):
    record = {'checkpoint': checkpoint, 'event': event, 'leg': number, 'dir': direction, 'tiles': tiles,
              'end_kind': kind, 'planned_end': end, 'chunks': tiles, 'ms': ms, 'end': end,
              'known': known or ('wall' if kind == 'wall' else 'dead_reckoned'), 'interrupts': list(interrupts)}
    record.update(extra)
    return record


def at(x, y, map_name='Route 201'):
    return [[map_name, x, y]]


# The simulation's walk of one event: west to a wall, a tile south, west in
# the open, north to a wall.
SIM = {'legs': [
    leg('to_ledge_column', 1, 'west', 7, 'wall', at(56, 4)),
    leg('to_ledge_column', 2, 'south', 1, 'open', at(56, 5)),
    leg('to_ledge_column', 3, 'west', 5, 'open', at(51, 5)),
    leg('to_ledge_column', 4, 'north', 3, 'wall', at(51, 2)),
]}


def console(*changes):
    """SIM's legs with `changes` (index, {field: value}) applied."""
    legs = json.loads(json.dumps(SIM['legs']))
    for index, change in changes:
        legs[index].update(change)
    return {'legs': legs}


def grades(report):
    return [(g.console, g.sim, g.grade) for g in report.grades]


class ReplayTest(unittest.TestCase):
    def test_records_are_read_back(self):
        lines = [
            '2026-11-09 18:41:00.1 - [Console 0]: [Route] Turning north.',
            '2026-11-09 18:41:00.2 - [Console 0]: ' + leg_trace.LEG_TAG + json.dumps(SIM['legs'][0]),
            '2026-11-09 18:41:00.3 - [Console 0]: ' + leg_trace.LEG_TAG + json.dumps(SIM['legs'][1]),
        ]
        self.assertEqual(leg_trace.replay(lines), {'legs': [dict(x, attempt=1) for x in SIM['legs'][:2]]})

    def test_route_lines_without_records(self):
        lines = '''
[Program]: [Event] 00.04/4 to_the_signs_row: start
[Console 0]: [Route] Turning north.
[Console 0]: [00.04] to_the_signs_row: taps north moved 1 of 1 tiles.
[Program]: [Event] 00.04/5 to_ledge_column: start
[Console 0]: [Route] to ledge column: plan from Route 201 (57, 4), Route 201 (58, 4): west 7 (to wall) to Route 201 (56, 4); south 1 (open) to Route 201 (56, 5); west 2 (to wall) to Route 201 (54, 5)
[Console 0]: [Route] Leg 1/3: west 7 (to wall) to Route 201 (56, 4)
[Console 0]: [Walk] to ledge column: west 7 (to wall): wild battle; running.
[Console 0]: [Route] to ledge column: west 7 (to wall): ended at Route 201 (56, 4) (walked).
[Console 0]: [Route] Leg 2/3: south 1 (open) to Route 201 (56, 5)
[Console 0]: [Walk] to ledge column: south 1 (open): blocked after 800ms, 0 moved chunks, 0 wild battle(s).
[Console 0]: [Route] to ledge column: south 1 (open): blocked after 0 of 1 tiles; waiting for the way to clear.
[Console 0]: [Route] to ledge column: south 1 (open): blocked after 0 of 1, at Route 201 (56, 4); planning again.
[Program]: [Event] 00.04/18 into_moms_call: start
[Console 0]: [00.04] into_moms_call: ran west to a dialogue: 2 moved chunks, 1 tiles at most, dialog, 0 wild battle(s).
'''.splitlines()
        legs = leg_trace.replay(lines)['legs']
        self.assertEqual([(x['event'], x['leg'], x['dir'], x['tiles'], x['end_kind'], x['known']) for x in legs], [
            ('to_the_signs_row', 1, 'north', 1, 'tile', 'counted'),
            ('to_ledge_column', 1, 'west', 7, 'wall', 'wall'),
            ('to_ledge_column', 2, 'south', 1, 'open', 'seen'),
            ('into_moms_call', 1, 'west', 1, 'open', 'dialogue'),
        ])
        self.assertEqual(legs[0]['moved'], 1)
        self.assertEqual(legs[1]['interrupts'], ['wild_battle'])
        self.assertEqual(legs[1]['end'], at(56, 4))
        self.assertEqual(legs[2]['note'], 'replan')
        self.assertEqual((legs[2]['interrupts'], legs[2]['ms'], legs[2]['moved']), (['blocked'], 800, 0))
        self.assertEqual(legs[2]['checkpoint'], '00.04')


    def test_runs_of_a_checkpoint_are_told_apart(self):
        record = json.dumps(SIM['legs'][0])
        lines = ['[Checkpoint] 00.04: start, 23 events', leg_trace.LEG_TAG + record,
                 '[Checkpoint] 00.04: start, 23 events', leg_trace.LEG_TAG + record, leg_trace.LEG_TAG + record]
        self.assertEqual([x['attempt'] for x in leg_trace.replay(lines)['legs']], [1, 2, 2])
        self.assertEqual(len(leg_trace.replay(lines, attempt=1)['legs']), 1)
        self.assertEqual(len(leg_trace.replay(lines, attempt=-1)['legs']), 2)


class DiffTest(unittest.TestCase):
    def test_same_route_is_ok(self):
        report = leg_trace.diff(console(), SIM)
        self.assertEqual([g.grade for g in report.grades], [OK] * 4)
        self.assertFalse(report.large())
        self.assertEqual(report.summary(), ['00.04: 4 legs: 4 OK, 0 drift, 0 LARGE, 0 masked'])

    def test_open_leg_inside_its_band_is_ok(self):
        report = leg_trace.diff(console((2, {'end': at(49, 5)})), SIM)
        self.assertEqual([g.grade for g in report.grades], [OK] * 4)

    def test_open_leg_off_then_a_wall_resyncs_is_drift(self):
        report = leg_trace.diff(console((2, {'end': at(46, 5)})), SIM)
        self.assertEqual([g.grade for g in report.grades], [OK, OK, DRIFT, OK])
        self.assertFalse(report.large())
        self.assertIn('re-synced at leg 4', report.grades[2].why)

    def test_open_leg_taps_short_then_a_wall_resyncs_is_drift(self):
        report = leg_trace.diff(console((2, {'moved': 1}), (2, {'moved': 5})), {'legs': console((2, {'moved': 5}))['legs']})
        self.assertEqual([g.grade for g in report.grades], [OK] * 4)
        short = console((2, {'moved': 1}))
        sim = console((2, {'moved': 5}))
        report = leg_trace.diff(short, sim)
        self.assertEqual([g.grade for g in report.grades], [OK, OK, DRIFT, OK])
        self.assertIn('moved 1 of 5 tiles, the simulation 5', report.grades[2].why)

    def test_wall_leg_off_never_resynced_is_large(self):
        report = leg_trace.diff(console((3, {'end': at(51, 3)})), SIM)
        self.assertEqual([g.grade for g in report.grades], [OK, OK, OK, LARGE])
        self.assertTrue(report.large())
        self.assertEqual(report.first_divergence().console, 4)

    def test_different_map_is_large(self):
        report = leg_trace.diff(console((0, {'end': at(56, 4, 'Twinleaf Town')})), SIM)
        self.assertEqual(report.grades[0].grade, LARGE)

    def test_interruption_mid_leg_is_masked_and_resyncs(self):
        # A wild battle on the open leg west: the console replans from where
        # it is (two legs) and joins the simulation's route at the wall.
        legs = json.loads(json.dumps(SIM['legs'][:2]))
        legs += [
            leg('to_ledge_column', 3, 'west', 5, 'open', at(53, 5), interrupts=['wild_battle'], known='seen', note='replan'),
            leg('to_ledge_column', 4, 'west', 2, 'open', at(51, 5)),
            leg('to_ledge_column', 5, 'north', 3, 'wall', at(51, 2)),
        ]
        report = leg_trace.diff({'legs': legs}, SIM)
        self.assertEqual(grades(report), [(1, 1, OK), (2, 2, OK), (3, 3, MASKED), (4, 3, MASKED), (5, 4, OK)])
        self.assertFalse(report.large())

    def test_interruption_never_resynced_masks_the_rest(self):
        legs = json.loads(json.dumps(SIM['legs'][:2]))
        legs += [leg('to_ledge_column', 3, 'west', 5, 'open', at(53, 5), interrupts=['trainer'])]
        legs += [leg('to_ledge_column', 4, 'north', 9, 'wall', at(53, 0))]
        report = leg_trace.diff({'legs': legs}, SIM)
        self.assertEqual([g.grade for g in report.grades], [OK, OK, MASKED, MASKED])
        self.assertFalse(report.large())

    def test_replan_that_resyncs_is_drift(self):
        legs = json.loads(json.dumps(SIM['legs'][:2]))
        legs += [
            leg('to_ledge_column', 3, 'west', 5, 'open', at(51, 5), known='seen', note='replan'),
            leg('to_ledge_column', 4, 'north', 1, 'wall', at(51, 4)),
            leg('to_ledge_column', 5, 'north', 2, 'wall', at(51, 2)),
        ]
        report = leg_trace.diff({'legs': legs}, SIM)
        self.assertEqual(grades(report), [(1, 1, OK), (2, 2, OK), (3, 3, OK), (4, 0, DRIFT), (5, 4, OK)])
        self.assertFalse(report.large())

    def test_event_without_leg_lines_in_an_old_log_is_masked(self):
        old = {'legs': SIM['legs'][:1], 'events': [['00.04', 'to_ledge_column', 1], ['00.04', 'into_the_east_corner', 1]]}
        sim = {'legs': SIM['legs'][:1] + [leg('into_the_east_corner', 1, 'north', 4, 'wall', [])]}
        report = leg_trace.diff(old, sim)
        self.assertEqual([g.grade for g in report.grades], [OK, MASKED])

    def test_missing_and_extra_legs_are_large(self):
        report = leg_trace.diff({'legs': SIM['legs'][:3]}, SIM)
        self.assertEqual(grades(report)[-1], (0, 4, LARGE))
        extra = {'legs': SIM['legs'] + [leg('to_ledge_column', 5, 'east', 1, 'open', at(52, 2))]}
        self.assertEqual(grades(leg_trace.diff(extra, SIM))[-1], (5, 0, LARGE))

    def test_different_plan_is_large(self):
        report = leg_trace.diff(console((1, {'tiles': 2})), SIM)
        self.assertEqual(report.grades[1].grade, LARGE)

    def test_event_the_simulation_lacks_is_large(self):
        extra = {'legs': SIM['legs'] + [leg('to_ledge_column_recovery', 1, 'east', 1, 'open', at(52, 2))]}
        report = leg_trace.diff(extra, SIM)
        self.assertTrue(report.large())
        self.assertIn('to_ledge_column_recovery', report.summary()[0])

    def test_recovery_loop_is_large(self):
        loop = [leg('to_ledge_column', 5 + i, 'east' if i % 2 else 'west', 1, 'open', at(51, 5), interrupts=['wild_battle'])
                for i in range(leg_trace.LOOP_EXTRA_LEGS + 1)]
        report = leg_trace.diff({'legs': SIM['legs'] + loop}, SIM)
        self.assertTrue(any('recovery loop' in f[2] for f in report.findings))

    def test_slow_checkpoint_is_large(self):
        slow = console(*[(i, {'ms': 3000}) for i in range(4)])
        report = leg_trace.diff(slow, SIM)
        self.assertTrue(report.large())
        self.assertIn('walked 12000 ms, the simulation 4000 ms', report.summary()[0])

    def test_slow_checkpoint_within_the_ratio_is_ok(self):
        report = leg_trace.diff(console(*[(i, {'ms': 1400}) for i in range(4)]), SIM)
        self.assertFalse(report.large())


class GoldenTest(unittest.TestCase):
    @unittest.skipUnless(os.path.exists(GOLDEN), 'the BDSP golden trace is not on this branch')
    def test_golden_trace_against_itself(self):
        with open(GOLDEN, encoding='utf-8') as f:
            sim = json.load(f)
        self.assertTrue(sim['legs'])
        self.assertFalse(leg_trace.diff(sim, sim).large())

    def test_cli_exit_code_is_large(self):
        with tempfile.TemporaryDirectory() as folder:
            off = os.path.join(folder, 'console.json')
            same = os.path.join(folder, 'same.json')
            sim = os.path.join(folder, 'sim.json')
            for path, trace in ((off, console((3, {'end': at(51, 3)}))), (same, console()), (sim, SIM)):
                with open(path, 'w', encoding='utf-8') as f:
                    json.dump(trace, f)
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(leg_trace.main(['diff', off, sim]), 1)
                self.assertEqual(leg_trace.main(['diff', same, sim]), 0)


if __name__ == '__main__':
    unittest.main()
