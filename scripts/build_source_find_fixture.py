#!/usr/bin/env python3
"""Construct the committed, installed-Git pane Find course. No remote data."""
from __future__ import annotations
import json
import subprocess
import sys
from pathlib import Path
import build_nested_folders_fixture as nested
import build_recorded_pull_request_pack as pack
from contained_temporary_support import contained_temporary_directory


def construction() -> dict:
    context = ''.join(f'// unchanged source row {line:04d}\n' for line in range(1100))
    context += 'offscreen needle\nneedle again\n𝄞 café Σσς\r\nStraße K𐐀\r\nliteral a+b(c)[d]?\n'
    nested.FILES = (
        ('src/find.cpp', 'earlier only\nremoved bridge\n'+context, 'later only\nadded bridge\n'+context),
        ('src/other.cpp', 'old\n', 'new\n'),
    )
    result = nested.construction()
    result['constructed'] = 'Synthetic RFC-BPR-0077 source Find course; no remote data'
    result['repository'] = {'name':'source-find','owner':'bpr-fixtures'}
    result['pull_request'].update(number=77,title='Find complete source text',body='Synthetic full-source Find course.\n')
    with contained_temporary_directory() as repository:
        base, head = pack.build(result,repository)
        failures = pack.verify(result,repository,base,head)
        if failures:
            raise RuntimeError(failures)
    result['pull_request']['base']['sha'] = base
    result['pull_request']['head']['sha'] = head
    result['commits'][0].update(sha=head,parents=[base],message='Find complete source text')
    return result


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    recording = Path(sys.argv[1]) if len(sys.argv) > 1 else root/'tests/fixtures/github/pull-requests/source-find-v1/construction.json'
    recording.parent.mkdir(parents=True,exist_ok=True)
    recording.write_text(json.dumps(construction(),indent='\t',sort_keys=True)+'\n',encoding='utf-8',newline='\n')
    if len(sys.argv) > 1:
        return 0
    subprocess.run([sys.executable,str(root/'scripts/build_recorded_pull_request_pack.py'),str(recording),str(recording.parent/'objects.pack')],check=True)
    subprocess.run([sys.executable,str(root/'scripts/build_recorded_pull_request_contract.py'),str(recording),str(root/'tests/fixtures/github/contracts/v1/source-find-v1')],check=True)
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
