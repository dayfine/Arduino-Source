"""Shared setup for the Slice 3 control-plane tests: a temp SQLite file and request builders."""

import copy
from pathlib import Path
import tempfile
import uuid

from pa_lab import control, db
from tests.fake_lab import FakeClock, golden

START = '2026-11-14T06:00:00.000Z'


def temp_db_path(test) -> Path:
    tmp = tempfile.TemporaryDirectory(prefix='pa-lab-control-')
    test.addCleanup(tmp.cleanup)
    return Path(tmp.name) / 'state' / 'control.sqlite'


def open_plane(test, path, clock, retention=None):
    database = db.Database(path)
    test.addCleanup(database.close)
    return control.ControlPlane(database, ['B'], now=clock.now, retention=retention)


def new_id():
    return str(uuid.uuid4())


def acquire(owner='agent:frlg-lane', request_id=None, **arguments):
    return {'schema': 1, 'request_id': request_id or new_id(), 'tool': 'acquire_console', 'console': 'B',
            'arguments': dict({'owner': owner}, **arguments)}


def renew(token, request_id=None, **arguments):
    return {'schema': 1, 'request_id': request_id or new_id(), 'tool': 'renew_console', 'console': 'B',
            'lease_token': token, 'arguments': arguments}


def release(token, request_id=None):
    return {'schema': 1, 'request_id': request_id or new_id(), 'tool': 'release_console', 'console': 'B',
            'lease_token': token, 'arguments': {}}


def app_state(transition=None, state='stopped', instance=None):
    """The running_lab golden with this program state, last transition and instance id."""
    doc = copy.deepcopy(golden('app_state', 'running_lab'))
    doc['program']['state'] = state
    doc['program']['last_transition'] = transition
    if instance is not None:
        doc['instance']['id'] = instance
    return doc


def transition(action, source, at, request_id=None):
    return {'action': action, 'source': source, 'request_id': request_id, 'at': at}


def clock():
    return FakeClock(START)
