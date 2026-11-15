#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Added 2026-11-15 for the three dependencies documented by AirPort_RTW89.
"""Download pinned upstream ZIPs. Does not extract, install, modify EFI or reboot."""
import argparse
import hashlib
import json
import tempfile
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def verify(path, entry):
    if path.stat().st_size != entry['size'] or sha(path) != entry['sha256']:
        raise RuntimeError('Size/SHA256 mismatch: ' + str(path))
    with zipfile.ZipFile(path) as archive:
        if archive.testzip() is not None:
            raise RuntimeError('ZIP CRC check failed: ' + str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path,
                        default=Path.home() / 'Downloads/RTLAirLink-Dependencies')
    args = parser.parse_args()
    lock = json.loads((ROOT / 'DEPENDENCIES.json').read_text())
    args.out.mkdir(parents=True, exist_ok=True)
    for entry in lock['dependencies']:
        name = entry['filename']
        if Path(name).name != name or not name.endswith('.zip'):
            raise RuntimeError('Invalid dependency filename')
        expected_prefix = ('https://raw.githubusercontent.com/' +
                           lock['provider_repository'] + '/' + lock['provider_commit'] + '/')
        if not entry['url'].startswith(expected_prefix):
            raise RuntimeError('Dependency URL is outside the pinned provider')
        destination = args.out / name
        if destination.exists():
            verify(destination, entry)
            print('Already verified: ' + name, flush=True)
            continue
        print('Downloading ' + name, flush=True)
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=args.out, prefix=name + '.',
                                             suffix='.part', delete=False) as stream:
                temporary = Path(stream.name)
                received = 0
                with urllib.request.urlopen(entry['url'], timeout=60) as response:
                    while True:
                        chunk = response.read(1024 * 1024)
                        if not chunk:
                            break
                        received += len(chunk)
                        if received > entry['size']:
                            raise RuntimeError('Download exceeds pinned size')
                        stream.write(chunk)
            verify(temporary, entry)
            temporary.replace(destination)
            temporary = None
            print('SHA256/ZIP verified: ' + name, flush=True)
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
    print('Saved to: ' + str(args.out))
    print('Unzip these three dependencies and follow DEPENDENCIES.md/INSTALL.md.')
    print('No EFI or configuration changes were made.')


if __name__ == '__main__':
    main()
