#!/usr/bin/env python3
"""Generate deterministic synthetic PR Search objects and served contracts."""
from __future__ import annotations
import base64
import hashlib
import json
import subprocess
import sys
from pathlib import Path
import build_nested_folders_fixture as nested
import build_recorded_pull_request_pack as pack
from contained_temporary_support import contained_temporary_directory


def make(large: bool = False) -> tuple[dict, bytes, str]:
    count, rows = (64, 2000) if large else (2, 30)
    sources: list[tuple[str, str | None, bytes | None, bytes | None]] = []
    for index in range(count):
        common = ''.join(f'// fixture {index:03d} full unchanged source row {line:04d}\n' for line in range(rows))
        common += f'catalogue token {index:03d}\n'
        sources.append((f'bulk/{index:03d}.cpp', None, ('earlier\n'+common).encode(), ('later\n'+common).encode()))
    sources += [
        ('src/added.cpp', None, None, b'added catalogue token\n'),
        ('src/removed.cpp', None, b'removed catalogue token\n', None),
        ('src/renamed.cpp', 'old/name.cpp', b'renamed catalogue token\n', b'renamed catalogue token\n'),
        ('historical/only.cpp', None, None, b'historical-only token\n'),
        ('unsupported/binary.bin', None, None, b'catalogue token\x00binary'),
        ('unsupported/invalid.cpp', None, None, b'catalogue token\xff'),
    ]
    files=[]
    for name, previous, before, after in sources:
        patch, added, removed = nested.patch(None if before is None else before.decode('utf-8',errors='replace'), '' if after is None else after.decode('utf-8',errors='replace'))
        status='renamed' if previous else 'added' if before is None else 'removed' if after is None else 'modified'
        content=after if after is not None else before
        entry={'filename':name,'status':status,'sha':hashlib.sha1(b'blob %d\0'%len(content)+content).hexdigest(),'additions':added,'deletions':removed,'changes':added+removed,'patch':patch}
        if previous: entry['previous_filename']=previous
        for side, value in [('base',before),('head',after)]:
            if value is not None: entry[f'{side}_content_base64']=base64.b64encode(value).decode()
        files.append(entry)
    files.sort(key=lambda entry:entry['filename'])
    result=nested.construction()
    result['files']=files
    result['constructed']='Synthetic RFC-BPR-0077 exact PR Search course; no remote data'
    result['repository']={'owner':'bpr-fixtures','name':'source-pr-search-large' if large else 'source-pr-search'}
    result['pull_request'].update(number=78, title='Search full retained source', body='Synthetic full-file historical search course.\n', changed_files=len(files),additions=sum(f['additions'] for f in files),deletions=sum(f['deletions'] for f in files))
    with contained_temporary_directory() as repository:
        run=lambda *args:pack.run(repository,*args)
        run('init','-q','--initial-branch=main')
        root=Path(repository)
        def populate(side: int):
            for name,previous,before,after in sources:
                for path in {name,previous} - {None}:
                    target=root/path
                    if target.exists():target.unlink()
            for name,previous,before,after in sources:
                value=before if side==0 else after
                if value is not None:
                    target=root/(previous if side==0 and previous else name);target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(value)
        populate(0);run('add','-A');run('commit','-q','-m','base');base=run('rev-parse','HEAD').stdout.strip()
        populate(1);run('add','-A');run('commit','-q','-m','head');head=run('rev-parse','HEAD').stdout.strip()
        (root/'historical/only.cpp').unlink();(root/'today').mkdir();(root/'today/only.cpp').write_text('today-only token\n')
        run('add','-A');run('commit','-q','-m','later head');second=run('rev-parse','HEAD').stdout.strip()
        names=run('diff','--name-status','-M',base,head).stdout
        if 'R100\told/name.cpp\tsrc/renamed.cpp' not in names:raise RuntimeError('exact rename absent')
        for name,previous,before,after in sources:
            for revision,path,value in [(base,previous or name,before),(head,name,after)]:
                probe=subprocess.run(['git','-C',repository,'show',f'{revision}:{path}'],capture_output=True)
                if value is None:
                    if probe.returncode==0:raise RuntimeError('absent source exists')
                elif probe.returncode or probe.stdout!=value:raise RuntimeError('source mismatch')
        listing=run('rev-list','--objects',second)
        blob=subprocess.run(['git','-C',repository,'pack-objects','--stdout'],input=listing.stdout.encode(),capture_output=True,env={**__import__('os').environ,**pack.FIXTURE_ENVIRONMENT},check=True).stdout
    result['pull_request']['base']['sha']=base;result['pull_request']['head']['sha']=head
    result['commits'][0].update(sha=head,parents=[base],message='Search full retained source')
    result['search_fixture']={'head_two':second,'files':len(files),'source_rows_per_bulk_side':rows,'bulk_files':count,'source_bytes':sum(len(v) for _,_,b,h in sources for v in (b,h) if v is not None)}
    return result,blob,second


def main() -> int:
    large='--large' in sys.argv
    name='source-pr-search-large-v1' if large else 'source-pr-search-v1'
    root=Path(__file__).resolve().parents[1]
    result,blob,second=make(large)
    directory=root/'tests/fixtures/github/pull-requests'/name;directory.mkdir(parents=True,exist_ok=True)
    recording=directory/'construction.json';recording.write_text(json.dumps(result,indent='\t',sort_keys=True)+'\n')
    (directory/'objects.pack').write_bytes(blob)
    subprocess.run([sys.executable,str(root/'scripts/build_recorded_pull_request_contract.py'),str(recording),str(root/'tests/fixtures/github/contracts/v1'/name)],check=True)
    print(f"SOURCE_SEARCH_FIXTURE_OK {name} base={result['pull_request']['base']['sha']} head={result['pull_request']['head']['sha']} second={second} pack={len(blob)} bytes={result['search_fixture']['source_bytes']}")
    return 0
if __name__=='__main__':raise SystemExit(main())
