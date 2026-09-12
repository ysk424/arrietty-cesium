"""Audit the entire index or all reachable Git history; never print private matches."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PRIVATE_DIRS = {'thirdparty', 'logs', 'saved', 'content', 'artifacts', '.venv',
                'plugins', 'cache', 'binaries', 'intermediate', 'deriveddatacache', '.vs', '.runtime'}
PRIVATE_SUFFIXES = {'.dll', '.exe', '.uasset', '.umap', '.bin', '.blend', '.fbx',
                    '.glb', '.pdf', '.csv', '.wav', '.wava', '.pkf', '.bundle',
                    '.pem', '.key', '.pdb', '.log'}
SECRET_PATTERN = re.compile(
    r'eyJ[A-Za-z0-9_-]{20,}\.[A-Za-z0-9_-]+\.|'
    r'sk-(?:proj-)?[A-Za-z0-9_-]{20,}|'
    r'(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,})|'
    r'LHR-[0-9A-Fa-f]{8}|(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}|'
    r'[Cc]:[\\/]Users[\\/]|/[U]sers/|/home/[A-Za-z0-9_-]+/')


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT)


def private_values():
    values = []
    for filename in ('config/row.local.json', 'config/fly.local.json', 'config/cesium.local.json'):
        path = ROOT / filename
        if path.exists():
            config = json.loads(path.read_text(encoding='utf-8-sig'))
            # Known input-mode names are public protocol choices, not device IDs.
            # Keep addresses, serials, tokens and unknown setting values protected.
            values.extend(v for key, v in config.items()
                          if isinstance(v, str) and len(v) > 6
                          and not (key == 'bar_input' and v in {'tracker', 'wt9011dcl'}))
    values.extend(os.environ.get(name, '') for name in ('OPENAI_API_KEY', 'CESIUM_ION_TOKEN'))
    return [v.casefold() for v in values if v]


def private_content(data, values):
    try:
        content = data.decode('utf-8')
    except UnicodeDecodeError:
        return True
    emails = re.findall(r'[A-Za-z0-9_.+-]+@([A-Za-z0-9.-]+\.[A-Za-z]{2,})', content)
    private_email = any(domain.lower() not in {'users.noreply.github.com',
                                               'example.com', 'example.org', 'example.net'}
                        for domain in emails)
    return bool(private_email or SECRET_PATTERN.search(content) or
                any(v in content.casefold() for v in values))


def forbidden_path(name):
    path = Path(name.lower())
    return (path.suffix in PRIVATE_SUFFIXES or '.sqlite' in path.name or
            bool(PRIVATE_DIRS.intersection(path.parts)) or '.local.' in path.name or
            path.name == '.env' or path.name.startswith('.env.'))


def audit(history=False):
    values, bad, entries = private_values(), set(), set()
    commits = git('rev-list', '--all').decode().splitlines() if history else []
    for commit in commits:
        meta = git('show', '-s', '--format=%an%x00%ae%x00%cn%x00%ce%x00%B', commit)
        fields = meta.decode('utf-8').split('\0', 4)
        public_email = r'[A-Za-z0-9+_.-]+@users\.noreply\.github\.com'
        if (private_content(meta, values) or not all(
                re.fullmatch(public_email, fields[i]) for i in (1, 3))):
            bad.add('commit metadata ' + commit[:12])
        for entry in git('ls-tree', '-r', '-z', commit).split(b'\0'):
            if not entry:
                continue
            record, name = entry.split(b'\t', 1)
            mode, kind, oid = record.decode().split()
            if kind != 'blob' or mode == '120000':
                bad.add(name.decode('utf-8'))
            else:
                entries.add((name.decode('utf-8'), oid))
    if not history:
        for entry in git('ls-files', '--stage', '-z').split(b'\0'):
            if not entry:
                continue
            record, name = entry.split(b'\t', 1)
            mode, oid, stage = record.decode().split()
            if mode in ('120000', '160000') or stage != '0':
                bad.add(name.decode('utf-8'))
            else:
                entries.add((name.decode('utf-8'), oid))
    checked = {}
    wheel_hashes = json.loads((ROOT/'apps/fly/wheels.lock.json').read_text(encoding='utf-8'))
    for name, oid in sorted(entries):
        if forbidden_path(name):
            bad.add(name)
            continue
        if name.startswith('apps/fly/wheels/') and name.endswith('.whl'):
            digest=hashlib.sha256(git('cat-file','blob',oid)).hexdigest()
            if wheel_hashes.get(Path(name).name)!=digest: bad.add(name)
            continue
        if oid not in checked:
            checked[oid] = private_content(git('cat-file', 'blob', oid), values)
        if checked[oid]:
            bad.add(name)
    if bad:
        raise SystemExit('Public tree check failed (values redacted): ' + ', '.join(sorted(bad)))
    scope = f'{len(commits)} commits across all refs' if history else 'entire index'
    print(f'Public tree check passed: {len(entries)} file versions, {scope}.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--history', action='store_true', help='Audit every branch/tag and commit metadata')
    audit(parser.parse_args().history)
