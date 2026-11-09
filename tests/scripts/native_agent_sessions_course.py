#!/usr/bin/env python3
"""Agent-tab native session composition, restart and offscreen rendering."""
import argparse
from pathlib import Path
import os
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--canary', required=True, type=Path)
    parser.add_argument('--evidence-dir', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='bpr-native-session-') as temporary:
        parent = Path(temporary).resolve()
        os.chmod(parent, 0o700)
        data = parent / 'data'
        for course in ('native-session', 'native-session-reopen', 'tour-native', 'tour-native-reopen'):
            result = subprocess.run([str(args.executable.resolve()), course, str(args.canary.resolve()), str(data)],
                                    text=True, capture_output=True, timeout=180)
            print(result.stdout, end='')
            print(result.stderr, end='')
            if result.returncode or f'agent-invocation-{course}:pass' not in result.stdout:
                return 1
        if args.evidence_dir:
            args.evidence_dir.mkdir(parents=True, exist_ok=True)
            for name in ('native-agent-tab.png', 'native-reopen.png', 'native-tour.png', 'native-tour-reopen.png'):
                shutil.copyfile(parent / name, args.evidence_dir / name)
    print('native-agent-sessions-course:pass')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
