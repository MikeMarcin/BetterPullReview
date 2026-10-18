# Stacked merge-base fixture

Synthetic public-path course for [[BPR-1572]]. No remote data or credentials.
`scripts/build_stacked_merge_base_fixture.py` builds a stacked pull request:
the fork point `M`, the recorded base `A1` (`base.sha`, the base branch's own
change), and the head's two commits `B1` and `B2`. The base branch has
fast-forwarded to `B1`, so compare against the branch names `B1` as merge base,
while GitHub's Files changed (measured on github.com, 2026-10-17) still
compares from the recorded base: `A1...B2`, `x.txt` and `y.txt`. A comparison
from `B1` would show `x.txt` with the pull request's text on both sides. The
pack holds every object reachable from `B2`; the recording lists both of the
head's commits (`packed_history`). The fixture server answers the branch with
`B1` and the recorded base with `A1`, against the head and against `B1`, and
with `ahead_by` on each. The committed artifacts are consumed directly;
ordinary builds regenerate nothing.
