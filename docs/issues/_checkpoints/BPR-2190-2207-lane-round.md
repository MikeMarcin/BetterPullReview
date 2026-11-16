# Lane round checkpoint: BPR-2207, BPR-2206, BPR-2190, BPR-2194, BPR-2195

Lane `claude/v0-5-0-prerelease-targets-52265d`. The assignment: work these
five cards in order, fixing the real cause of each and repairing the course
that proves it. Other implementers share this tree (BPR-2152/2154 queue and
steer, BPR-2189 local-draft check), so each commit names its paths.

## State (second round, 2026-11-16)

| Card | State | Commits |
|---|---|---|
| BPR-2207 | Done | `b59d7c61d` |
| BPR-2206 | Done; the owner's look at the corner Dif is open in the session below | `2185e9ab3` |
| BPR-2190 | Done; the owner accepted AC03 | `0e5f2e1e1`, `984dd4696`, card `ac7fece50` |
| BPR-2194 | Done; pending states passes 10 of 10 | `18c55f938`, card `b165cca54` |
| BPR-2195 | Open; the refusing step is found: a cancelled `index-pack` leaves the cache unrepairable | `ade6b9a55`, `42a1fe963`, evidence `605556fc9` |

Attended session `1924669e55ed8a79ae40e9320a64c32b`, state
`~/.attended-review/bpr2190-2206-look-v001`, served on the tailnet (host pid
in its `serve.out`; the URL stays in that external state). Consumed cursor 8.
`BPR-2190-footer-v001` is accepted; `BPR-2206-setup-dif-v001` is pending.
Frozen roots: `/Users/jaeger/bpr2190-footer-v002.onzw9n3r` and
`/Users/jaeger/bpr2206-setup-dif-v002.__yhiamk`. Remove each with
`freeze_attended_build.py remove` once its question is answered.

## Next action

1. Read the session from cursor 8. Record the BPR-2206 verdict on its card;
   a Reject reopens the Dif placement.
2. BPR-2195: name the residue a stopped `index-pack` leaves, make the
   failed-index cleanup roll back the expected pack's partial artifacts, and
   hold that in `git_cache_contract` (fault-injection build). Then repeat
   pending states 10 times under build load.
