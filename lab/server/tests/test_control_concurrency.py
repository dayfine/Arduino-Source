"""Concurrent clients and crashed daemons on one control database (Slice 3, fork #131): one lease
winner, one result per request key, and nothing half-written after a kill.

Threads each open their own Database (their own SQLite connection), as separate daemons would;
the process tests start real Python processes on the same temp file.
"""

import json
from pathlib import Path
import subprocess
import sys
import threading
import unittest

from pa_lab import control, db
from tests.control_fixtures import acquire, clock, new_id, open_plane, temp_db_path

SERVER_DIR = Path(__file__).resolve().parent.parent
RACERS = 8


def race(count, work):
    """Runs `work(i)` on `count` threads released together; returns results in thread order."""
    barrier = threading.Barrier(count)
    results = [None] * count

    def run(i):
        barrier.wait(10)
        try:
            results[i] = work(i)
        except Exception as e:  # surfaced to the test as a value
            results[i] = e

    threads = [threading.Thread(target=run, args=(i,)) for i in range(count)]
    for t in threads:
        t.start()
    for t in threads:
        t.join(30)
    return results


def run_python(code, *args):
    return subprocess.run([sys.executable, '-c', code, *[str(a) for a in args]], cwd=str(SERVER_DIR),
                          capture_output=True, text=True, timeout=60)


ACQUIRE_IN_A_PROCESS = """
import json, sys
from pa_lab import control, db
plane = control.ControlPlane(db.Database(sys.argv[1]), ['B'])
request = {'schema': 1, 'request_id': sys.argv[2], 'tool': 'acquire_console', 'console': 'B',
           'arguments': {'owner': sys.argv[3]}}
print(json.dumps(plane.execute(sys.argv[3], request)))
"""

CRASH_MID_WRITE = """
import os, sys
from pa_lab import control, db
plane = control.ControlPlane(db.Database(sys.argv[1]), ['B'])
with plane.db.write() as conn:
    conn.execute("UPDATE leases SET state = 'agent_idle', owner = 'half', acquired_at = '2026-11-14T06:00:00.000Z', "
                 "expires_at = '2026-11-14T07:00:00.000Z' WHERE console = 'B'")
    conn.execute("INSERT INTO events (type, at, console, payload) VALUES ('stopped', '2026-11-14T06:00:00.000Z', 'B', '{}')")
    os._exit(9)
"""


class RaceTests(unittest.TestCase):
    def setUp(self):
        self.clock = clock()
        self.path = temp_db_path(self)
        self.plane = open_plane(self, self.path, self.clock)

    def planes(self, count):
        return [open_plane(self, self.path, self.clock) for _ in range(count)]

    def test_concurrent_acquires_have_one_winner(self):
        planes = self.planes(RACERS)
        records = race(RACERS, lambda i: planes[i].execute('claude:lane{}'.format(i), acquire('agent:lane{}'.format(i))))
        statuses = sorted(r['status'] for r in records)
        self.assertEqual(statuses, ['completed'] + ['refused'] * (RACERS - 1))
        winner = next(r for r in records if r['status'] == 'completed')
        self.assertEqual(self.plane.lease('B')['owner'], winner['arguments']['owner'])
        self.assertEqual({r['refusal']['code'] for r in records if r['refusal']}, {'console_held_by_agent'})

    def test_concurrent_retries_of_one_request_get_one_result(self):
        planes = self.planes(RACERS)
        request = acquire()
        records = race(RACERS, lambda i: planes[i].execute('claude:frlg-lane', request))
        self.assertTrue(all(r == records[0] for r in records), records)
        page = self.plane.next_events('reader', 'B', 0)
        self.assertEqual([e['type'] for e in page['events']], ['lease_changed', 'request_completed'])

    def test_acquires_racing_an_expiry_have_one_winner_and_one_expiry(self):
        self.plane.execute('claude:old', acquire('agent:old'))
        self.clock.sleep(1200)
        planes = self.planes(RACERS)
        records = race(RACERS, lambda i: planes[i].execute('claude:lane{}'.format(i), acquire('agent:lane{}'.format(i))))
        self.assertEqual([r['status'] for r in records].count('completed'), 1)
        reasons = [e['payload'].get('reason') for e in self.plane.next_events('reader', 'B', 0)['events']
                   if e['type'] == 'lease_changed']
        self.assertEqual(reasons, ['acquired', 'expired', 'acquired'])

    def test_concurrent_readers_see_every_event_once_in_order(self):
        planes = self.planes(4)

        def read_all(i):
            seen, cursor = [], 0
            for _ in range(200):
                page = planes[i].next_events('reader{}'.format(i), 'B', cursor, timeout_s=0.5, limit=2)
                seen.extend(e['seq'] for e in page['events'])
                cursor = page['next_after_seq']
                if len(seen) >= 2 + RACERS - 1:
                    break
            return seen

        readers = []
        results = {}
        for i in range(4):
            t = threading.Thread(target=lambda i=i: results.update({i: read_all(i)}))
            t.start()
            readers.append(t)
        writers = self.planes(RACERS)
        race(RACERS, lambda i: writers[i].execute('claude:lane{}'.format(i), acquire('agent:lane{}'.format(i))))
        for t in readers:
            t.join(30)
        expected = list(range(1, 2 + RACERS))
        self.assertEqual(results, {i: expected for i in range(4)})


class ProcessTests(unittest.TestCase):
    def setUp(self):
        self.path = temp_db_path(self)
        db.Database(self.path).close()

    def test_racing_processes_have_one_winner(self):
        procs = [subprocess.Popen([sys.executable, '-c', ACQUIRE_IN_A_PROCESS, str(self.path), new_id(),
                                   'agent:p{}'.format(i)], cwd=str(SERVER_DIR), stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True) for i in range(4)]
        outputs = [p.communicate(timeout=60) for p in procs]
        self.assertEqual([p.returncode for p in procs], [0] * 4, [err for _, err in outputs])
        statuses = sorted(json.loads(out)['status'] for out, _ in outputs)
        self.assertEqual(statuses, ['completed', 'refused', 'refused', 'refused'])

    def test_a_retry_from_another_process_gets_the_stored_result(self):
        request_id = new_id()
        first = run_python(ACQUIRE_IN_A_PROCESS, self.path, request_id, 'agent:p')
        again = run_python(ACQUIRE_IN_A_PROCESS, self.path, request_id, 'agent:p')
        self.assertEqual(json.loads(first.stdout), json.loads(again.stdout))
        self.assertEqual(json.loads(first.stdout)['status'], 'completed')

    def test_a_daemon_killed_mid_write_leaves_nothing(self):
        crashed = run_python(CRASH_MID_WRITE, self.path)
        self.assertEqual(crashed.returncode, 9, crashed.stderr)
        plane = control.ControlPlane(db.Database(self.path), ['B'])
        self.addCleanup(plane.db.close)
        self.assertEqual(plane.lease('B')['state'], 'free')
        self.assertEqual(plane.next_events('reader', 'B', 0)['events'], [])

    def test_a_lease_from_a_killed_daemon_is_still_held(self):
        out = run_python(ACQUIRE_IN_A_PROCESS + '\nimport os\nos._exit(9)\n', self.path, new_id(), 'agent:p')
        self.assertEqual(out.returncode, 9, out.stderr)
        plane = control.ControlPlane(db.Database(self.path), ['B'])
        self.addCleanup(plane.db.close)
        self.assertEqual(plane.lease('B')['owner'], 'agent:p')
        record = plane.execute('claude:other', acquire('agent:other'))
        self.assertEqual(record['refusal']['code'], 'console_held_by_agent')


if __name__ == '__main__':
    unittest.main()
