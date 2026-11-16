"""An in-memory app channel: the core's tests drive actions and queries without any files.

`FakeChannel` keeps the app's state document in memory and answers each request at once with
`answer(commands, request_id)`, a function the test sets: it returns a `Journal` (or None to
leave the request taken and unanswered) and may change `state` the way the app would.
"""

import copy
import json

from pa_lab import timestamps
from pa_lab.channel import AppChannel, Journal


class FakeChannel(AppChannel):
    state_label = 'fake state'
    save_label = 'fake save'

    def __init__(self, state, clock):
        self.state = copy.deepcopy(state)
        self.clock = clock
        self.answer_with = None
        self.sent = []
        self.answers = {}
        self.inbox = set()
        self._serial = 0

    def update(self, **changes):
        """Dotted-path changes to the state, stamped `written_at` now, as the app's timer writes it."""
        for dotted, value in dict(changes, written_at=timestamps.format(self.clock.now())).items():
            node = self.state
            keys = dotted.split('.')
            for key in keys[:-1]:
                node = node[key]
            node[keys[-1]] = value

    def request_name(self, label, request_id, unique=False):
        self._serial += 1
        return '{}:{}:{}'.format(label, request_id, self._serial if unique else 0)

    def send(self, name, request_id, commands):
        self.sent.append((name, request_id, tuple(commands)))
        journal = self.answer_with(tuple(commands), request_id) if self.answer_with is not None else None
        if journal is None:
            self.inbox.add(name)
        else:
            self.answers[name] = journal

    def answer(self, name):
        return self.answers.get(name)

    def taken(self, name):
        return name in self.answers

    def withdraw(self, name):
        if name in self.inbox:
            self.inbox.remove(name)
            return True
        return False

    def read_state(self, limit):
        return json.dumps(self.state).encode('utf-8')

    def read_save(self, limit):
        return None

    def snapshot_path(self, name, label):
        return '/nonexistent/{}_{}.png'.format(name, label)


def journal(*commands, error=None, printed=()):
    """A finished answer in which `commands` ran (and `error` failed the next one)."""
    return Journal(lines=('fake',), succeeded=frozenset(commands), error=error, printed=tuple(printed))
