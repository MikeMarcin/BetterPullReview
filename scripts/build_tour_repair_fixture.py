#!/usr/bin/env python3
"""Build the synthetic BPR-1979 three-commit pinned-source repair course."""
from __future__ import annotations

import json
import os
import subprocess
from pathlib import Path

from build_recorded_pull_request_pack import FIXTURE_ENVIRONMENT
from contained_temporary_support import contained_temporary_directory


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    target = root / 'tests/fixtures/github/pull-requests/tour-repairs-v1'
    environment = {**os.environ, **FIXTURE_ENVIRONMENT}
    with contained_temporary_directory() as repository:
        def git(*arguments: str) -> str:
            return subprocess.run(['git', '-C', repository, *arguments], check=True,
                                  capture_output=True, text=True, env=environment).stdout.strip()
        git('init', '-q', '--initial-branch=main')
        files = {name: [f'int {name[:-4]}_line_{line} = 0;\n' for line in range(1, 17)]
                 for name in ('review.cpp', 'secondary.cpp')}
        identities: dict[str, str] = {}
        for stage in ('base', 'historical', 'head'):
            if stage == 'historical':
                files['review.cpp'][5] = 'int review_line_6 = 1;\n'
            if stage == 'head':
                files['secondary.cpp'][5] = 'int secondary_line_6 = 1;\n'
            for name, lines in files.items():
                (Path(repository) / name).write_text(''.join(lines), encoding='utf-8', newline='\n')
            git('add', '-A')
            git('commit', '-q', '-m', f'Synthetic tour repair {stage}')
            identities[stage] = git('rev-parse', 'HEAD')
        assert git('show', identities['historical'] + ':review.cpp') == git('show', identities['head'] + ':review.cpp')
        identities['review_blob'] = git('rev-parse', 'HEAD:review.cpp')
        listing = git('rev-list', '--objects', identities['head'])
        packed = subprocess.run(['git', '-C', repository, 'pack-objects', '--stdout'],
                                input=(listing + '\n').encode(), env=environment,
                                check=True, capture_output=True).stdout
    target.mkdir(parents=True, exist_ok=True)
    (target / 'objects.pack').write_bytes(packed)
    (target / 'identities.json').write_text(json.dumps(identities, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(identities))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
