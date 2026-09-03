---
id: BPR-0540
title: Refresh material shares the base credential's erasure path, and the resume never spends an orphan
status: In Progress
rfc: RFC-BPR-0016
depends_on: [BPR-0536]
---

# Refresh material shares the base credential's erasure path, and the resume never spends an orphan

## Why

The second independent review of RFC-BPR-0016 at `0bd39349` ([[BPR-0285]],
2026-09-04) found two blocking defects in the refresh composition
[[BPR-0536]] delivered. No product code calls `delete_refresh`: the
orderly-shutdown policy deletes the base credential and leaves the refresh
token in the vault under the same authority, so a grant followed by a quit
orphans a six-month refresh token. On the next launch a pasted token for the
same account reaches the same authority key, the host's one-time probe finds
the orphan, composition takes the "renewable" branch, `resume_refresh`
spends the orphan at GitHub, and the resulting device token supersedes and
deletes the pasted-token session with nothing on screen. The fixture's
retained-secret check reads only the base slot, so the refresh course passes
with the refresh token left behind after the shutdown that deleted the
access token. No session survives a restart today, so the resume from slot
presence is reachable only through that wrong case.

## Required final state

- Wherever the base credential is exact-deleted under an authority —
  orderly shutdown, sign-out, a superseding enrollment from a manual source —
  the refresh slot is deleted under the same authority through the same
  vault port and completion discipline; host tests cover each site.
- The resume answers true only for a credential whose provenance is a
  device grant this host enrolled or resumed; a pasted token never resumes,
  even with material in the slot.
- The fixture reports the refresh slot (`DEVICE_FLOW_REFRESH_RETAINED`,
  `DEVICE_FLOW_REFRESH_STORES`); the refresh and registered courses pin
  `REFRESH_RETAINED:false` after their orderly shutdown; and an orphan
  course — refresh slot pre-seeded, a pasted credential connected — proves
  `REFRESH_RESUMES:0` and the pasted session intact, failing before the fix.
- The ledger's refresh row, the RFC's token-handling disposition, and the
  three documents describe the erasure and the provenance rule.

## Non-goals

- Persisting a session across a restart ([[BPR-0541]]).

## Lane — 2026-09-04

Claimed straight into `30-in-progress/` by the integrator under the
transition exception, its dependency terminal, as the blocking fix the
review named. The slice runs on `bpr-0540-work` in a worktree; the
documents and the ledger row are the integrator's at close.
