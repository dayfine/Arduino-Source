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
    heartbeat, and the inbox's `shot` requests answered as LabChannel does (done/, png, journal).

    Knobs: `frames` (False: the video froze), `serve` (False: the app is busy), `journal_error`,
    `no_frame`, `png` (the bytes the app saves).
    """

    def __init__(self, lab, clock):
        self.lab = lab
        self.clock = clock
        self.frames = True
        self.serve = True
        self.journal_error = None
        self.no_frame = False
        self.png = PNG
        self.requests = []
        clock.on_sleep.append(self.tick)

    def tick(self):
        now = timestamps.format(self.clock.now())
        changes = {'written_at': now}
        if self.frames:
            changes['video.frame_seq'] = self.lab.state['video']['frame_seq'] + 1
            changes['video.captured_at'] = now
        self.lab.update_state(**changes)
        if self.serve:
            for request in sorted((self.lab.dir / 'inbox').glob('*.txt')):
                self.answer(request)

    def answer(self, request):
        name = request.stem
        text = request.read_text(encoding='utf-8')
        os.replace(request, self.lab.dir / 'done' / request.name)
        self.requests.append(text)
        outbox = self.lab.dir / 'outbox'
        if self.journal_error is not None:
            journal = 'error 2 (shot frame): {}\nend\n'.format(self.journal_error)
        elif self.no_frame:
            journal = 'shot (no video frame)\nok 2 shot frame @0-3ms\nend\n'
        else:
            png = outbox / '{}_frame.png'.format(name)
            png.write_bytes(self.png)
            journal = 'shot {}\nok 2 shot frame @0-41ms\nend\n'.format(png)
        (outbox / (name + '.out')).write_text(journal, encoding='utf-8')
