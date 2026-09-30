"""Drive the shipped executable in separate processes against its safe GitHub fixture."""
import argparse
import os
from pathlib import Path
import sqlite3
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
# [[BPR-1231]]. The data root's descriptor, and the contained temporary root.
from bpr_narrow_owner_private import make_root_private  # noqa: E402
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--application', type=Path, required=True)
    parser.add_argument('--agent', type=Path, required=True)
    parser.add_argument('--representation', choices=('native', 'file'), required=True)
    parser.add_argument('--render', type=Path)
    parser.add_argument('--scale', choices=('1', '1.25', '2'), default='1')
    parser.add_argument('--theme', choices=('system', 'light', 'dark', 'high-contrast-light', 'high-contrast-dark'), default='system')
    parser.add_argument('--lose-first-result', action='store_true')
    args = parser.parse_args()
    with contained_temporary_directory(prefix='bpr-private-review-') as temporary:
        parent = Path(temporary).resolve()
        make_root_private(parent)
        root = parent / 'data'
        make_root_private(root)
        for phase in ('seed', 'compose', 'publish'):
            env = os.environ.copy()
            env.setdefault('QT_QPA_PLATFORM', 'offscreen')
            env['QT_SCALE_FACTOR'] = args.scale
            env['BPR_PRIVATE_REVIEW_THEME'] = args.theme
            if args.render:
                env['BPR_PRIVATE_REVIEW_RENDER'] = str(args.render.resolve().with_name(args.render.stem + '-' + phase + args.render.suffix))
            command = [str(args.application.resolve()), '--github-fixture', 'github-com', '--epoch-fixture-root', str(root), '--epoch-fixture-phase', 'single', '--private-review-course', phase, '--private-review-representation', args.representation, '--private-review-agent', str(args.agent.resolve())]
            if args.lose_first_result:
                command.append('--private-review-lose-first-result')
            result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=110)
            markers = [line for line in result.stdout.splitlines() if 'PRIVATE_REVIEW_' in line or 'AGENT_WORKSPACE_RECOVERY_REFUSED' in line]
            print('\n'.join(markers), flush=True)
            if result.returncode or f'PRIVATE_REVIEW_COURSE_OK phase={phase}' not in result.stdout or 'AGENT_WORKSPACE_RECOVERY_REFUSED' in result.stdout:
                raise RuntimeError(f'{phase} returned {result.returncode}:\n{result.stdout[-14000:]}')
            if phase == 'publish' and args.lose_first_result:
                assert 'submissions=2' in result.stdout, result.stdout[-14000:]
            with sqlite3.connect(f'file:{root / "state/v1/review-ledger.sqlite3"}?mode=ro', uri=True) as db:
                assert db.execute('SELECT count(*) FROM private_run').fetchone()[0] >= 1
                assert db.execute('SELECT count(*) FROM private_content WHERE kind=0').fetchone()[0] >= 3
                if phase != 'seed':
                    expected = b'BPR-0571 off-hunk publication.' if args.representation == 'file' else b'Synthetic reviewer-approved RFC-0023 comment.'
                    assert db.execute('SELECT count(*) FROM private_draft d JOIN private_content c ON d.scope_id=c.scope_id AND d.content_id=c.id WHERE c.body=?', (expected,)).fetchone()[0] == 1
                publications = db.execute('SELECT count(*) FROM private_publication WHERE server_id IS NOT NULL').fetchone()[0]
                assert publications == (1 if phase == 'publish' else 0), (phase, publications)
        print('PRIVATE_REVIEW_PUBLIC_PATH_OK', args.representation)


if __name__ == '__main__':
    main()
