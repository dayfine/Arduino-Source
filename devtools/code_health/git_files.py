"""Git adapter: changed first-party C++ and corresponding base-revision content."""
import subprocess
from pathlib import PurePosixPath


EXTENSIONS = {'.h', '.hpp', '.c', '.cpp', '.tpp'}


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args])


def included(path):
    parts = PurePosixPath(path).parts
    return (len(parts) > 1 and parts[0] in ('Common', 'SerialPrograms')
            and PurePosixPath(path).suffix in EXTENSIONS
            and not any(part.startswith('3rdParty') for part in parts))


def changed_files(root, base_ref):
    fields = iter(git(root, 'diff', '--name-status', '-z', '--find-renames', base_ref, '--').decode().split('\0'))
    changes = []
    for status in fields:
        if not status:
            continue
        original = next(fields)
        current = next(fields) if status[0] in 'RC' else original
        if status[0] != 'D' and included(current):
            previous = original if status[0] not in 'AC' and included(original) else None
            changes.append((current, previous))
    return changes


def read_revision(root, revision, path):
    return git(root, 'show', revision + ':' + path).decode('utf-8-sig')
