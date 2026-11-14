"""A fake lab-B profile folder for the Slice 2 tests: what the app writes into `Lab/`, by hand.

Everything lives in a temp dir; nothing here touches a real profile or console.
"""

import copy
import json
import os
from pathlib import Path
import tempfile

from pa_lab import timestamps

FIXTURES = Path(__file__).resolve().parent.parent / 'contracts' / 'v1'


def golden(kind, name):
    return json.loads((FIXTURES / kind / (name + '.json')).read_text(encoding='utf-8'))


class FakeClock:
    """Epoch seconds the test moves by hand; `sleep` advances it instead of waiting."""

    def __init__(self, at):
        self.t = timestamps.parse(at)
        self.on_sleep = []

    def now(self):
        return self.t

    def sleep(self, seconds):
        self.t += seconds
        for hook in list(self.on_sleep):
            hook()


class FakeLab:
    def __init__(self, test):
        self._tmp = tempfile.TemporaryDirectory(prefix='pa-lab-test-')
        test.addCleanup(self._tmp.cleanup)
        self.dir = Path(self._tmp.name) / 'Lab'
        for sub in ('inbox', 'outbox', 'done'):
            (self.dir / sub).mkdir(parents=True)
        self.state = None

    @property
    def state_path(self):
        return self.dir / 'state.json'

    @property
    def save_path(self):
        return self.dir / 'save.json'

    def write_state(self, doc):
        """Atomic, as the app does: write beside, then rename."""
        self.state = copy.deepcopy(doc)
        self._atomic(self.state_path, json.dumps(doc, indent=2) + '\n')

    def write_save(self, doc):
        self._atomic(self.save_path, json.dumps(doc, indent=2) + '\n')

    def write_raw(self, path, text):
        """A torn or hand-made file: written in place, not atomically."""
        Path(path).write_text(text, encoding='utf-8')

    def update_state(self, **changes):
        """Rewrite state.json with dotted-path changes: update_state(**{'video.frame_seq': 5})."""
        doc = copy.deepcopy(self.state)
        for dotted, value in changes.items():
            node = doc
            keys = dotted.split('.')
            for key in keys[:-1]:
                node = node[key]
            node[keys[-1]] = value
        self.write_state(doc)

    @staticmethod
    def _atomic(path, text):
        tmp = path.with_name(path.name + '.tmp')
        tmp.write_text(text, encoding='utf-8')
        os.replace(tmp, path)


PNG = b'\x89PNG\r\n\x1a\n' + b'\x00\x00\x00\rIHDR' + b'\x00' * 17 + b'\x00\x00\x00\x00IEND\xaeB`\x82'


class FakeApp:
    """The app's side of the channel, run on each FakeClock.sleep: a new frame per tick, a fresh
    heartbeat, and the inbox's `snapshot LABEL TIMEOUT_MS` requests answered as LabChannel does
    (Lab.md "Snapshot"): the request moves to done/, and the first frame newer than the line's
    start is saved as `<request>_<LABEL>.png` and named in the journal.

    Knobs: `frames` (False: the video froze), `serve` (False: the app is busy), `journal_error`
    (the line fails without a snapshot line), `unavailable` (a `snapshot unavailable` reason),
    `reply_instance` (the instance the reply names), `reply_dir` (the folder the reply's path
    names; the PNG is still saved in outbox/), `png` (the bytes the app saves).
    """

    def __init__(self, lab, clock):
        self.lab = lab
        self.clock = clock
        self.frames = True
        self.serve = True
        self.journal_error = None
        self.unavailable = None
        self.reply_instance = None
        self.reply_dir = None
        self.png = PNG
        self.requests = []
        self.pending = []
        clock.on_sleep.append(self.tick)

    def tick(self):
        now = timestamps.format(self.clock.now())
        changes = {'written_at': now}
        if self.frames:
            changes['video.frame_seq'] = (self.lab.state['video']['frame_seq'] or 0) + 1
            changes['video.state'] = 'live'
            changes['video.captured_at'] = now
        self.lab.update_state(**changes)
        for pending in list(self.pending):
            self.continue_snapshot(pending)
        if self.serve:
            for request in sorted((self.lab.dir / 'inbox').glob('*.txt')):
                self.pick_up(request)

    def pick_up(self, request):
        text = request.read_text(encoding='utf-8')
        os.replace(request, self.lab.dir / 'done' / request.name)
        self.requests.append(text)
        line = text.splitlines()[-1]
        words = line.split()
        pending = {'name': request.stem, 'line': line, 'label': words[1], 'timeout_ms': int(words[2]),
                   'started': self.clock.now(), 'after': self.lab.state['video']['frame_seq'] or 0}
        if self.journal_error is not None:
            self.finish(pending, '', self.journal_error)
        elif self.unavailable is not None:
            self.finish(pending, 'snapshot unavailable instance={} reason={}\n'.format(
                self.instance(), self.unavailable), 'unavailable: ' + self.unavailable)
        else:
            self.pending.append(pending)

    def continue_snapshot(self, pending):
        video = self.lab.state['video']
        if video['frame_seq'] is not None and video['frame_seq'] > pending['after']:
            png = self.lab.dir / 'outbox' / '{}_{}.png'.format(pending['name'], pending['label'])
            png.write_bytes(self.png)
            if self.reply_dir is not None:
                png = Path(self.reply_dir) / png.name
            self.finish(pending, 'snapshot saved instance={} frame_seq={} captured_at={} after={} path={}\n'.format(
                self.instance(), video['frame_seq'], video['captured_at'], pending['after'], png))
        elif (self.clock.now() - pending['started']) * 1000 >= pending['timeout_ms']:
            self.finish(pending, 'snapshot timeout instance={} after={} timeout_ms={}\n'.format(
                self.instance(), pending['after'], pending['timeout_ms']),
                'timeout: no frame newer than frame_seq {} within {} ms'.format(
                    pending['after'], pending['timeout_ms']))

    def instance(self):
        return self.reply_instance or self.lab.state['instance']['id']

    def finish(self, pending, printed, error=None):
        if pending in self.pending:
            self.pending.remove(pending)
        if error is None:
            journal = printed + 'ok 2 {} @0-41ms\nend\n'.format(pending['line'])
        else:
            journal = printed + 'error 2 ({}): {}\nend\n'.format(pending['line'], error)
        (self.lab.dir / 'outbox' / (pending['name'] + '.out')).write_text(journal, encoding='utf-8')
