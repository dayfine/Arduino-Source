#!/usr/bin/env python3
"""Merge a feature into a configured dev slot, including an amended replacement.

  python3 lab/scripts/sync_dev.py B bdsp/my-fix
  python3 lab/scripts/sync_dev.py B bdsp/my-fix --force

--force is a checked feature replacement, not Git's blind conflict preference.
It retains integration-only files and backs up a matching pending merge first.
It never builds, launches, pushes, rebases or force-updates a branch.
"""

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import sys

from start_dev import DEFAULT_CONFIG, DEVICE_GUARD_REVISION, build_lock, console_lock, load_config


def git(repo, *args, check=True):
    """Run Git directly; paths are literal and failed commands retain their diagnostics."""
    result = subprocess.run(['git', '-C', str(repo), '--literal-pathspecs', *args], capture_output=True)
    if check and result.returncode:
        raise ValueError((result.stderr or result.stdout).decode(errors='replace').strip()
                         or 'git ' + args[0] + ' failed (exit ' + str(result.returncode) + ')')
    return result


def output(repo, *args):
    """Read newline-terminated Git output without invoking a shell."""
    return git(repo, *args).stdout.decode().rstrip('\r\n')


def pending_merge(repo):
    """Return the pending merge heads, or an empty list when not merging."""
    result = git(repo, 'rev-parse', '-q', '--verify', 'MERGE_HEAD', check=False)
    return result.stdout.decode().split() if result.returncode == 0 else []


def identifies_feature(message, feature):
    """Match our trailer or Git's exact quoted branch name, not a substring."""
    subject = message.splitlines()[0] if message else ''
    return ('Dev-Feature: ' + feature) in message.splitlines() or any(
        subject.startswith(prefix + "'" + feature + "'")
        for prefix in ('Merge branch ', 'Merge remote-tracking branch ')
    )


def previous_feature(repo, feature):
    """Find the previous tip from our trailer or Git's ordinary two-parent merge message."""
    log = git(repo, 'log', '--first-parent', '--merges', '--format=%P%x00%B%x00').stdout
    fields = log.split(b'\0')
    for index in range(0, len(fields) - 1, 2):
        parents = fields[index].decode().split()
        message = fields[index + 1].decode(errors='replace')
        if len(parents) != 2:
            continue
        if identifies_feature(message, feature):
            return parents[1]
    raise ValueError('Cannot identify the previously merged feature; provide --previous <commit>.')


def tree_entry(repo, revision, path):
    """Compare both object identity and file mode, including an absent path."""
    item = git(repo, 'ls-tree', '-z', revision, '--', path).stdout
    return item.split(b'\t', 1)[0] if item else b''


def replacement_paths(repo, before, incoming):
    """Only replace paths whose integration copy is unchanged from the old feature."""
    if git(repo, 'merge-base', '--is-ancestor', before, 'HEAD', check=False).returncode:
        raise ValueError('The previous feature is not part of this integration history.')
    names = git(repo, 'diff', '--no-renames', '--name-only', '-z', before, incoming).stdout
    paths = []
    for raw in filter(None, names.split(b'\0')):
        path = raw.decode()
        current = tree_entry(repo, 'HEAD', path)
        old = tree_entry(repo, before, path)
        new = tree_entry(repo, incoming, path)
        if current == new:
            continue
        if current != old:
            raise ValueError('Refusing to overwrite integration changes in ' + path)
        paths.append(path)
    return paths


def backup_merge(repo, incoming):
    """Preserve conflict/index/worktree evidence outside tracked source before aborting."""
    common = Path(output(repo, 'rev-parse', '--git-common-dir'))
    if not common.is_absolute():
        common = repo / common
    name = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ') + '-' + secrets.token_hex(4)
    backup = common.resolve() / 'dev-feature-sync-backups' / name
    backup.mkdir(parents=True)
    index = Path(output(repo, 'rev-parse', '--git-path', 'index'))
    if not index.is_absolute():
        index = repo / index
    shutil.copy2(index, backup / 'index')
    names = git(repo, 'diff', '--name-only', '-z', 'HEAD').stdout
    for raw in filter(None, names.split(b'\0')):
        path = raw.decode()
        source = repo / path
        if source.exists() or source.is_symlink():
            destination = backup / 'files' / path
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination, follow_symlinks=False)
    manifest = {
        'head': output(repo, 'rev-parse', 'HEAD'), 'incoming': incoming,
        'pending_heads': pending_merge(repo),
        'status': output(repo, 'status', '--porcelain'),
        'unmerged': output(repo, 'ls-files', '--unmerged'),
    }
    (backup / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (backup / 'worktree.patch').write_bytes(git(repo, 'diff', '--binary').stdout)
    (backup / 'index.patch').write_bytes(git(repo, 'diff', '--cached', '--binary').stdout)
    return backup


def require_clean(repo):
    """Never discard unrelated staged, unstaged or untracked work."""
    if output(repo, 'status', '--porcelain', '--untracked-files=all'):
        raise ValueError('Integration worktree is dirty; preserve unrelated work before syncing.')


def sync_feature(repo, feature, force=False, previous=None):
    """Normal merge, or a history-preserving replacement of an already merged feature."""
    repo = Path(repo)
    incoming = output(repo, 'rev-parse', '--verify', feature + '^{commit}')
    if git(repo, 'cat-file', '-e', incoming + ':SerialPrograms/CMakeLists.txt', check=False).returncode:
        raise ValueError('Select a code feature, not the agents/notes branch.')
    pending = pending_merge(repo)
    if pending and pending != [incoming]:
        message_path = Path(output(repo, 'rev-parse', '--git-path', 'MERGE_MSG'))
        if not message_path.is_absolute():
            message_path = repo / message_path
        message = message_path.read_text() if message_path.exists() else ''
        if len(pending) != 1 or not identifies_feature(message, feature):
            raise ValueError('A different merge is pending; it was left untouched.')
    if not pending:
        require_clean(repo)
        if git(repo, 'merge-base', '--is-ancestor', incoming, 'HEAD', check=False).returncode == 0:
            return None
    if not force:
        if pending:
            raise ValueError('A matching merge is conflicted; resolve it or retry with --force.')
        git(repo, 'merge', '--no-edit', feature)
        return None
    old = output(repo, 'rev-parse', '--verify', previous + '^{commit}') if previous else previous_feature(repo, feature)
    paths = replacement_paths(repo, old, incoming)
    backup = None
    if pending:
        backup = backup_merge(repo, incoming)
        print('Pending merge preserved at ' + str(backup), flush=True)
        git(repo, 'merge', '--abort')
    require_clean(repo)
    # The tree delta is validated above; ours only records the new feature ancestry.
    git(repo, 'merge', '--no-ff', '--no-commit', '-s', 'ours', incoming)
    for path in paths:
        git(repo, 'restore', '--source=' + incoming, '--staged', '--worktree', '--', path)
    message = (
        'Sync ' + feature + ' into ' + output(repo, 'branch', '--show-current') + '\n\n'
        'Dev-Feature: ' + feature + '\nDev-Feature-Head: ' + incoming + '\n'
        'Dev-Feature-Previous: ' + old + '\n'
    )
    # Keep any failure state for review; commit hooks are not bypassed.
    git(repo, 'commit', '-m', message)
    return backup


def main(argv=None):
    """Acquire the launcher's console/build locks; update only its configured lab branch."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('console', choices=['A', 'B'])
    parser.add_argument('feature')
    parser.add_argument('--config', type=Path, default=DEFAULT_CONFIG)
    parser.add_argument('--force', action='store_true')
    parser.add_argument('--previous', help='Explicit previous feature tip if automatic detection is unavailable')
    args = parser.parse_args(argv)
    try:
        slot = load_config(args.config, args.console)
        repo = Path(slot['worktree'])
        lock_root = Path(os.environ.get('LAB_DIR', str(Path.home() / '.lab')))
        with console_lock(args.console, lock_root):
            with build_lock(lock_root):
                if Path(output(repo, 'rev-parse', '--show-toplevel')).resolve() != repo.resolve():
                    raise ValueError('Configure the root of the integration worktree.')
                if output(repo, 'branch', '--show-current') != slot['branch']:
                    raise ValueError('The integration worktree is not on its configured lab branch.')
                if args.feature.startswith('lab/') or args.feature in ('agents', 'HEAD'):
                    raise ValueError('Select a feature branch, not lab integration or agent notes.')
                git(repo, 'merge-base', '--is-ancestor', DEVICE_GUARD_REVISION, 'HEAD')
                git(repo, 'merge-base', '--is-ancestor', 'origin/lab/main', 'HEAD')
                sync_feature(repo, args.feature, args.force, args.previous)
                print('Synced ' + args.feature + ' into ' + slot['branch'] + ' at ' + output(repo, 'rev-parse', '--short', 'HEAD'))
    except (OSError, ValueError) as error:
        print('Dev sync refused: ' + str(error), file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
