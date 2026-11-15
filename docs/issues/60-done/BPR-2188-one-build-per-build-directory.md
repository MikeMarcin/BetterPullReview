---
schema: "implement-rfc.card.v1"
id: "BPR-2188"
title: "Build one build directory at a time"
status: "Done"
rfcs: []
depends_on: ["BPR-2186"]
priority: "P1"
platforms: ["mac", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2188--BPR-2188--b2821b106781.json","kind":"tooling","follow_up_to":"BPR-2186","found_by":"coordinator: two implementers ran Ninja in one build/dev at once and hit 'malformed archive', the core and Qt adapter libraries rewritten mid-link","landed_in":"6c48283d0: the three files were staged when another agent committed the shared index (its BPR-2181 commit); the content is this card's change, unaltered"}
acceptance: [{"criterion":"AC1: builds of one build directory run one after another, on the joined and the fallback path","outcome":"pass","revision":"6c48283d0","evidence":{"kind":"command","command":"scratch: two jobserver_ninja.py builds of one Ninja tree started 0.3 s apart (o1-o6, o7-o12, one-second jobs), server down and server up with 4 tokens; real: two concurrent `cmake --build --preset dev` (bpr_fixture_agent, bpr_update_helper) in build/dev with the 12-token server","detail":"Scratch, down: o1-6 0.00-1.02 s, o7-12 1.04-2.07 s; up: 0.00-2.06 s then 2.09-4.15 s; each second build printed 'waiting for the build already running in <dir> (pid N, o1 o2 o3 o4 o5 o6)'. Real: both preset builds printed the waiting line (queued behind two other agents' builds), dev#1's Ninja ended at 39.89 s and dev#2's began at 39.96 s; both exit 0. Live, earlier: another agent's wrapper (pid 20191) sat with no Ninja child while .bpr-build.lock named pid 19284 and its three targets."}},{"criterion":"AC2: builds of different trees overlap and share the pool","outcome":"pass","revision":"6c48283d0","evidence":{"kind":"command","command":"scratch trees a and b concurrently (down and up); a scratch CMake tree configured with the wrapper built during the real preset run above","detail":"Down: a 0.00-1.02 s, b 0.31-1.33 s; up (4 tokens): a 0.00-2.08 s, b 1.06-3.12 s, b waiting for a token in between. Real: the other tree waited for a token from the full 12-token pool and built at 5.1-5.3 s while build/dev was still held."}},{"criterion":"AC3: a waiting build holds no token, and the lock releases on a crash","outcome":"pass","revision":"6c48283d0","evidence":{"kind":"command","command":"scratch, 4 tokens: SIGKILL a joined wrapper and its Ninja mid-build, then build the same tree","detail":"The lock is taken before any token. After the SIGKILL, status read 0 free; the next build printed the waiting line until the killed build's orphaned jobs (which inherit the lock and are still writing outputs) ended, then waited for a token; the server logged 'idle with 0 of 4 tokens: restored 4 leaked' and the build exited 0 after 5.4 s. Before this card the waiter's own client lease held off that refill forever; the lease now follows the token."}},{"criterion":"AC4: gates","outcome":"pass","revision":"6c48283d0","evidence":{"kind":"command","command":"python3 scripts/verify_documentation.py --root .","detail":"DOCUMENTATION_OK after this card's counter sync."}}]
---

# One build per build directory

Two agents running Ninja in the same `build/dev` corrupted static archives
("malformed archive"; the core and Qt adapter libraries rewritten mid-link).
BPR-2186's jobserver bounds jobs across trees but not builds within one.

## What landed

- `scripts/jobserver_ninja.py` takes an exclusive `flock` on
  `<build dir>/.bpr-build.lock` before anything else, on the joined and the
  fallback path, and hands the locked descriptor to Ninja, so the lock lives
  as long as the build and the kernel drops it on any exit. The holder writes
  its pid and targets into the file; a waiter prints one line naming them.
- A build waits for the lock holding no token, and takes its client lease only
  after its token, so a pool that a killed build drained is refilled even
  while builds wait.
- No lock for `-t` tools (CMake runs `-t restat` and `-t recompact` while
  regenerating inside a locked build), `--version`, `-n`, `-h`, a Ninja nested
  in a jobserver job, or a Ninja inside a build already holding this
  directory's lock (`BPR_BUILD_LOCK_HELD`).
- AGENTS.md "Build jobserver" says one build per tree.

## Limits

- A bare `ninja` in the tree takes no lock and can still race a wrapped build.
- After a SIGKILL the killed build's still-running compiler jobs hold the lock
  until they exit; they are still writing its outputs, so that is deliberate.
