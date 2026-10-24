"""Git adapter: tracked first-party C++ and base-revision content."""
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


def tracked(root, revision=None):
    args = ['ls-tree', '-r', '--name-only', '-z', revision] if revision else ['ls-files', '-z']
    paths = git(root, *args).decode('utf-8').split('\0')
    return sorted(path for path in paths if path and included(path))


def read_revision(root, revision, path):
    return git(root, 'show', revision + ':' + path).decode('utf-8-sig')
