#!/usr/bin/env python3
"""Forward repository allocation to the canonical board model."""
import argparse
from pathlib import Path
import subprocess
import sys

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
p.add_argument('--count',type=int,default=1)
p.add_argument('--reserve-for',default='local-main')
p.add_argument('--purpose',default='')
p.add_argument('--dry-run',action='store_true')
p.add_argument('--json',action='store_true')
a=p.parse_args();root=a.root.resolve()
command=[sys.executable,str(root/'.agents/skills/implement-rfc/scripts/issue_board.py'),'--repo-root',str(root),'allocate','--count',str(a.count),'--reserve-for',a.reserve_for,'--purpose',a.purpose]
if a.dry_run:command.append('--dry-run')
if a.json:command.append('--json')
sys.exit(subprocess.call(command))
